// Stand-ins for the Lynx.emu app / EmuFramework driver hooks the Mednafen Lynx core calls.
// The real ones (Lynx.emu/src/main/options.cc, EmuFramework/src/shared/mednafen-emuex/
// MDFNApi.cc, StreamImpl.cc) are C++-module code that needs Clang 21, so they are reproduced
// here with the app's default behavior:
//  - settings: cheats off, lynx.lowpass off (app default), untrusted_fip_check off;
//  - firmware path: empty (no BIOS configured) -> the core's HLE boot ROM;
//  - logging / notices / video commit: no-ops;
//  - cheats (mempatcher.cpp, whose cheat-file code needs the app VFS): no cheats are ever
//    loaded here, so the cheat-engine hooks the core calls are no-ops;
//  - state save/load (MDFNSS_StateAction) is not reachable from this harness.
#include <mednafen/mednafen.h>
#include <mednafen/FileStream.h>
#include <mednafen/MemoryStream.h>
#include <mednafen/state.h>
#include <mednafen/general.h>
#include <mednafen/mempatcher.h>
#include <mednafen/video/convert.h>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace Mednafen {

MDFNGI *MDFNGameInfo{};

void MDFN_printf(const char *, ...) noexcept {}
void MDFN_Notify(MDFN_NoticeType, const char *, ...) noexcept {}
void MDFND_OutputNotice(MDFN_NoticeType, const char *) noexcept {}
void MDFND_commitVideoFrame(EmulateSpecStruct *) {}
void MDFN_indent(int) {}

bool MDFN_GetSettingB(const char *name_) {
	std::string_view name{name_};
	if(name == "cheats" || name == "lynx.lowpass" || name == "filesys.untrusted_fip_check")
		return false;
	abort(); // the app treats any other setting as unreachable
}
int64 MDFN_GetSettingI(const char *name_) {
	if(std::string_view{name_} == "filesys.state_comp_level")
		return 6;
	abort();
}

std::string MDFN_MakeFName(MakeFName_Type type, int, const char *) {
	if(type == MDFNMKF_FIRMWARE)
		return {}; // LynxSystem::biosPath default: empty -> HLE BIOS
	abort();
}

// Only reached with a BIOS path, which the harness never sets.
FileStream::FileStream(const std::string &, const uint32, const int, const uint32) {
	throw std::runtime_error("FileStream not available in the fuzz build");
}
FileStream::~FileStream() {}
uint64 FileStream::attributes() { return 0; }
uint8 *FileStream::map() noexcept { return nullptr; }
uint64 FileStream::map_size() noexcept { return 0; }
void FileStream::unmap() noexcept {}
uint64 FileStream::read(void *, uint64, bool) { abort(); }
uint64 FileStream::readAtPos(void *, uint64, uint64) { abort(); }
void FileStream::write(const void *, uint64) { abort(); }
void FileStream::truncate(uint64) { abort(); }
void FileStream::seek(int64, int) { abort(); }
void FileStream::flush() {}
uint64 FileStream::tell() { abort(); }
uint64 FileStream::size() { abort(); }
void FileStream::close() {}
void FileStream::advise(off_t, size_t, IG::IOAdvice) {}
int FileStream::get_char() { abort(); }

// StreamImpl.cc
uint64 MemoryStream::readAtPos(void *data, uint64 count, uint64 pos) {
	auto prevPos = tell();
	seek(pos, SEEK_SET);
	auto bytes = read(data, count, false);
	seek(prevPos, SEEK_SET);
	return bytes;
}
void MemoryStream::setSize(size_t size) {
	if(size > data_buffer_alloced) abort();
	data_buffer_size = size;
}
uint64 Stream::readAtPos(void *, uint64, uint64) { abort(); }
void Stream::advise(off_t, size_t, IG::IOAdvice) {}

void MDFNMP_Init(uint32, uint32) {}
void MDFNMP_AddRAM(uint32, uint32, uint8 *, bool) {}
void MDFNMP_ApplyPeriodicCheats() {}

// Only used by MDFN_Surface::SetFormat/DupeCompactConvert, which the Lynx path never calls.
MDFN_PixelFormatConverter::MDFN_PixelFormatConverter(const MDFN_PixelFormat &, const MDFN_PixelFormat &, const MDFN_PaletteEntry *) {
	abort();
}

bool MDFNSS_StateAction(StateMem *, const unsigned, const bool, const SFORMAT *, const char *, const bool) noexcept {
	abort();
}

}
