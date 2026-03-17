#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../lib/stb_image.h"
#include "../lib/stb_image_write.h"
#include "../lib/stb_image_resize2.h"

#include <math.h>
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

// Sobel wights for edge finding
const int weights_x[3][3] = {
    {-1, 0, 1},
    {-2, 0, 2},
    {-1, 0, 1}
};

const int weights_y[3][3] = {
    { 1,  2,  1},
    { 0,  0,  0},
    {-1, -2, -1}
};

// Blur
const int box_blur[3][3] = {
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1}
};

float apply_kernel(unsigned char* buffer, int x, int y, int w, int h, int c_idx, const int kernel[3][3]) {
    float sum = 0;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            int ny = y + i;
            int nx = x + j;

            // Edge handling: repeat the edge pixel if out of bounds
            if (nx < 0) nx = 0;
            if (nx >= w) nx = w - 1;
            if (ny < 0) ny = 0;
            if (ny >= h) ny = h - 1;

            // c_idx is the specific channel (0 for Red, 1 for Green, etc.)
            // Or just 0 if the buffer only has 1 channel (like a grayscale map)
            int idx = (ny * w + nx) * channels + c_idx;
            sum += buffer[idx] * kernel[i + 1][j + 1];
        }
    }
    return sum;
}

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

        // --- Edge finding ---
        unsigned char *blur_img = calloc(width * height * channels, 1);
        unsigned char *edge_img = calloc(width * height, 1);

        // Blur
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                int pixel_index = (y * width + x) * channels;

                for (int c = 0; c < 3; c++) {
                    float val = apply_kernel(img, x, y, width, height, c, box_blur);
                    blur_img[pixel_index + c] = (unsigned char)(val / 9.0f);
                }
            }
        }

        // Edges
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                float gx = apply_kernel(blur_img, x, y, width, height, 1, weights_x);
                float gy = apply_kernel(blur_img, x, y, width, height, 1, weights_y);

                // Calculate magnitude
                int magnitude = (int)sqrt(gx*gx + gy*gy);
                edge_img[y * width + x] = (magnitude > 40) ? 255:0; // Thresholding
            }
        }

        // --- Image editing ---
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Rewriting colours
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

        // 8-bit saving
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

        // Edge saving
        char edge_output[512];
        snprintf(edge_output, sizeof(edge_output), "%s-edges.png", temp_name);
        stbi_write_png(edge_output, width, height, 1, edge_img, width);

        // Don't forget to free it!
        free(edge_img);

        // Bluer saving
        char blur_output[512];
        snprintf(blur_output, sizeof(blur_output), "%s-blur.png", temp_name);
        stbi_write_png(blur_output, width, height, 1, blur_img, width);

        // Don't forget to free it!
        free(blur_img);
    }

	return 0;
}

