#ifndef RENDER_H
#define RENDER_H

#include <stddef.h>

typedef struct {
    char ch;
    unsigned char r, g, b;
} Pixel;

typedef struct {
    int width;
    int height;
    Pixel *front_buffer;      // what is currently on the terminal
    Pixel *back_buffer;       // what we are drawing this frame
    char *output_buffer;      // one frame's worth of escape codes, sent with a single write()
    size_t output_capacity;
} Renderer;

Renderer *create_renderer(int width, int height);
void free_renderer(Renderer *rend);
int  resize_renderer(Renderer *rend, int width, int height);
void clear_back_buffer(Renderer *rend, char bg_char, unsigned char r, unsigned char g, unsigned char b);
void draw_pixel(Renderer *rend, int x, int y, char ch, unsigned char r, unsigned char g, unsigned char b);
void present_renderer(Renderer *rend);

#endif