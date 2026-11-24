// Snes9x.emu: load an untrusted save state. Mirrors Snes9xSystem::readState
// (Snes9x/src/main/Main.cc) for an uncompressed state: S9xUnfreezeGameMem(buff, size)
// (snapshot.cpp, the "#!s9xsnp" block format), then emulation continues from it.
// A state is only applied to a running game, so a fixed, valid LoROM built below is loaded
// first (Memory.LoadROMMem, which also S9xResets the machine for every input).
// The app also accepts gzip-compressed states (EmuSystem::uncompressGzipState, framework
// code); this target feeds S9xUnfreezeGameMem directly.
#include "snes9x_common.h"
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	static const std::vector<uint8_t> rom = mayhem_snes::makeRom();
	mayhem_snes::initOnce();
	mayhem_snes::prepareLoad();
	if(!Memory.LoadROMMem(rom.data(), rom.size(), "state.sfc"))
		abort(); // the fixed ROM must always load
	mayhem_snes::setupInput();
	if(S9xUnfreezeGameMem(data, size) != SUCCESS)
		return 0; // readState: "Invalid state data"
	IPPU.RenderThisFrame = TRUE;
	mayhem_snes::runFrames(4);
	return 0;
}
