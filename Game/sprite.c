#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "sprite.h"

Sprite* load_sprite(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return NULL; }

    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, NULL, NULL); fclose(fp); return NULL; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    Sprite *sprite = malloc(sizeof(Sprite));
    sprite->width = (int)png_get_image_width(png, info);
    sprite->height = (int)png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    sprite->row_pointers = malloc(sizeof(png_bytep) * sprite->height);
    for (int y = 0; y < sprite->height; y++) {
        sprite->row_pointers[y] = malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, sprite->row_pointers);
    fclose(fp);
    png_destroy_read_struct(&png, &info, NULL);

    return sprite;
}

void free_sprite(Sprite *sprite) {
    if (!sprite) return;
    for (int y = 0; y < sprite->height; y++) {
        free(sprite->row_pointers[y]);
    }
    free(sprite->row_pointers);
    free(sprite);
}