// Snes9x.emu: load an untrusted SNES/Super Famicom ROM image and emulate it. Mirrors
// Snes9xSystem::loadContent + runFrame (Snes9x/src/main/Main.cc): the content is read into a
// buffer, rejected if larger than CMemory::MAX_ROM_SIZE + 512, and passed to
// Memory.LoadROMMem (copier-header removal, LoROM/HiROM/ExHiROM detection, special chips
// SA-1, SuperFX, SPC7110, S-DD1, DSP, C4, BS-X, ...), then S9xMainLoop runs frames.
// The app's Sufami Turbo path needs a user-supplied BIOS, which isn't configured by default,
// so it is not taken here. Core state persists between inputs exactly as it does when the
// app loads one game after another: LoadROMMem re-initialises the cartridge mapping and calls
// S9xReset (CPU, PPU, APU, DMA), and settings are reset in prepareLoad/setupInput.
#include "snes9x_common.h"
#include <exception>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if(size == 0 || size > CMemory::MAX_ROM_SIZE + 512)
		return -1; // Snes9xSystem::loadContent: "ROM is too large"; an empty read fails
	mayhem_snes::initOnce();
	mayhem_snes::prepareLoad();
	try {
		if(!Memory.LoadROMMem(data, size, "fuzz.sfc"))
			return 0;
	} catch(const std::exception &) {
		return 0; // e.g. S9xReadBSXBios: no BS-X BIOS set
	}
	mayhem_snes::setupInput();
	mayhem_snes::runFrames(4);
	return 0;
}
