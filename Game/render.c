#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "render.h"

// Worst case per changed cell: cursor move (~12) + 24-bit color (~19) + char (1)
#define BYTES_PER_CELL 40

static void write_all(const char *buf, size_t len) {
    while (len > 0) {
        ssize_t w = write(STDOUT_FILENO, buf, len);
        if (w < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN) { usleep(500); continue; }
            return;
        }
        buf += w;
        len -= (size_t)w;
    }
}

static void write_str(const char *s) { write_all(s, strlen(s)); }

static int alloc_buffers(Renderer *rend, int width, int height) {
    free(rend->front_buffer);
    free(rend->back_buffer);
    free(rend->output_buffer);

    size_t cells = (size_t)width * (size_t)height;
    rend->width = width;
    rend->height = height;
    // calloc => ch == 0 everywhere, which never matches a real back-buffer cell,
    // so the first present redraws the whole screen.
    rend->front_buffer = calloc(cells, sizeof(Pixel));
    rend->back_buffer = calloc(cells, sizeof(Pixel));
    rend->output_capacity = cells * BYTES_PER_CELL + 64;
    rend->output_buffer = malloc(rend->output_capacity);

    return rend->front_buffer && rend->back_buffer && rend->output_buffer;
}

Renderer *create_renderer(int width, int height) {
    Renderer *rend = calloc(1, sizeof(Renderer));
    if (!rend) return NULL;
    if (!alloc_buffers(rend, width, height)) {
        free_renderer(rend);
        return NULL;
    }
    // Alternate screen (your shell history is restored on exit), hide cursor, clear
    write_str("\x1b[?1049h\x1b[?25l\x1b[2J");
    return rend;
}

void free_renderer(Renderer *rend) {
    if (!rend) return;
    // Reset colors, show cursor, leave alternate screen
    write_str("\x1b[0m\x1b[?25h\x1b[?1049l");
    free(rend->front_buffer);
    free(rend->back_buffer);
    free(rend->output_buffer);
    free(rend);
}

int resize_renderer(Renderer *rend, int width, int height) {
    if (!alloc_buffers(rend, width, height)) return 0;
    write_str("\x1b[0m\x1b[2J");
    return 1;
}

void clear_back_buffer(Renderer *rend, char bg_char, unsigned char r, unsigned char g, unsigned char b) {
    Pixel blank = { bg_char, r, g, b };
    size_t cells = (size_t)rend->width * (size_t)rend->height;
    for (size_t i = 0; i < cells; i++) rend->back_buffer[i] = blank;
}

void draw_pixel(Renderer *rend, int x, int y, char ch, unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || x >= rend->width || y < 0 || y >= rend->height) return;
    rend->back_buffer[y * rend->width + x] = (Pixel){ ch, r, g, b };
}

void present_renderer(Renderer *rend) {
    char *out = rend->output_buffer;
    size_t n = 0;

    int cur_x = -1, cur_y = -1;           // where the terminal cursor will be
    int have_color = 0;
    unsigned char cr = 0, cg = 0, cb = 0; // last color we emitted

    for (int y = 0; y < rend->height; y++) {
        for (int x = 0; x < rend->width; x++) {
            int idx = y * rend->width + x;
            const Pixel *back = &rend->back_buffer[idx];
            const Pixel *front = &rend->front_buffer[idx];

            // Only send cells that changed since last frame
            if (back->ch == front->ch && back->r == front->r &&
                back->g == front->g && back->b == front->b) continue;

            // Only move the cursor if it isn't already where we need it
            if (cur_y != y || cur_x != x)
                n += (size_t)sprintf(out + n, "\x1b[%d;%dH", y + 1, x + 1);

            // Only change color if it differs from the previous emitted cell
            if (!have_color || back->r != cr || back->g != cg || back->b != cb) {
                n += (size_t)sprintf(out + n, "\x1b[38;2;%u;%u;%um",
                                     (unsigned)back->r, (unsigned)back->g, (unsigned)back->b);
                cr = back->r; cg = back->g; cb = back->b;
                have_color = 1;
            }

            out[n++] = back->ch;
            cur_x = x + 1;
            cur_y = y;
        }
    }

    // One syscall per frame instead of thousands of printf calls
    if (n > 0) write_all(out, n);

    // front now mirrors the screen; back is stale and must be cleared by the caller
    Pixel *tmp = rend->front_buffer;
    rend->front_buffer = rend->back_buffer;
    rend->back_buffer = tmp;
}