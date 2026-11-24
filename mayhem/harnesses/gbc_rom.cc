// GBC.emu: load an untrusted Game Boy / Game Boy Color ROM image and emulate it.
// Mirrors GbcSystem::loadContent (GBC.emu/src/main/Main.cc): the whole file is handed to
// gambatte::GB::load(data, size, name, flags), then runFrame() drives GB::runFor().
// Exercises cartridge header parsing / MBC selection (mem/cartridge.cpp), the SM83 CPU,
// PPU, APU, timers and RTC.
#include "gbc_common.h"

static mayhem_gbc::Buffers bufs;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	gambatte::GB gb;
	mayhem_gbc::NoInput input;
	gb.setInputGetter(&input);
	// GBC.emu's "report as GBA" option maps to the GBA_CGB flag; take it from the input
	// length so both modes are reachable without altering the ROM bytes.
	unsigned flags = (size & 1) ? gambatte::GB::GBA_CGB : 0;
	if(gb.load(data, size, "fuzz.gbc", flags) != gambatte::LOADRES_OK)
		return 0;
	mayhem_gbc::runFrames(gb, bufs, 8);
	return 0;
}
