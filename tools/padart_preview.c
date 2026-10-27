// The controller pictures of runtime/nvn/padart.c as PNG files, to look at them without the game: each kind at the
// sizes of the game's controller pictures, over a dark background like the game's menus, as
// build/padart_<kind>_<w>x<h>.png. Checks a few texels too (exits 1 when they are wrong).
//   clang -O1 -Iruntime/nvn -Ithird_party/stb tools/padart_preview.c runtime/nvn/padart.c -o build/padart_preview -lm
//   build/padart_preview
#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "padart.h"

int main(void) {
    static const uint32_t sizes[][2] = {{960, 500}, {480, 250}, {188, 188}};  // (Wii U's Controls screen and tips)
    static const char *names[] = {"xbox", "playstation"};
    int bad = 0;
    for (int kind = 0; kind < 2; kind++)
        for (int i = 0; i < 3; i++) {
            uint32_t w = sizes[i][0], h = sizes[i][1];
            uint8_t *px = malloc((size_t)w * h * 4);
            padart_draw(kind, px, w, h);
            size_t n = (size_t)w * h, opaque = 0;
            for (size_t k = 0; k < n; k++) opaque += px[4 * k + 3] == 255;
            // the corners are background, the centre is the controller, and it fills a good part of the picture
            if (px[3] || px[4 * (n - 1) + 3] || px[4 * ((size_t)h / 2 * w + w / 2) + 3] != 255 || opaque < n / 4 || opaque > n * 4 / 5) {
                fprintf(stderr, "%s %ux%u: wrong coverage (%zu of %zu texels opaque)\n", names[kind], w, h, opaque, n);
                bad = 1;
            }
            for (size_t k = 0; k < n; k++) {  // over the menus' dark grey
                uint8_t *q = px + 4 * k;
                for (int c = 0; c < 3; c++) q[c] = (uint8_t)((q[c] * q[3] + 0x22 * (255 - q[3])) / 255);
                q[3] = 255;
            }
            char path[64];
            snprintf(path, sizeof path, "build/padart_%s_%ux%u.png", names[kind], w, h);
            if (!stbi_write_png(path, (int)w, (int)h, 4, px, (int)w * 4)) fprintf(stderr, "can't write %s\n", path), bad = 1;
            else printf("%s\n", path);
            free(px);
        }
    return bad;
}
