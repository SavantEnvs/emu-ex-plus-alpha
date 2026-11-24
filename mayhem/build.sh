#!/usr/bin/env bash
#
# mayhem/build.sh — build the emu-ex-plus-alpha fuzz harnesses and functional self-tests.
#
# Upstream builds each emulator app with CMake >= 4.1 + C++26 named modules on top of the
# imagine/EmuFramework libraries and needs Clang 21 / GCC 16, which the base image (clang-19)
# does not have. The emulator cores themselves are plain C++ though, so this script compiles
# the cores that GBC.emu and GBA.emu feed untrusted files to, from the same source lists the
# app CMakeLists use, and links them into the harnesses:
#   - libgambatte (GBC.emu/src/libgambatte, list from GBC.emu/src/CMakeLists.txt)
#   - VBA-M patchers (GBA.emu/src/core/base/patch.cpp)
#   - Mednafen Lynx (Lynx.emu/src/lynx, list from Lynx.emu/src/CMakeLists.txt) plus the
#     shared Mednafen sources it uses (EmuFramework/src/shared/mednafen); the app-side hooks
#     it calls are in mayhem/harnesses/lynx_glue.cc. Linked with --gc-sections, as unused
#     shared-Mednafen code references framework symbols that only the full app provides.
#   - Snes9x (Snes9x/src/snes9x, list from Snes9x/src/CMakeLists.txt); app hooks in
#     mayhem/harnesses/snes9x_glue.cc; also linked with --gc-sections.
# Objects go to mayhem/out/ (gitignored); binaries to /mayhem/<target>(-standalone).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
OUT="$SRC/mayhem/out"
mkdir -p "$OUT"

GBC=GBC.emu/src
GAMBATTE_SRCS=(
	libgambatte/src/cpu.cpp libgambatte/src/gambatte.cpp libgambatte/src/initstate.cpp
	libgambatte/src/interrupter.cpp libgambatte/src/tima.cpp libgambatte/src/memory.cpp
	libgambatte/src/mem/rtc.cpp libgambatte/src/sound.cpp libgambatte/src/statesaver.cpp
	libgambatte/src/video.cpp libgambatte/src/sound/channel1.cpp libgambatte/src/sound/channel2.cpp
	libgambatte/src/sound/channel3.cpp libgambatte/src/sound/channel4.cpp
	libgambatte/src/sound/duty_unit.cpp libgambatte/src/sound/envelope_unit.cpp
	libgambatte/src/sound/length_counter.cpp libgambatte/src/video/ly_counter.cpp
	libgambatte/src/video/lyc_irq.cpp libgambatte/src/video/next_m0_time.cpp
	libgambatte/src/video/ppu.cpp libgambatte/src/video/sprite_mapper.cpp
	libgambatte/src/mem/cartridge.cpp libgambatte/src/mem/memptrs.cpp
	libgambatte/src/interruptrequester.cpp libgambatte/src/mem/pakinfo.cpp
	libgambatte/src/loadres.cpp
)
# GBC.emu/CMakeLists.txt: HAVE_STDINT_H GAMBATTE_NO_OSD; the core is C++ with a C++20
# header from imagine (DelegateFunc.hh).
GAMBATTE_FLAGS=(-std=c++23 -DHAVE_STDINT_H -DGAMBATTE_NO_OSD -I"$SRC/$GBC/libgambatte/include"
	-I"$SRC/$GBC/libgambatte/src" -I"$SRC/$GBC/common" -I"$SRC/imagine/include")
GBA_FLAGS=(-std=c++23 -I"$SRC/GBA.emu/src")
LYNX_SRCS=(
	Lynx.emu/src/lynx/c65c02.cpp Lynx.emu/src/lynx/cart.cpp Lynx.emu/src/lynx/lynxdec.cpp
	Lynx.emu/src/lynx/memmap.cpp Lynx.emu/src/lynx/mikie.cpp Lynx.emu/src/lynx/ram.cpp
	Lynx.emu/src/lynx/rom.cpp Lynx.emu/src/lynx/susie.cpp Lynx.emu/src/lynx/system.cpp
)
MDFN=EmuFramework/src/shared/mednafen
MDFN_SRCS=(
	$MDFN/MemoryStream.cpp $MDFN/Stream.cpp $MDFN/error.cpp
	$MDFN/hash/md5.cpp $MDFN/sound/Blip_Buffer.cpp $MDFN/sound/Stereo_Buffer.cpp
	$MDFN/endian.cpp $MDFN/git.cpp $MDFN/string/string.cpp $MDFN/video/surface.cpp
)
# imagine's generated config header (normally written by imagine's CMake build); the Mednafen
# headers only need it to exist.
mkdir -p "$OUT/inc"
[ -f "$OUT/inc/imagine-config.h" ] || echo '#define IMAGINE_VERSION_BASE "0"' > "$OUT/inc/imagine-config.h"
# EmuFramework/CMakeLists.txt (mednafen_common): HAVE_CONFIG_H + these include dirs.
E="$SRC/EmuFramework"
LYNX_FLAGS=(-std=c++23 -DHAVE_CONFIG_H -ffunction-sections -fdata-sections -I"$OUT/inc"
	-I"$E/include/shared/mednafen" -I"$E/include/shared" -I"$E/include" -I"$E/src/shared"
	-I"$SRC/imagine/include" -I"$SRC/Lynx.emu/src" -Wno-missing-field-initializers
	-Wno-unused-parameter -Wno-unused-function)

# Snes9x core: the source list of Snes9x/src/CMakeLists.txt minus the app's main/*.cc (module
# code; the hooks they provide are in mayhem/harnesses/snes9x_glue.cc) and minus bml.cpp, the
# only core file that imports a C++ module (`import imagine` for its file reader). It is only
# used by the BML cheat-file and MSU-1 manifest readers, which the app reaches through file
# lookups that find nothing here (snes9x_glue.cc gives bml_node a parse_file that finds no
# file, as the app's does for content without a cheat file). The app list also omits
# fscompat.cpp, which defines the splitpath() that CMemory::FileLoader (the path-based loader
# the app never calls) references; snes9x_glue.cc carries a copy of upstream's POSIX
# splitpath/ext_is so the core links. Defines/options from Snes9x/CMakeLists.txt.
S9X=Snes9x/src
SNES_SRCS=($(sed -n '/snes9x\/bsx.cpp/,/smp_state.cpp/p' "$SRC/$S9X/CMakeLists.txt" | grep -v 'bml\.cpp'))
[ "${#SNES_SRCS[@]}" -ge 40 ] || { echo "build.sh: Snes9x source list not found" >&2; exit 1; }
SNES_FLAGS=(-std=c++23 -DHAVE_STRINGS_H -DHAVE_STDINT_H -DRIGHTSHIFT_IS_SAR -DZLIB
	-Wno-register -Wno-implicit-fallthrough -ffunction-sections -fdata-sections
	-I"$SRC/$S9X/snes9x" -I"$SRC/$S9X/snes9x/apu/bapu" -I"$SRC/$S9X")

# compile_lib <name> <flags-string> -> $OUT/<name>/libgambatte.a, libvbampatch.a
compile_lib() {
	local name="$1" flags="$2" d="$OUT/$1"
	mkdir -p "$d"
	local objs=() pids=()
	for s in "${GAMBATTE_SRCS[@]}"; do
		local o="$d/gb_$(echo "$s" | tr '/' '_').o"
		# shellcheck disable=SC2086
		$CXX $flags "${GAMBATTE_FLAGS[@]}" -c "$SRC/$GBC/$s" -o "$o" &
		pids+=($!); objs+=("$o")
		if [ "${#pids[@]}" -ge "$MAYHEM_JOBS" ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
	done
	for p in "${pids[@]}"; do wait "$p"; done
	# shellcheck disable=SC2086
	$CXX $flags "${GAMBATTE_FLAGS[@]}" -c "$SRC/mayhem/harnesses/gbc_glue.cc" -o "$d/gbc_glue.o"
	rm -f "$d/libgambatte.a"; ar rcs "$d/libgambatte.a" "${objs[@]}" "$d/gbc_glue.o"
	# shellcheck disable=SC2086
	$CXX $flags "${GBA_FLAGS[@]}" -c "$SRC/GBA.emu/src/core/base/patch.cpp" -o "$d/patch.o"
	rm -f "$d/libvbampatch.a"; ar rcs "$d/libvbampatch.a" "$d/patch.o"
	objs=(); pids=()
	for s in "${LYNX_SRCS[@]}" "${MDFN_SRCS[@]}" mayhem/harnesses/lynx_glue.cc; do
		local o="$d/lx_$(echo "$s" | tr '/' '_').o"
		# shellcheck disable=SC2086
		$CXX $flags "${LYNX_FLAGS[@]}" -c "$SRC/$s" -o "$o" &
		pids+=($!); objs+=("$o")
		if [ "${#pids[@]}" -ge "$MAYHEM_JOBS" ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
	done
	for p in "${pids[@]}"; do wait "$p"; done
	rm -f "$d/liblynx.a"; ar rcs "$d/liblynx.a" "${objs[@]}"
	objs=(); pids=()
	for s in "${SNES_SRCS[@]/#/$S9X/}" mayhem/harnesses/snes9x_glue.cc; do
		local o="$d/s9x_$(echo "$s" | tr '/' '_').o" extra=()
		# memmap.cpp: CMemory::ScoreHiROM/ScoreLoROM compute `1 << (buf[0xd7] - 7)` on the
		# header-size byte at all four candidate header offsets before knowing which one is
		# real, and InitROM does `(1 << (ROMSize - 7)) * 128` / `(1 << (SRAMSize + 3)) * 128`.
		# Any ROM with a byte < 7 at one of those offsets, which includes every ROM under
		# 66 KiB (the rest of ROM[] is zero), is a negative shift (then a signed overflow),
		# so with halting UBSan nearly every input stops there before the mapper code. The
		# results only feed the header-score heuristic and a UI colour. Only these two UBSan
		# checks, and only in memmap.cpp, are relaxed; ASan and the rest of UBSan stay on and
		# halting, and every other Snes9x file keeps them.
		[ "${s##*/}" = memmap.cpp ] && extra=(-fno-sanitize=shift-exponent,signed-integer-overflow)
		# shellcheck disable=SC2086
		$CXX $flags "${SNES_FLAGS[@]}" "${extra[@]}" -c "$SRC/$s" -o "$o" &
		pids+=($!); objs+=("$o")
		if [ "${#pids[@]}" -ge "$MAYHEM_JOBS" ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
	done
	for p in "${pids[@]}"; do wait "$p"; done
	rm -f "$d/libsnes9x.a"; ar rcs "$d/libsnes9x.a" "${objs[@]}"
}

# 1) Sanitized + instrumented cores (fuzz build).
FUZZ_FLAGS="-O1 $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
compile_lib fuzz "$FUZZ_FLAGS"

# 2) Harnesses: libFuzzer binary + standalone reproducer each.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OUT/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"
build_harness() {
	local name="$1" lib="$2"; shift 2
	$CXX -O1 $SANITIZER_FLAGS $DEBUG_FLAGS "$@" -c "$SRC/mayhem/harnesses/$name.cc" -o "$OUT/$name.o"
	$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/$name.o" "$OUT/lsan_off.o" \
		"$OUT/fuzz/$lib" -Wl,--gc-sections -lz -lm -o "$SRC/$name"
	$CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/$name.o" "$OUT/standalone_main.o" "$OUT/lsan_off.o" \
		"$OUT/fuzz/$lib" -Wl,--gc-sections -lz -lm -o "$SRC/$name-standalone"
}
build_harness gbc_rom libgambatte.a "${GAMBATTE_FLAGS[@]}"
build_harness gbc_state libgambatte.a "${GAMBATTE_FLAGS[@]}"
build_harness gba_patch libvbampatch.a "${GBA_FLAGS[@]}"
build_harness lynx_rom liblynx.a "${LYNX_FLAGS[@]}"
build_harness snes_rom libsnes9x.a "${SNES_FLAGS[@]}"
build_harness snes_state libsnes9x.a "${SNES_FLAGS[@]}"

# 3) Functional self-tests with the project's normal flags (no sanitizers), for test.sh.
compile_lib test "-O2 $COVERAGE_FLAGS"
$CXX -O2 $COVERAGE_FLAGS "${GAMBATTE_FLAGS[@]}" "$SRC/mayhem/tests/gbc_selftest.cc" \
	"$OUT/test/libgambatte.a" -o "$OUT/gbc_selftest"
$CXX -O2 $COVERAGE_FLAGS "${GBA_FLAGS[@]}" "$SRC/mayhem/tests/gba_patch_selftest.cc" \
	"$OUT/test/libvbampatch.a" -lz -o "$OUT/gba_patch_selftest"
$CXX -O2 $COVERAGE_FLAGS "${LYNX_FLAGS[@]}" "$SRC/mayhem/tests/lynx_selftest.cc" \
	"$OUT/test/liblynx.a" -Wl,--gc-sections -o "$OUT/lynx_selftest"
$CXX -O2 $COVERAGE_FLAGS "${SNES_FLAGS[@]}" "$SRC/mayhem/tests/snes9x_selftest.cc" \
	"$OUT/test/libsnes9x.a" -Wl,--gc-sections -lz -o "$OUT/snes9x_selftest"
echo "build.sh: done"
