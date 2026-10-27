// Checks runtime/nvn/bcdecode.c (the BC1-BC7 decode the Vulkan backend uses on GPUs without BC, over third_party/bcdec)
// on hand-made blocks: texel values and formats of each BC, blocks cropped at the image edge, block order, no writes
// past the output. No game data.
//   clang -O1 -Wall -Wno-unused-function -Iruntime/nvn tools/test_bcdecode.c runtime/nvn/bcdecode.c -o build/test_bcdecode && build/test_bcdecode
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcdecode.h"

static int g_fail, g_checks;
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// bits [pos, pos + n) of b = v, least significant first (how BC6H and BC7 pack their fields)
static void put(uint8_t *b, unsigned pos, unsigned n, uint64_t v) {
    for (unsigned i = 0; i < n; i++)
        if ((v >> i) & 1) b[(pos + i) >> 3] |= (uint8_t)(1u << ((pos + i) & 7));
}

// Decodes into a buffer with a canary after it, which must stay untouched.
static uint8_t *decode(int bc, const uint8_t *blocks, uint32_t w, uint32_t h, int sgn) {
    size_t n = (size_t)w * h * bc_texel_bytes(bc);
    uint8_t *o = malloc(n + 16);
    memset(o, 0xAA, n + 16);
    bc_decode(bc, blocks, w, h, sgn, o);
    for (int i = 0; i < 16; i++) CHECK(o[n + i] == 0xAA);
    return o;
}

static int rgba(const uint8_t *o, uint32_t w, uint32_t x, uint32_t y, int r, int g, int b, int a) {
    const uint8_t *p = o + ((size_t)y * w + x) * 4;
    return p[0] == r && p[1] == g && p[2] == b && p[3] == a;
}

static void test_sizes(void) {
    CHECK(bc_block_bytes(BCD_BC1) == 8 && bc_block_bytes(BCD_BC4) == 8);
    CHECK(bc_block_bytes(BCD_BC2) == 16 && bc_block_bytes(BCD_BC3) == 16 && bc_block_bytes(BCD_BC5) == 16);
    CHECK(bc_block_bytes(BCD_BC6H) == 16 && bc_block_bytes(BCD_BC7) == 16);
    CHECK(bc_texel_bytes(BCD_BC1) == 4 && bc_texel_bytes(BCD_BC2) == 4 && bc_texel_bytes(BCD_BC3) == 4);
    CHECK(bc_texel_bytes(BCD_BC4) == 1 && bc_texel_bytes(BCD_BC5) == 2);
    CHECK(bc_texel_bytes(BCD_BC6H) == 8 && bc_texel_bytes(BCD_BC7) == 4);
}

static void test_bc1(void) {
    // 4-colour mode (c0 > c1): red, blue, 2/3 red + 1/3 blue, 1/3 red + 2/3 blue in the first row
    const uint8_t b4[8] = {0x00, 0xF8, 0x1F, 0x00, 0xE4, 0, 0, 0};
    uint8_t *o = decode(BCD_BC1, b4, 4, 4, 0);
    CHECK(rgba(o, 4, 0, 0, 255, 0, 0, 255));
    CHECK(rgba(o, 4, 1, 0, 0, 0, 255, 255));
    CHECK(rgba(o, 4, 2, 0, 170, 0, 85, 255));
    CHECK(rgba(o, 4, 3, 0, 85, 0, 170, 255));
    CHECK(rgba(o, 4, 3, 3, 255, 0, 0, 255));  // (the other rows: index 0)
    free(o);
    // 3-colour mode (c0 <= c1): blue, red, their middle, and transparent black (BC1's 1-bit alpha)
    const uint8_t b3[8] = {0x1F, 0x00, 0x00, 0xF8, 0xE4, 0, 0, 0};
    o = decode(BCD_BC1, b3, 4, 4, 0);
    CHECK(rgba(o, 4, 0, 0, 0, 0, 255, 255));
    CHECK(rgba(o, 4, 1, 0, 255, 0, 0, 255));
    CHECK(rgba(o, 4, 2, 0, 128, 0, 128, 255));
    CHECK(rgba(o, 4, 3, 0, 0, 0, 0, 0));
    free(o);
}

static void test_layout(void) {
    // 6 x 5 texels: 2 x 2 blocks (red, blue / green, white), the right and bottom ones cut off
    const uint16_t col[4] = {0xF800, 0x001F, 0x07E0, 0xFFFF};
    uint8_t blocks[4 * 8] = {0};
    for (int i = 0; i < 4; i++) { blocks[i * 8] = blocks[i * 8 + 2] = col[i] & 0xFF; blocks[i * 8 + 1] = blocks[i * 8 + 3] = col[i] >> 8; }
    uint8_t *o = decode(BCD_BC1, blocks, 6, 5, 0);
    CHECK(rgba(o, 6, 0, 0, 255, 0, 0, 255) && rgba(o, 6, 3, 3, 255, 0, 0, 255));
    CHECK(rgba(o, 6, 4, 0, 0, 0, 255, 255) && rgba(o, 6, 5, 3, 0, 0, 255, 255));
    CHECK(rgba(o, 6, 0, 4, 0, 255, 0, 255) && rgba(o, 6, 3, 4, 0, 255, 0, 255));
    CHECK(rgba(o, 6, 4, 4, 255, 255, 255, 255) && rgba(o, 6, 5, 4, 255, 255, 255, 255));
    free(o);
    o = decode(BCD_BC1, blocks, 1, 1, 0);  // (a 1 x 1 mip level: one texel of its block)
    CHECK(rgba(o, 1, 0, 0, 255, 0, 0, 255));
    free(o);
}

static void test_bc2_bc3(void) {
    // BC2: explicit 4-bit alpha (first row 15, 8, 0, 1), white colour
    uint8_t b2[16] = {0x8F, 0x10, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0};
    uint8_t *o = decode(BCD_BC2, b2, 4, 4, 0);
    CHECK(rgba(o, 4, 0, 0, 255, 255, 255, 255) && rgba(o, 4, 1, 0, 255, 255, 255, 136));
    CHECK(rgba(o, 4, 2, 0, 255, 255, 255, 0) && rgba(o, 4, 3, 0, 255, 255, 255, 17));
    free(o);
    // BC3: alpha 255 .. 0, indices 0, 1, 7 (1/7 of the way), 0
    uint8_t b3[16] = {255, 0, 0xC8, 0x01, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0};
    o = decode(BCD_BC3, b3, 4, 4, 0);
    CHECK(rgba(o, 4, 0, 0, 255, 255, 255, 255) && rgba(o, 4, 1, 0, 255, 255, 255, 0));
    CHECK(rgba(o, 4, 2, 0, 255, 255, 255, 36) && rgba(o, 4, 3, 0, 255, 255, 255, 255));
    free(o);
}

static void test_bc4_bc5(void) {
    // BC4 UNORM, 6 interpolated values (a0 > a1): 255, 0, 36 (index 7), 255
    const uint8_t u6[8] = {255, 0, 0xC8, 0x01, 0, 0, 0, 0};
    uint8_t *o = decode(BCD_BC4, u6, 4, 4, 0);
    CHECK(o[0] == 255 && o[1] == 0 && o[2] == 36 && o[3] == 255);
    free(o);
    // 4 interpolated values and 0 / 255 (a0 <= a1): index 6 is 0, 7 is 255, 2 is 1/5 of the way, 1 is a1
    const uint8_t u4[8] = {0, 255, 0xBE, 0x02, 0, 0, 0, 0};
    o = decode(BCD_BC4, u4, 4, 4, 0);
    CHECK(o[0] == 0 && o[1] == 255 && o[2] == 51 && o[3] == 255);
    free(o);
    // BC4 SNORM: 127 .. -128 (which counts as -127); indices 0, 1, 7, 0 -> 127, -127, -91, 127
    const uint8_t s6[8] = {0x7F, 0x80, 0xC8, 0x01, 0, 0, 0, 0};
    o = decode(BCD_BC4, s6, 4, 4, 1);
    CHECK((int8_t)o[0] == 127 && (int8_t)o[1] == -127 && (int8_t)o[2] == -91 && (int8_t)o[3] == 127);
    free(o);
    // BC5: red like the first BC4 (index 0 = 255, 1 = 0), green a 4-value one (0 .. 255: index 7 = 255, 2 = 51)
    const uint8_t b5[16] = {255, 0, 0x08, 0, 0, 0, 0, 0, 0, 255, 0x17, 0, 0, 0, 0, 0};
    o = decode(BCD_BC5, b5, 4, 4, 0);
    CHECK(o[0] == 255 && o[1] == 255);  // texel 0: red index 0, green index 7
    CHECK(o[2] == 0 && o[3] == 51);     // texel 1: red index 1, green index 2
    free(o);
    // BC5 SNORM: both 127 .. -127; red index 0 -> 127, green index 1 -> -127
    const uint8_t s5[16] = {0x7F, 0x81, 0, 0, 0, 0, 0, 0, 0x7F, 0x81, 0x08, 0, 0, 0, 0, 0};
    o = decode(BCD_BC5, s5, 4, 4, 1);
    CHECK((int8_t)o[0] == 127 && (int8_t)o[1] == 127);  // texel 0: red index 0, green index 0 (127)
    CHECK((int8_t)o[2] == 127 && (int8_t)o[3] == -127); // texel 1: red index 0, green index 1 (-127)
    free(o);
}

static uint16_t u16(const uint8_t *o, size_t i) { uint16_t v; memcpy(&v, o + i * 2, 2); return v; }

static void test_bc6h(void) {
    // Mode 11 (00011, then 10-bit endpoints): both endpoints 0x200 in all channels, so every texel is that value
    uint8_t b[16] = {0};
    put(b, 0, 5, 0x03);
    for (int i = 0; i < 6; i++) put(b, 5 + i * 10, 10, 0x200);
    uint8_t *o = decode(BCD_BC6H, b, 4, 4, 0);
    for (uint32_t t = 0; t < 16; t++) {  // unsigned: 0x8020 * 31 >> 6 = 0x3E0F (about 1.51); alpha 1.0
        CHECK(u16(o, t * 4) == 0x3E0F && u16(o, t * 4 + 1) == 0x3E0F && u16(o, t * 4 + 2) == 0x3E0F);
        CHECK(u16(o, t * 4 + 3) == 0x3C00);
    }
    free(o);
    o = decode(BCD_BC6H, b, 3, 2, 0);  // (cut off to 3 x 2)
    for (uint32_t t = 0; t < 6; t++) CHECK(u16(o, t * 4) == 0x3E0F && u16(o, t * 4 + 3) == 0x3C00);
    free(o);
    o = decode(BCD_BC6H, b, 4, 4, 1);  // signed: 0x200 is -512, the most negative: -65504
    CHECK(u16(o, 0) == 0xFBFF && u16(o, 1) == 0xFBFF && u16(o, 2) == 0xFBFF && u16(o, 3) == 0x3C00);
    free(o);
    // a reserved mode decodes to zeros, alpha still 1.0
    uint8_t r[16] = {0x13};  // 10011
    o = decode(BCD_BC6H, r, 4, 4, 0);
    CHECK(u16(o, 0) == 0 && u16(o, 1) == 0 && u16(o, 2) == 0 && u16(o, 3) == 0x3C00);
    free(o);
}

static void test_bc7(void) {
    // Mode 6 (bit 6 set): RGBA 7 bits and a p-bit per endpoint, 4-bit indices. Endpoint 0 (127, 0, 64, 127) + p-bit 1,
    // endpoint 1 (0, 127, 0, 0) + p-bit 0: texels are (255, 1, 129, 255) and (0, 254, 0, 0); texel 1 uses index 15
    uint8_t b[16] = {0};
    put(b, 6, 1, 1);
    const uint8_t e0[4] = {127, 0, 64, 127}, e1[4] = {0, 127, 0, 0};
    for (int c = 0; c < 4; c++) { put(b, 7 + c * 14, 7, e0[c]); put(b, 14 + c * 14, 7, e1[c]); }
    put(b, 63, 1, 1);
    put(b, 68, 4, 15);  // indices start at bit 65: texel 0 has 3 bits, texel 1 follows
    uint8_t *o = decode(BCD_BC7, b, 4, 4, 0);
    CHECK(rgba(o, 4, 0, 0, 255, 1, 129, 255));
    CHECK(rgba(o, 4, 1, 0, 0, 254, 0, 0));
    CHECK(rgba(o, 4, 2, 0, 255, 1, 129, 255) && rgba(o, 4, 3, 3, 255, 1, 129, 255));
    free(o);
}

int main(void) {
    test_sizes();
    test_bc1();
    test_layout();
    test_bc2_bc3();
    test_bc4_bc5();
    test_bc6h();
    test_bc7();
    printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail != 0;
}
