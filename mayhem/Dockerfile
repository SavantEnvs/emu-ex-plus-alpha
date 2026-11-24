# syntax=docker/dockerfile:1.7
#
# Commit image for emu-ex-plus-alpha (savantenvs). Builds the GBC.emu (libgambatte) and GBA.emu
# (VBA-M patcher), Lynx.emu (Mednafen Lynx) and Snes9x.emu (Snes9x) cores into libFuzzer harnesses; see mayhem/build.sh for why the cores are built
# directly rather than through upstream's CMake/C++-modules app build.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# zlib (crc32 for the UPS patcher) ships in the base image (zlib1g-dev).

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
