// Stand-ins for the Snes9x.emu app hooks the Snes9x core calls. The real ones
// (Snes9x/src/main/S9XApi.cc, Main.cc) are C++-module code that needs Clang 21, so they are
// reproduced here with the app's default behavior:
//  - logging (S9xMessage/S9xPrintf) is off, as in a release build with logging disabled;
//  - no BS-X BIOS is configured (Snes9xSystem::bsxBiosPath empty) -> S9xReadBSXBios throws,
//    exactly like the app;
//  - files next to the content (SRAM/.rtc, IPS/UPS patches, MSU-1 tracks, cheats) do not
//    exist: fopenHelper/gzopenHelper return null, like opening a missing URI in the app;
//  - bml_node (bml.cpp imports the imagine module for its file reader): parse_file opens the
//    content's .cht/.bml cheat file through the app VFS, which doesn't exist for fuzzed
//    content, so it returns false like the app's; the in-memory tree helpers are copied;
//  - S9xDeinitUpdate clears the Z buffers like the app and records the rendered size; the
//    frame is not presented anywhere.
#include "snes9x_common.h"
#include <bml.h>
#include <fscompat.h>
#include <strings.h>
#include <cstdarg>
#include <stdexcept>
#include <zlib.h>

uint16 SSettings::DisplayColor{};
uint32 SSettings::SkipFrames{};
uint32 SSettings::TurboSkipFrames{};
bool8 SSettings::StopEmulation{};
std::string SGFX::InfoString;
uint32 SGFX::InfoStringTimeout{};
char SGFX::FrameDisplayString[256]{};

// fscompat.cpp (not in the app's source list): POSIX splitpath() and SplitPath::ext_is, copied.
bool SplitPath::ext_is(const std::string &other)
{
	if(strcasecmp(ext.c_str(), other.c_str()) == 0)
		return true;
	if(other[0] != '.' && (strcasecmp(other.c_str(), &(ext.c_str()[1])) == 0))
		return true;
	return false;
}

SplitPath splitpath(std::string path)
{
	constexpr auto npos = std::string::npos;
	SplitPath output{};
	auto slash = path.find_last_of("/\\");
	auto dot = path.rfind('.');
	if(dot != npos && slash != npos && dot < slash)
		dot = npos;
	else if(dot != npos)
		output.ext = path.substr(dot);
	if(slash != npos)
	{
		output.dir = path.substr(0, slash + 1);
		output.stem = path.substr(slash + 1, dot - slash - 1);
	}
	else
		output.stem = path.substr(0, dot);
	return output;
}

bml_node::bml_node()
{
	type = CHILD;
	depth = -1;
}

bool bml_node::parse_file(const std::string &) { return false; }

bml_node *bml_node::find_subnode(const std::string &name)
{
	for(auto &i : child)
		if(name == i.name)
			return &i;
	return nullptr;
}

namespace mayhem_snes {
int lastWidth, lastHeight, framesRendered;
}

void S9xMessage(int, int, const char *) {}
void S9xPrintf(const char *, ...) {}
void S9xPrintfError(const char *, ...) {}
void S9xHandlePortCommand(s9xcommand_t, int16, int16) {}
bool8 S9xOpenSoundDevice() { return TRUE; }
const char *S9xGetCrosshair(int) { return nullptr; }

// Content-relative paths: the app maps these to its save/cheat/patch directories. Nothing
// exists there for fuzzed content, so any name works; fopenHelper never finds a file.
std::string S9xGetFilenameInc(std::string_view, enum s9x_getdirtype) { return {}; }
std::string S9xGetFilename(std::string_view ex, enum s9x_getdirtype) { return std::string{"fuzz"} + std::string{ex}; }
std::string S9xGetFilename(std::string_view, std::string_view ex, enum s9x_getdirtype dirtype) { return S9xGetFilename(ex, dirtype); }
std::string S9xGetFullFilename(std::string_view name, enum s9x_getdirtype) { return std::string{name}; }
std::string S9xBasename(std::string_view f)
{
	auto p = f.find_last_of("/\\");
	return std::string{p == std::string_view::npos ? f : f.substr(p + 1)};
}

void S9xReadBSXBios(uint8 *)
{
	throw std::runtime_error{"No BS-X BIOS set"};
}

bool S9xPollAxis(uint32, int16 *) { return false; }
bool S9xPollPointer(uint32, int16 *, int16 *) { return false; }
void S9xExit() {}
void S9xToggleSoundChannel(int) {}
const char *S9xStringInput(const char *) { return nullptr; }

bool8 S9xOpenSnapshotFile(const char *, bool8, STREAM *) { return FALSE; }
void S9xCloseSnapshotFile(STREAM file) { CLOSE_STREAM(file); }
FILE *fopenHelper(const char *, const char *) { return nullptr; }
void removeFileHelper(const char *) {}
gzFile gzopenHelper(const char *, const char *) { return nullptr; }

bool8 S9xDoScreenshot(int, int) { return 1; }
void S9xSyncSpeed() {}
bool8 S9xInitUpdate() { return 1; }
void notifyBackupMemoryWritten() {}

bool8 S9xDeinitUpdate(int width, int height)
{
	mayhem_snes::lastWidth = width;
	mayhem_snes::lastHeight = height;
	mayhem_snes::framesRendered++;
	memset(GFX.ZBuffer, 0, GFX.ScreenSize);
	memset(GFX.SubZBuffer, 0, GFX.ScreenSize);
	return true;
}

bool8 S9xContinueUpdate(int width, int height) { return S9xDeinitUpdate(width, height); }

namespace mayhem_snes {

// Snes9xSystem constructor (Snes9x/src/main/system.ccm).
void initOnce()
{
	static bool done = false;
	if(done)
		return;
	done = true;
	Memory.Init();
	S9xGraphicsInit();
	S9xInitAPU();
	S9xInitSound(0);
	S9xUnmapAllControls();
	S9xCheatsEnable();
}

// Start of Snes9xSystem::loadContent, with the default video-system option (auto).
void prepareLoad()
{
	memset(Memory.NSRTHeader, 0, sizeof(Memory.NSRTHeader));
	Memory.HeaderCount = 0;
	Settings.ForceNTSC = false;
	Settings.ForcePAL = false;
}

// Snes9xSystem::setupSNESInput with the default joypad setup (no multitap).
void setupInput()
{
	S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
	S9xSetController(1, CTL_JOYPAD, 1, 0, 0, 0);
}

int16_t lastAudio[1800];
int samplesMixed;

// Snes9xSystem::runFrame with video and audio enabled.
void runFrames(int frames)
{
	for(int i = 0; i < frames; i++)
	{
		IPPU.RenderThisFrame = TRUE;
		S9xSetSamplesAvailableCallback([](void *)
			{
				int samples = S9xGetSampleCount();
				if(!samples)
					return;
				int16_t audioBuff[1800]; // same stack buffer as the app's mixSamples, unclamped
				S9xMixSamples((uint8 *)audioBuff, samples);
				memcpy(lastAudio, audioBuff, sizeof(audioBuff));
				samplesMixed += samples;
			}, nullptr);
		S9xMainLoop();
	}
}

}
