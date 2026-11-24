// The two hooks GBC.emu's app layer supplies to libgambatte (GBC.emu/src/main/Main.cc,
// which needs the C++-module framework and so is not compiled here), reproduced verbatim
// in behavior: save data is handled by the app, so the core's load/save hooks are no-ops,
// and gbcToRgb32 converts a CGB BGR15 colour to RGBA8888 (default, non-saturated mode).
#include <cstdint>
#include <gbint.h>

extern "C++" {
gambatte::uint_least32_t gbcToRgb32(unsigned const bgr15, unsigned flags) {
	unsigned r = bgr15 & 0x1F;
	unsigned g = bgr15 >> 5 & 0x1F;
	unsigned b = bgr15 >> 10 & 0x1F;
	unsigned outR, outG, outB;
	if(flags & 1) { // COLOR_CONVERSION_SATURATED_BIT
		outR = (r * 255 + 15) / 31;
		outG = (g * 255 + 15) / 31;
		outB = (b * 255 + 15) / 31;
	} else {
		outR = (r * 13 + g * 2 + b) >> 1;
		outG = (g * 3 + b) << 1;
		outB = (r * 3 + g * 2 + b * 11) >> 1;
	}
	return (outR & 0xFF) | (outG & 0xFF) << 8 | (outB & 0xFF) << 16;
}
}

namespace gambatte {
class Cartridge {
public:
	void loadSavedata();
	void saveSavedata();
};
void Cartridge::loadSavedata() {}
void Cartridge::saveSavedata() {}
}
