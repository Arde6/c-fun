#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct winsize terminal;

int width, height;
png_byte color_type;
png_byte bit_depth;
png_bytep *row_pointers = NULL;

void read_png_file(char *filename, int t_width, int t_height) {
    // Open image as a file
    FILE *fp = fopen(filename, "rb");

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) abort();

    png_infop info = png_create_info_struct(png);
    if(!info) abort();

    if(setjmp(png_jmpbuf(png))) abort();

    png_init_io(png, fp);
    png_read_info(png, info);

    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    color_type = png_get_color_type(png, info);
    bit_depth = png_get_bit_depth(png, info);

    // Read color_type info 8bit, RGBA
    if(bit_depth == 16)
        png_set_strip_16(png);

    if(color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    // Gray
    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if(png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    // NO alpha -> fill with 0xff
    if(color_type == PNG_COLOR_TYPE_RGB ||
       color_type == PNG_COLOR_TYPE_GRAY ||
       color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if(color_type == PNG_COLOR_TYPE_GRAY ||
     color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    if (row_pointers) abort();

    row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for(int y= 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, row_pointers);

    fclose(fp);

    png_destroy_read_struct(&png, &info, NULL);
}

void process_png_file(int output_width, int output_height) {
    static const char ramp[] = ".-=+*#%@";

    for (int y = 0; y < output_height; y++) {
        int source_y = (int)((long long)y * height / output_height);
        png_bytep row = row_pointers[source_y];

        for (int x = 0; x < output_width; x++) {
            int source_x = (int)((long long)x * width / output_width);
            png_bytep px = &(row[source_x * 4]);
            
            if (px[3] == 0) {
                printf(" ");
            } else {
                unsigned int brightness =
                    (299u * px[0] + 587u * px[1] + 114u * px[2]) / 1000u;
                    // 255;
                size_t index = brightness * (sizeof(ramp) - 2) / 255;
                printf("\x1b[38;2;%u;%u;%um%c", (unsigned)px[0], (unsigned)px[1], (unsigned)px[2], ramp[index]);
            }
        }
        printf("\x1b[0m\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image.png>\n", argv[0]);
        return 1;
    }

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal) == -1) {
        perror("ioctl");
        return 1;
    }

    int columns = terminal.ws_col;
    int rows = terminal.ws_row;

    read_png_file(argv[1], columns, rows);
    int max_height = rows > 1 ? rows - 1 : 1;

    double char_height_scale = 0.5; // terminal character width / height ratio

    // Calculate scale factors to fit both width and height inside the terminal window
    double scale_w = (double)columns / width;
    double scale_h = (double)max_height / (height * char_height_scale);
    
    // Pick the smaller scale to ensure it fits completely without wrapping
    double scale = scale_w < scale_h ? scale_w : scale_h;
    if (scale > 1.0) scale = 1.0; // Don't upscale smaller images

    int output_width = (int)(width * scale);
    int output_height = (int)(height * scale * char_height_scale);

    if (output_width < 1) output_width = 1;
    if (output_height < 1) output_height = 1;

    process_png_file(output_width, output_height);

    return 0;
}