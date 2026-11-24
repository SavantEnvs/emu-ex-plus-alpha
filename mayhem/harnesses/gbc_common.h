// Shared helpers for the GBC.emu (libgambatte) harnesses.
#pragma once
#include <gambatte.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace mayhem_gbc {

// GBC.emu's frame loop (GBC.emu/src/main/Main.cc runFrame) passes a 160x144 frame buffer and
// an audio buffer of at least samples + 2064 entries.
struct Buffers {
	gambatte::uint_least32_t video[160 * 144];
	gambatte::uint_least32_t audio[35112 + 2064];
};

class NoInput : public gambatte::InputGetter {
public:
	unsigned operator()() override { return 0; }
};

// Run up to `calls` runFor() slices (each at most one video frame / 35112 samples).
inline void runFrames(gambatte::GB &gb, Buffers &b, int calls) {
	for(int i = 0; i < calls; i++) {
		std::size_t samples = 35112;
		gb.runFor(b.video, 160, b.audio, samples, gambatte::VideoFrameDelegate{});
	}
}

} // namespace mayhem_gbc
