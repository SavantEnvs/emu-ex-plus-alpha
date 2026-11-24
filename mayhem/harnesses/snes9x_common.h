// Shared pieces of the Snes9x harnesses and self-test (see snes9x_glue.cc).
#pragma once
#include <snes9x.h>
#include <memmap.h>
#include <display.h>
#include <snapshot.h>
#include <cheats.h>
#include <controls.h>
#include <gfx.h>
#include <apu/bapu/snes/snes.hpp>
#include <apu/apu.h>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mayhem_snes {
extern int lastWidth, lastHeight, framesRendered;
extern int16_t lastAudio[1800];
extern int samplesMixed;
void initOnce();
void prepareLoad();
void setupInput();
void runFrames(int frames);

// 128 KiB LoROM whose reset handler writes to WRAM, turns the screen on and spins.
// The header-size byte is 7 at every candidate header offset (0x7FD7, 0xFFD7, +0x200), so
// the ROM-scoring code never takes the negative-shift path build.sh describes for memmap.cpp.
inline std::vector<uint8_t> makeRom(uint8_t region = 0x01) {
	std::vector<uint8_t> rom(0x20000, 0);
	rom[0x81D7] = rom[0xFFD7] = rom[0x101D7] = 0x07;
	memcpy(&rom[0x7FC0], "MAYHEM STATE         ", 21);
	rom[0x7FD5] = 0x20; // LoROM, slow
	rom[0x7FD6] = 0x00; // ROM only
	rom[0x7FD7] = 0x07; // 128 KiB
	rom[0x7FD9] = region; // 0x01 North America, 0x02 Europe (PAL)
	// reset vector -> $8000
	rom[0x7FFC] = 0x00; rom[0x7FFD] = 0x80;
	const uint8_t prog[] = {
		0x78, 0x18, 0xFB,             // sei; clc; xce
		0xA9, 0x42, 0x8D, 0x00, 0x00, // lda #$42; sta $0000
		0xA9, 0x0F, 0x8D, 0x00, 0x21, // lda #$0f; sta $2100
		0x80, 0xFE,                   // bra *
	};
	memcpy(&rom[0], prog, sizeof prog);
	uint16_t sum = 0;
	rom[0x7FDC] = 0xFF; rom[0x7FDD] = 0xFF; rom[0x7FDE] = 0; rom[0x7FDF] = 0;
	for(auto b : rom) sum += b;
	rom[0x7FDE] = sum & 0xFF; rom[0x7FDF] = sum >> 8;
	rom[0x7FDC] = ~sum & 0xFF; rom[0x7FDD] = (~sum >> 8) & 0xFF;
	return rom;
}
}

