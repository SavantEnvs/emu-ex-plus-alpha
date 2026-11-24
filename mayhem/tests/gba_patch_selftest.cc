// Known-answer tests for GBA.emu's IPS / UPS patchers (GBA.emu/src/core/base/patch.cpp).
// Prints "PASS <name>" or "FAIL <name>" per check; mayhem/test.sh counts them.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>

bool patchApplyIPS(FILE *f, uint8_t **rom, int *size);
bool patchApplyUPS(FILE *f, uint8_t **rom, int *size);

static int fails = 0;
static void check(bool ok, const char *name) {
	printf("%s %s\n", ok ? "PASS" : "FAIL", name);
	if(!ok) fails++;
}

static FILE *mem(std::string &s) { return fmemopen(s.data(), s.size(), "rb"); }

static void var(std::string &o, uint64_t x) {
	for(;;) {
		uint8_t b = x & 0x7F; x >>= 7;
		if(!x) { o += char(b | 0x80); break; }
		o += char(b); x--;
	}
}
static void le32(std::string &o, uint32_t v) { for(int i = 0; i < 4; i++) o += char((v >> (8 * i)) & 0xFF); }

int main() {
	const int bufSize = 0x10000, romSize = 0x1000;
	std::vector<uint8_t> buf(bufSize);
	auto reset = [&] { for(int i = 0; i < bufSize; i++) buf[i] = uint8_t(i); };
	{
		reset();
		std::string p = std::string("PATCH") + std::string("\x00\x00\x10\x00\x03" "ABC", 8)
			+ std::string("\x00\x00\x20\x00\x00\x00\x04Z", 8) + "EOF";
		uint8_t *rom = buf.data(); int size = romSize;
		check(patchApplyIPS(mem(p), &rom, &size), "ips_applies");
		check(memcmp(&buf[0x10], "ABC", 3) == 0, "ips_record");
		check(buf[0x20] == 'Z' && buf[0x23] == 'Z' && buf[0x24] == 0x24, "ips_rle_record");
		check(buf[0x0F] == 0x0F && buf[0x13] == 0x13, "ips_leaves_rest");
		check(size == romSize, "ips_size_unchanged");
	}
	{
		reset();
		std::string p = "NOTAPATCH";
		uint8_t *rom = buf.data(); int size = romSize;
		check(!patchApplyIPS(mem(p), &rom, &size), "ips_rejects_bad_magic");
		check(buf[0] == 0 && buf[5] == 5, "ips_bad_magic_no_change");
	}
	{
		reset();
		const int n = 256;
		std::vector<uint8_t> dst(buf.begin(), buf.begin() + n);
		dst[5] ^= 0x0F;
		std::string p = "UPS1";
		var(p, n); var(p, n); var(p, 5); p += char(0x0F); p += char(0);
		le32(p, crc32(0, buf.data(), n));
		le32(p, crc32(0, dst.data(), n));
		le32(p, crc32(0, (const Bytef *)p.data(), p.size()));
		uint8_t *rom = buf.data(); int size = n;
		check(patchApplyUPS(mem(p), &rom, &size), "ups_applies");
		check(buf[5] == (5 ^ 0x0F) && buf[4] == 4 && buf[6] == 6, "ups_xor_record");
		check(size == n, "ups_size");
		// Applying the same patch to the patched ROM reverts it.
		rom = buf.data(); size = n;
		check(patchApplyUPS(mem(p), &rom, &size), "ups_reverse_applies");
		check(buf[5] == 5, "ups_reverse_restores");
		std::string bad = p; bad[bad.size() - 1] ^= 1;
		rom = buf.data(); size = n;
		check(!patchApplyUPS(mem(bad), &rom, &size), "ups_rejects_bad_crc");
	}
	return fails ? 1 : 0;
}
