// Controller pictures for the device in use (runtime/nvn/padart.c): the Controls screen and the tutorial pictures can
// show the controller the player holds instead of the Switch's. Drawn here, nothing shipped.
#pragma once
#include <stdint.h>

enum { PADART_XBOX, PADART_PLAYSTATION };

// Draws controller `kind` fitted and centred into rgba (w x h texels, RGBA8, straight alpha, rows of w * 4 bytes) over
// a transparent background.
void padart_draw(int kind, uint8_t *rgba, uint32_t w, uint32_t h);
