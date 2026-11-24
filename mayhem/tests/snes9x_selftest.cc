// Known-answer tests for the Snes9x core as Snes9x.emu uses it (load / run / save state).
// Prints "PASS <name>" or "FAIL <name>" per check; mayhem/test.sh counts them.
// With arguments, also writes the saved state of the test ROM to argv[1] and the test ROM to
// argv[2] (seed generation).
#include "../harnesses/snes9x_common.h"
#include <cstdio>
#include <fstream>

static int fails = 0;
static void check(bool ok, const char *name) {
	printf("%s %s\n", ok ? "PASS" : "FAIL", name);
	if(!ok) fails++;
}

static bool load(const std::vector<uint8_t> &rom) {
	mayhem_snes::prepareLoad();
	if(!Memory.LoadROMMem(rom.data(), rom.size(), "test.sfc"))
		return false;
	mayhem_snes::setupInput();
	return true;
}

int main(int argc, char **argv) {
	using namespace mayhem_snes;
	initOnce();
	auto rom = makeRom();
	check(load(rom), "load_lorom");
	check(strncmp(Memory.ROMName, "MAYHEM STATE", 12) == 0, "rom_name");
	check(Memory.LoROM && !Memory.HiROM, "lorom_detected");
	check(Memory.CalculatedChecksum == (uint32)(rom[0x7FDE] | rom[0x7FDF] << 8), "checksum");
	check(!Settings.PAL, "ntsc_region");
	framesRendered = 0; samplesMixed = 0;
	runFrames(2);
	check(Memory.RAM[0] == 0x42, "cpu_wrote_wram");
	check(PPU.Brightness == 0x0F && !PPU.ForcedBlanking, "ppu_inidisp");
	check(framesRendered == 2 && lastWidth == SNES_WIDTH && lastHeight == SNES_HEIGHT, "frame_size");
	check(samplesMixed > 0, "audio_mixed");

	// Save-state round trip (Snes9xSystem::writeState / readState, uncompressed).
	std::vector<uint8_t> state(S9xFreezeSize());
	check(S9xFreezeGameMem(state.data(), state.size()) && memcmp(state.data(), "#!s9xsnp", 8) == 0,
		"freeze_header");
	Memory.RAM[0] = 0;
	check(S9xUnfreezeGameMem(state.data(), state.size()) == SUCCESS && Memory.RAM[0] == 0x42,
		"unfreeze_restores_wram");
	std::vector<uint8_t> bad(state);
	memcpy(bad.data(), "#!s9xsnX", 8);
	check(S9xUnfreezeGameMem(bad.data(), bad.size()) != SUCCESS, "unfreeze_rejects_bad_magic");
	if(argc > 1)
		std::ofstream(argv[1], std::ios::binary).write((const char *)state.data(), state.size());
	if(argc > 2)
		std::ofstream(argv[2], std::ios::binary).write((const char *)rom.data(), rom.size());

	// 512-byte copier header is stripped.
	std::vector<uint8_t> hdr(512, 0);
	hdr.insert(hdr.end(), rom.begin(), rom.end());
	check(load(hdr) && Memory.HeaderCount == 1 && strncmp(Memory.ROMName, "MAYHEM STATE", 12) == 0,
		"copier_header_removed");
	// Europe region -> PAL timing.
	check(load(makeRom(0x02)) && Settings.PAL, "pal_region");
	return fails ? 1 : 0;
}
