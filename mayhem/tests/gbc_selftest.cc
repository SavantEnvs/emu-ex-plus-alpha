// Known-answer tests for libgambatte as GBC.emu uses it (load / run / sram / save state).
// Prints "PASS <name>" or "FAIL <name>" per check; mayhem/test.sh counts them.
// With an argument, also writes the saved state of the test ROM there (seed generation).
#include <gambatte.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int fails = 0;
static void check(bool ok, const char *name) {
	printf("%s %s\n", ok ? "PASS" : "FAIL", name);
	if(!ok) fails++;
}

class NoInput : public gambatte::InputGetter {
public:
	unsigned operator()() override { return 0; }
};

static gambatte::uint_least32_t video[160 * 144];
static gambatte::uint_least32_t audio[35112 + 2064];

static std::vector<uint8_t> makeRom(uint8_t cgb) {
	std::vector<uint8_t> rom(0x8000, 0);
	const uint8_t entry[] = {0x00, 0xC3, 0x50, 0x01};
	memcpy(&rom[0x100], entry, sizeof entry);
	memcpy(&rom[0x134], "MAYHEMTEST", 10);
	rom[0x143] = cgb;
	rom[0x147] = 0x03; rom[0x148] = 0x00; rom[0x149] = 0x02;
	const uint8_t prog[] = {0x3E, 0x0A, 0xEA, 0x00, 0x00, 0x3E, 0x42, 0xEA, 0x00, 0xA0,
		0x3E, 0x99, 0xEA, 0x01, 0xA0, 0x18, 0xFE};
	memcpy(&rom[0x150], prog, sizeof prog);
	uint8_t x = 0;
	for(int i = 0x134; i <= 0x14C; i++) x = x - rom[i] - 1;
	rom[0x14D] = x;
	return rom;
}

static int run(gambatte::GB &gb, int calls) {
	int frames = 0;
	for(int i = 0; i < calls; i++) {
		std::size_t samples = 35112;
		if(gb.runFor(video, 160, audio, samples, gambatte::VideoFrameDelegate{}) != -1)
			frames++;
	}
	return frames;
}

int main(int argc, char **argv) {
	NoInput input;
	{
		gambatte::GB gb;
		std::vector<uint8_t> tiny(0x100, 0);
		check(gb.load(tiny.data(), tiny.size(), "tiny.gb") != gambatte::LOADRES_OK, "rejects_truncated_rom");
		check(!gb.isLoaded(), "not_loaded_after_reject");
	}
	{
		gambatte::GB gb;
		auto rom = makeRom(0x00);
		rom[0x147] = 0xFC; // Pocket Camera: unsupported
		check(gb.load(rom.data(), rom.size(), "cam.gb") == gambatte::LOADRES_UNSUPPORTED_MBC_POCKET_CAMERA, "rejects_pocket_camera");
	}
	std::string state;
	{
		gambatte::GB gb;
		gb.setInputGetter(&input);
		auto rom = makeRom(0x00);
		check(gb.load(rom.data(), rom.size(), "dmg.gb") == gambatte::LOADRES_OK, "loads_dmg_rom");
		check(gb.isLoaded(), "is_loaded");
		check(gb.romTitle() == "MAYHEMTEST", "rom_title");
		check(!gb.isCgb(), "dmg_is_not_cgb");
		check(gb.srambank().size() == 0x2000, "sram_8k");
		int frames = run(gb, 10);
		check(frames >= 3, "produces_frames");
		auto s = gb.srambank();
		check(s.size() >= 2 && s[0] == 0x42 && s[1] == 0x99, "cpu_wrote_cart_ram");
		std::ostringstream os(std::ios::binary);
		check(gb.saveState(video, 160, os), "save_state");
		state = os.str();
		check(state.size() > 1000, "state_nonempty");
		if(argc > 1) { std::ofstream(argv[1], std::ios::binary) << state; }
	}
	{
		gambatte::GB gb;
		gb.setInputGetter(&input);
		auto rom = makeRom(0x00);
		gb.load(rom.data(), rom.size(), "dmg.gb");
		auto s = gb.srambank();
		check(s[0] != 0x42, "fresh_sram_clear");
		std::istringstream is(state, std::ios::binary);
		check(gb.loadState(is), "load_state");
		s = gb.srambank();
		check(s[0] == 0x42 && s[1] == 0x99, "state_restores_sram");
		std::istringstream bad(std::string("garbage"), std::ios::binary);
		check(!gb.loadState(bad), "rejects_bad_state");
	}
	{
		gambatte::GB gb;
		gb.setInputGetter(&input);
		auto rom = makeRom(0x80);
		check(gb.load(rom.data(), rom.size(), "cgb.gbc") == gambatte::LOADRES_OK, "loads_cgb_rom");
		check(gb.isCgb(), "cgb_flag_detected");
		run(gb, 10);
		auto s = gb.srambank();
		check(s[0] == 0x42 && s[1] == 0x99, "cgb_cpu_wrote_cart_ram");
	}
	return fails ? 1 : 0;
}
