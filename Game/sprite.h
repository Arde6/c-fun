#ifndef SPRITE_H
#define SPRITE_H

#include <png.h>

typedef struct {
    int width, height;
    png_bytep *row_pointers;
} Sprite;

Sprite* load_sprite(const char *filename);
void free_sprite(Sprite *sprite);

#endif