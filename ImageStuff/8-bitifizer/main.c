#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../lib/stb_image.h"
#include "../lib/stb_image_write.h"
#include "../lib/stb_image_resize2.h"

#include <string.h>

int width, height, channels;
unsigned char *img;

const int bayer_matrix[4][4] = {
    { 0,  8,  2, 10 },
    { 12, 4,  14, 6 },
    { 3,  11, 1,  9 },
    { 15, 7,  13, 5 }
};

int dither_spread = 32;

int main(int argc, char* argv[]) {
    
    for (int i = 1; i < argc; i++) {
        const char *image_name = argv[i];

        img = stbi_load(image_name, &width, &height, &channels, 0);

        if (img == NULL) {
            printf("No image named %s in running directory\n", image_name);
            stbi_image_free(img);
            continue;;
        }

        int new_h = 0;
        int new_w = 0;

        // Resolution scaling to 240p
        if (height > width) {
            new_h = (int)(height * (240.0f/width));
            new_w = 240;
        } else {
            new_w = (int)(width * (240.0f/height));
            new_h = 240;
        }

        if (new_h == 0 || new_w == 0) {
            printf("Error in setting new resolution with image %s :(\n", image_name);
            stbi_image_free(img);
            continue;
        }

        unsigned char *resized_img = malloc(new_w * new_h * channels);

        stbir_resize_uint8_linear(img, width, height, 0, 
                            resized_img, new_w, new_h, 0, 
                            (channels == 4) ? STBIR_RGBA : STBIR_RGB);

        stbi_image_free(img);

        img = resized_img;
        width = new_w;
        height = new_h;

        // Image editing here
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int pixel_index = (y * width + x) * channels;

                int m_val = bayer_matrix[y % 4][x % 4];

                float bias = (m_val / 16.0f) - 0.5f;
                int shift = (int)(bias *dither_spread);

                for (int c = 0; c < 3; c++) {
                    int val = img[pixel_index + c];

                    val += shift;

                    if (val < 0) val = 0;
                    if (val > 255) val = 255;

                    if (c < 2) {
                        img[pixel_index + c] = (val / 32) * 32;
                    } else {
                        img[pixel_index + c] = ((val / 64) * 64) + 31;
                    }
                }
            }
        }

        char output_name[512];
        char temp_name[256];

        strncpy(temp_name, argv[i], sizeof(temp_name));

        char *dot = strchr(temp_name, '.');

        if (dot != NULL) {
            *dot = '\0';
        }

        snprintf(output_name, sizeof(output_name), "%s-8-bit.png", temp_name);
        printf("Processing %s -> %s (%dx%d)\n", argv[i], output_name, width, height);
        stbi_write_png(output_name, width, height, channels, img, width * channels);

        stbi_image_free(img);
    }

	return 0;
}

