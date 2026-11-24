// GBA.emu: apply an untrusted ROM patch (IPS / UPS / PPF) to the loaded ROM.
// Mirrors GbaSystem::applyGamePatches (GBA.emu/src/main/Main.cc): the app opens the
// patch file found next to the game as a FILE* and calls patchApplyIPS / patchApplyUPS /
// patchApplyPPF (GBA.emu/src/core/base/patch.cpp) on gGba.mem.rom, a SIZE_ROM (32 MiB)
// buffer holding a ROM of `romSize` bytes. The app picks the patcher by which file exists
// (.ips, then .ups, then .ppf); here the first input byte makes that choice and the rest is
// the patch file's content.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "core/base/patch.h"

// Defined in patch.cpp; GBA.emu's Main.cc calls them with the patch FILE*.
bool patchApplyIPS(FILE *f, uint8_t **rom, int *size);
bool patchApplyUPS(FILE *f, uint8_t **rom, int *size);
bool patchApplyPPF(FILE *f, uint8_t **rom, int *size);

static constexpr int kSizeRom = 0x2000000; // SIZE_ROM, core/gba/gba.h
static constexpr int kRomSize = 0x40000;   // a 256 KiB game

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if(size < 1)
		return -1;
	uint8_t sel = data[0] % 3;
	data++; size--;
	uint8_t *rom = static_cast<uint8_t *>(calloc(kSizeRom, 1));
	if(!rom)
		abort();
	for(int i = 0; i < kRomSize; i++)
		rom[i] = static_cast<uint8_t>(i * 7 + (i >> 8));
	int romSize = kRomSize;
	// fmemopen needs a non-empty buffer; an empty patch file is an empty stream.
	static char empty[1];
	FILE *f = size ? fmemopen(const_cast<uint8_t *>(data), size, "rb") : fmemopen(empty, 0, "rb");
	if(f) {
		// The patchApply* functions close the FILE.
		switch(sel) {
			case 0: patchApplyIPS(f, &rom, &romSize); break;
			case 1: patchApplyUPS(f, &rom, &romSize); break;
			default: patchApplyPPF(f, &rom, &romSize); break;
		}
	}
	free(rom);
	return 0;
}
