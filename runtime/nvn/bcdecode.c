// BC1-BC7 block decoding on the CPU: a thin wrapper over bcdec (https://github.com/iOrange/bcdec, MIT or Unlicense,
// pinned at commit 80859ed, v0.985, in third_party/bcdec). It decodes one 4x4 block at a time; this crops the blocks
// at the edges of the image and makes BC6H texels RGBA.
#include "bcdecode.h"

#include <string.h>

#define BCDEC_STATIC
#define BCDEC_BC4BC5_PRECISE  // BC4 / BC5 interpolated with rounding (as the GPUs do), and their signed (SNORM) modes
#define BCDEC_IMPLEMENTATION
#include "../../third_party/bcdec/bcdec.h"

uint32_t bc_block_bytes(int bc) { return bc == BCD_BC1 || bc == BCD_BC4 ? 8 : 16; }
uint32_t bc_texel_bytes(int bc) { return bc == BCD_BC4 ? 1 : bc == BCD_BC5 ? 2 : bc == BCD_BC6H ? 8 : 4; }

// One block into dst, rows `pitch` bytes apart (8-byte aligned, as the texels of the decoded format are)
static void block(int bc, const uint8_t *src, uint8_t *dst, int pitch, bool sgn) {
    switch (bc) {
    case BCD_BC1: bcdec_bc1(src, dst, pitch); break;
    case BCD_BC2: bcdec_bc2(src, dst, pitch); break;
    case BCD_BC3: bcdec_bc3(src, dst, pitch); break;
    case BCD_BC4: bcdec_bc4(src, dst, pitch, sgn); break;
    case BCD_BC5: bcdec_bc5(src, dst, pitch, sgn); break;
    case BCD_BC7: bcdec_bc7(src, dst, pitch); break;
    case BCD_BC6H: {  // half floats, RGB (its pitch counts them); alpha 1.0 as sampling a BC6H texture gives
        uint16_t rgb[4 * 4 * 3];
        bcdec_bc6h_half(src, rgb, 4 * 3, sgn);
        for (int y = 0; y < 4; y++) {
            uint16_t *o = (uint16_t *)(dst + (size_t)y * pitch);
            for (int x = 0; x < 4; x++) {
                memcpy(o + x * 4, rgb + (y * 4 + x) * 3, 6);
                o[x * 4 + 3] = 0x3C00;
            }
        }
        break;
    }
    }
}

void bc_decode(int bc, const uint8_t *blocks, uint32_t width, uint32_t height, bool is_signed, uint8_t *out) {
    if (bc < BCD_BC1 || bc > BCD_BC7) return;
    uint32_t bs = bc_block_bytes(bc), tb = bc_texel_bytes(bc);
    uint32_t bx = (width + 3) / 4, by = (height + 3) / 4;
    uint64_t tmp[16];  // a block's texels (at most 4 x 4 x 8 bytes)
    for (uint32_t j = 0; j < by; j++) {
        for (uint32_t i = 0; i < bx; i++) {
            const uint8_t *src = blocks + ((size_t)j * bx + i) * bs;
            uint32_t x0 = i * 4, y0 = j * 4;
            uint32_t cw = width - x0 < 4 ? width - x0 : 4, ch = height - y0 < 4 ? height - y0 : 4;
            if (cw == 4 && ch == 4) {  // inside the image: decoded in place
                block(bc, src, out + ((size_t)y0 * width + x0) * tb, (int)(width * tb), is_signed);
                continue;
            }
            block(bc, src, (uint8_t *)tmp, (int)(4 * tb), is_signed);
            for (uint32_t y = 0; y < ch; y++)
                memcpy(out + ((size_t)(y0 + y) * width + x0) * tb, (uint8_t *)tmp + (size_t)y * 4 * tb, cw * tb);
        }
    }
}
