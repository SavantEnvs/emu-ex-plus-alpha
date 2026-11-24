// GBC.emu: load an untrusted save state. Mirrors GbcSystem::readState
// (GBC.emu/src/main/Main.cc), which wraps the state bytes in an istream and calls
// gambatte::GB::loadState(std::istream&), then keeps emulating from that state.
// A fixed, valid ROM (built below) is loaded first, as a state is only applied to a
// running game.
#include "gbc_common.h"
#include <sstream>
#include <vector>

static mayhem_gbc::Buffers bufs;

// 32 KiB MBC1+RAM+BATTERY cartridge with 8 KiB RAM whose program enables cart RAM,
// writes two bytes to it and spins.
static std::vector<uint8_t> makeRom() {
	std::vector<uint8_t> rom(0x8000, 0);
	const uint8_t entry[] = {0x00, 0xC3, 0x50, 0x01};
	memcpy(&rom[0x100], entry, sizeof entry);
	memcpy(&rom[0x134], "MAYHEMSTATE", 11);
	rom[0x147] = 0x03; rom[0x148] = 0x00; rom[0x149] = 0x02;
	const uint8_t prog[] = {0x3E, 0x0A, 0xEA, 0x00, 0x00, 0x3E, 0x42, 0xEA, 0x00, 0xA0,
		0x3E, 0x99, 0xEA, 0x01, 0xA0, 0x18, 0xFE};
	memcpy(&rom[0x150], prog, sizeof prog);
	uint8_t x = 0;
	for(int i = 0x134; i <= 0x14C; i++) x = x - rom[i] - 1;
	rom[0x14D] = x;
	return rom;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	static const std::vector<uint8_t> rom = makeRom();
	gambatte::GB gb;
	mayhem_gbc::NoInput input;
	gb.setInputGetter(&input);
	if(gb.load(rom.data(), rom.size(), "state.gbc", 0) != gambatte::LOADRES_OK)
		abort(); // the fixed ROM must always load
	std::istringstream stream(std::string(reinterpret_cast<const char *>(data), size), std::ios::binary);
	if(!gb.loadState(stream))
		return 0;
	mayhem_gbc::runFrames(gb, bufs, 4);
	return 0;
}
