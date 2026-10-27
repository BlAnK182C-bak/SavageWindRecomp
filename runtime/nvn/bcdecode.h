// BC1-BC7 decoder (third_party/bcdec) to the uncompressed texels the Vulkan backend uploads, for GPUs without BC
// texture compression (Mali, most phones) or SAVAGEWIND_VK_DECODE_BC=1 (runtime/nvn/vulkan/textures.c).
#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { BCD_BC1 = 1, BCD_BC2, BCD_BC3, BCD_BC4, BCD_BC5, BCD_BC6H, BCD_BC7 };

uint32_t bc_block_bytes(int bc);  // a block of the format: 8 (BC1, BC4) or 16
// A texel of the decoded form: 4 (R8G8B8A8: BC1, BC2, BC3, BC7), 1 (R8: BC4), 2 (R8G8: BC5), 8 (R16G16B16A16 half
// floats, alpha 1.0: BC6H). The 8-bit ones are UNORM, or SNORM with is_signed (BC4, BC5); BC6H is_signed: SFLOAT.
uint32_t bc_texel_bytes(int bc);

// blocks: row-major blocks covering ceil(width/4) x ceil(height/4). out: width*height texels, tightly packed rows.
void bc_decode(int bc, const uint8_t *blocks, uint32_t width, uint32_t height, bool is_signed, uint8_t *out);
