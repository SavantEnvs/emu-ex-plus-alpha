// Lynx.emu: load an untrusted Atari Lynx cartridge image (.lnx / BS93 homebrew / raw) and
// emulate it. Mirrors LynxSystem::loadContent + runFrame (Lynx.emu/src/main/Main.cc):
// EmuEx::loadContent (EmuFramework/src/shared/mednafen-emuex/MDFNUtils.hh) copies the file
// into a MemoryStream, wraps it in a GameFile and calls the Lynx module's Load(); runFrame
// calls Emulate() with a 160x102 surface. With no BIOS path configured (the app's default
// biosPath), the core uses its HLE boot ROM. Exercises cart.cpp header parsing, the 65C02
// core, Mikie and the Susie sprite engine.
#include <mednafen/mednafen.h>
#include <mednafen/MemoryStream.h>
#include <mednafen/video/surface.h>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>

extern const Mednafen::MDFNGI EmulatedLynx; // Lynx.emu/src/lynx/system.cpp
void Lynx_SetPixelFormat(Mednafen::MDFN_PixelFormat format);

using namespace Mednafen;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	static constexpr size_t maxRomSize = 0x1000000; // LynxSystem::loadContent
	if(size == 0 || size > maxRomSize)
		return -1; // EmuEx::loadContent fails on an empty read and reads at most maxRomSize
	// The app's per-system game info (Load() writes rotation/MD5 into it); reset per input.
	static MDFNGI gi = EmulatedLynx;
	gi = EmulatedLynx;
	MDFNGameInfo = &gi;
	auto stream = std::make_unique<MemoryStream>(size, true);
	memcpy(stream->map(), data, size);
	stream->setSize(size);
	GameFile gf{nullptr, std::string{}, std::string{}, stream.get(), std::string{".lnx"}, std::string{"fuzz"}};
	try {
		gi.Load(&gf);
	} catch(const std::exception &) {
		return 0;
	}
	static uint8_t inputBuf[8];
	gi.SetInput(0, "gamepad", inputBuf);
	MDFN_PixelFormat fmt{MDFN_PixelFormat::RGB16_565};
	static uint16_t pixels[160 * 102];
	MDFN_Surface surface{pixels, 160, 102, 160, fmt, false};
	Lynx_SetPixelFormat(fmt);
	static int16_t audio[2400 * 2];
	for(int frame = 0; frame < 6; frame++) {
		EmulateSpecStruct espec{};
		espec.SoundBuf = audio;
		espec.SoundBufMaxSize = 2400;
		espec.surface = &surface;
		espec.skip = false;
		gi.Emulate(&espec);
	}
	gi.CloseGame();
	return 0;
}
