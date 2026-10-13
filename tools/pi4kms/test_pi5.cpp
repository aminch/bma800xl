//
// test_pi5.cpp
//
// Tests the Pi 5's display list entries (src/pi5kms/dlist.cpp) on a PC,
// against values worked out by hand from the BCM2712 HVS's entry layout:
// a 384x288 picture shown 1440x1080 at (240, 0), and unscaled.
//
//   tools/pi4kms/run_tests.sh
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#include "../../src/pi5kms/dlist.h"

#include <stdio.h>

static int g_failed = 0;

static void Check(bool ok, const char *name, const char *what) {
  printf("%s  %s: %s\n", ok ? "PASS" : "FAIL", name, what);
  if (!ok) {
    g_failed++;
  }
}

static kms::Plane Scaled(bool nearest) {
  kms::Plane p;
  p.address = 0x12345678;
  p.pitch = 1536;
  p.src_width = 384;
  p.src_height = 288;
  p.x = 240;
  p.y = 0;
  p.width = 1440;
  p.height = 1080;
  p.nearest = nearest;
  return p;
}

static void TestScaled(void) {
  const char *name = "pi5 scaled entry";
  kms::PlaneMemory m = {31, 100, 36, 3072};
  uint32_t w[kms::MaxWords];
  unsigned n = kms::BuildList(Scaled(false), m, false, w);
  Check(n == 19, name, "18-word entry and the list's end word");
  if (n != 19) {
    return;
  }
  Check(w[0] == 0x52004007, name, "control 0: valid, 18 words, ARGB, RGBA8888, PPF");
  Check(w[1] == 0x000000F0, name, "position 0: x 240, y 0");
  Check(w[2] == 0x4000FFF0, name, "control 2 (C): fixed alpha, opaque");
  Check(w[3] == 0x0437059F, name, "position 1: 1440x1080 less one");
  Check(w[4] == 0x011F017F, name, "position 2: 384x288 less one");
  Check(w[5] == 0xC0C0C0C0, name, "context");
  Check(w[6] == ((100u << 16) | (31u << 10)), name, "pointer 0: UPM base and handle");
  Check(w[7] == 0x12345678, name, "pointer 1: address");
  Check(w[8] == 1536, name, "pointer 2: pitch");
  Check(w[9] == 36, name, "line buffer start");
  Check(w[10] == 0x40444460 && w[11] == 0x40444460, name, "PPF words, smooth");
  Check(w[12] == 0xC0C0C0C0, name, "vertical PPF context");
  Check(w[13] == 3072 && w[16] == 3072, name, "kernel words");
  Check(w[17] == 0x80000000 && w[18] == 0x80000000, name, "entry end and list end");

  n = kms::BuildList(Scaled(true), m, true, w);
  Check(n == 19 && w[0] == 0x520C4007, name, "control 0 (D): fixed alpha mask");
  Check(w[2] == 0x2000FFF0, name, "control 2 (D)");
  Check(w[10] == 0xC0444460, name, "PPF word, sharp");

  kms::Plane high = Scaled(false);
  high.address = 0x0123456780ull;
  n = kms::BuildList(high, m, false, w);
  Check(n == 19 && (w[6] & 0xFF) == 0x01 && w[7] == 0x23456780, name,
        "address bits 39:32 in pointer 0");
  high.address = 1ull << 40;
  Check(kms::BuildList(high, m, false, w) == 0, name, "addresses past 40 bits refused");
}

static void TestUnscaled(void) {
  const char *name = "pi5 unscaled entry";
  kms::Plane p = Scaled(false);
  p.width = 384;
  p.height = 288;
  kms::PlaneMemory m = {0, 0, 0, 0};
  uint32_t w[kms::MaxWords];
  unsigned n = kms::BuildList(p, m, false, w);
  Check(n == 10, name, "9-word entry and the list's end word");
  Check(w[0] == 0x4900C007, name, "control 0: 9 words, unity");
  Check(w[3] == 0x011F017F, name, "position 2 follows control 2");
  Check(w[8] == 0x80000000 && w[9] == 0x80000000, name, "entry end and list end");
  Check(kms::LbmBlocksFor(p) == 0, name, "no line buffer");

  kms::EntryUse use;
  Check(kms::ReadEntry(w, n, &use) && use.words == 9 && !use.has_lbm &&
            use.upm_units == 13,
        name, "read back");
  Check(!kms::ReadEntry(w + 9, 1, &use), name, "the list's end stops reading");
}

static void TestSizes(void) {
  const char *name = "pi5 memory sizes";
  Check(kms::UpmUnitsFor(1536) == 13, name, "UPM for 384 pixels: 13 units");
  Check(kms::UpmUnitsFor(7680) == 61, name, "UPM for 1920 pixels: 61 units");
  Check(kms::LbmBlocksFor(Scaled(false)) == 36, name, "LBM for 384 -> 1440: 36 blocks");

  kms::PlaneMemory m = {5, 20, 40, 3072};
  uint32_t w[kms::MaxWords];
  unsigned n = kms::BuildList(Scaled(false), m, false, w);
  kms::EntryUse use;
  Check(kms::ReadEntry(w, n, &use) && use.words == 18 && use.upm_handle == 5 &&
            use.upm_base == 20 && use.has_lbm && use.lbm == 40 && use.lbm_blocks >= 36,
        name, "a scaled entry read back");

  kms::Plane small = Scaled(false);
  small.width = 200;  // less than 2/3 of 384
  Check(kms::BuildList(small, m, false, w) == 0, name, "shrinking past 2/3 refused");
}

int main(void) {
  TestScaled();
  TestUnscaled();
  TestSizes();
  printf("\n%s\n", g_failed ? "SOME TESTS FAILED." : "All tests passed.");
  return g_failed ? 1 : 0;
}
