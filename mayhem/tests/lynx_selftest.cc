// Known-answer tests for the Mednafen Lynx core as Lynx.emu drives it (load + emulate).
// Prints "PASS <name>" or "FAIL <name>" per check; mayhem/test.sh counts them.
#include <mednafen/mednafen.h>
#include <mednafen/MemoryStream.h>
#include <mednafen/video/surface.h>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

extern const Mednafen::MDFNGI EmulatedLynx;
void Lynx_SetPixelFormat(Mednafen::MDFN_PixelFormat format);
int Lynx_HCount();
using namespace Mednafen;

static int fails = 0;
static void check(bool ok, const char *name) {
	printf("%s %s\n", ok ? "PASS" : "FAIL", name);
	if(!ok) fails++;
}

static MDFNGI gi = EmulatedLynx;

static bool load(const std::vector<uint8_t> &file) {
	gi = EmulatedLynx;
	MDFNGameInfo = &gi;
	auto stream = std::make_unique<MemoryStream>(file.size(), true);
	memcpy(stream->map(), file.data(), file.size());
	stream->setSize(file.size());
	GameFile gf{nullptr, std::string{}, std::string{}, stream.get(), std::string{".lnx"}, std::string{"t"}};
	try { gi.Load(&gf); } catch(const std::exception &) { return false; }
	return true;
}

static std::vector<uint8_t> romData() {
	std::vector<uint8_t> rom(0x20000);
	for(size_t i = 0; i < rom.size(); i++) rom[i] = uint8_t(i * 13 + 7);
	rom[0] = 0xFF;
	return rom;
}

static std::vector<uint8_t> lnx(uint8_t rotation) {
	std::vector<uint8_t> f(64, 0);
	memcpy(&f[0], "LYNX", 4);
	f[4] = 0x00; f[5] = 0x02; // bank0 page size 512
	f[8] = 1;
	memcpy(&f[10], "MAYHEM SEED", 11);
	f[58] = rotation;
	auto rom = romData();
	f.insert(f.end(), rom.begin(), rom.end());
	return f;
}

static std::string hex(const uint8_t *p, int n) {
	std::string s; char b[3];
	for(int i = 0; i < n; i++) { snprintf(b, sizeof b, "%02x", p[i]); s += b; }
	return s;
}

int main() {
	check(!load(std::vector<uint8_t>(1000, 0x11)), "rejects_unknown_format");
	check(load(lnx(0)), "loads_lnx");
	check(hex(gi.MD5, 16) == "c4b80c8d2207bd6767742084ace1688d", "lnx_rom_md5");
	check(gi.rotated == MDFN_ROTATE0, "lnx_no_rotation");
	check(Lynx_HCount() == 159, "default_hcount");
	{
		static uint8_t input[8];
		gi.SetInput(0, "gamepad", input);
		MDFN_PixelFormat fmt{MDFN_PixelFormat::RGB16_565};
		static uint16_t pixels[160 * 102];
		memset(pixels, 0xAB, sizeof pixels);
		MDFN_Surface surface{pixels, 160, 102, 160, fmt, false};
		Lynx_SetPixelFormat(fmt);
		static int16_t audio[2400 * 2];
		int32 sound = 0;
		for(int i = 0; i < 4; i++) {
			EmulateSpecStruct espec{};
			espec.SoundBuf = audio;
			espec.SoundBufMaxSize = 2400;
			espec.surface = &surface;
			gi.Emulate(&espec);
			sound += espec.SoundBufSize;
		}
		check(pixels[0] != 0xABAB && pixels[160 * 101] != 0xABAB, "frame_written");
		check(sound > 0, "audio_produced");
	}
	gi.CloseGame();
	check(load(lnx(1)), "loads_rotated_lnx");
	check(gi.rotated == MDFN_ROTATE270, "lnx_rotate_left");
	gi.CloseGame();
	check(load(romData()), "loads_raw_128k");
	check(hex(gi.MD5, 16) == "c4b80c8d2207bd6767742084ace1688d", "raw_rom_md5");
	gi.CloseGame();
	return fails ? 1 : 0;
}
