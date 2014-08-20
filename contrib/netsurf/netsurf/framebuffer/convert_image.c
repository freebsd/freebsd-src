/*
 * Copyright 2009 Daniel Silverstone <dsilvers@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <png.h>
#include <stdlib.h>

#if PNG_LIBPNG_VER < 10209
#define png_set_expand_gray_1_2_4_to_8(png) png_set_gray_1_2_4_to_8(png)
#endif

static png_structp png;
static png_infop info;
static int interlace;
static size_t rowbytes;
static int raw_width, raw_height;
static int rowstride;
static unsigned char *bitmap_data;
static bool is_cursor = true;
static int raw_hot_x, raw_hot_y;

#define WIDTH (is_cursor?raw_width-1:raw_width)
#define HEIGHT (is_cursor?raw_height-1:raw_height)

#define HOT_X (is_cursor?raw_hot_x-1:0)
#define HOT_Y (is_cursor?raw_hot_y-1:0)

#define REAL(v) (is_cursor?v+1:v)

#define PPIX_AT(x,y) ((bitmap_data + (rowstride * y)) + (x * 4))

#define R_OFF 2
#define G_OFF 1
#define B_OFF 0
#define A_OFF 3

#define R_AT(x,y) *(PPIX_AT(x,y) + R_OFF)
#define G_AT(x,y) *(PPIX_AT(x,y) + G_OFF)
#define B_AT(x,y) *(PPIX_AT(x,y) + B_OFF)
#define A_AT(x,y) *(PPIX_AT(x,y) + A_OFF)

static void info_callback(png_structp png, png_infop info);
static void row_callback(png_structp png, png_bytep new_row,
                         png_uint_32 row_num, int pass);
static void end_callback(png_structp png, png_infop info);



static void
usage(void)
{
        fprintf(stderr, "usage: fb_convert_image input.png output.inc varname\n");
}

static void info_callback(png_structp png, png_infop info);
static void row_callback(png_structp png, png_bytep new_row,
                         png_uint_32 row_num, int pass);
static void end_callback(png_structp png, png_infop info);


static void
detect_hotspot(void)
{
        int i;
        int greenpixels = 0;
        
        for (i = 0; i < raw_width; ++i) {
                if (A_AT(i, 0) == 255) {
                        if (G_AT(i, 0) == 255) {
                                greenpixels++;
                                raw_hot_x = i;
                        }
                        if ((B_AT(i, 0) != 0) || (R_AT(i, 0) != 0)) {
                                is_cursor = false;
                                return;
                        }
                } else if (A_AT(i, 0) != 0) {
                        is_cursor = false;
                        return;
                }
        }
        if (greenpixels != 1) {
                is_cursor = false;
                return;
        }

        for (i = 0; i < raw_height; ++i) {
                if (A_AT(0, i) == 255) {
                        if (G_AT(0, i) == 255) {
                                greenpixels++;
                                raw_hot_y = i;
                        }
                        if ((B_AT(0, i) != 0) || (R_AT(0, i) != 0)) {
                                is_cursor = false;
                                return;
                        }
                } else if (A_AT(0, i) != 0) {
                        is_cursor = false;
                        return;
                }
        }
        if (greenpixels != 2) {
                is_cursor = false;
                return;
        }
        printf("          Pointer detected. Adjusted hotspot at %d, %d (0-based)\n",
               raw_hot_x - 1, raw_hot_y - 1);
}

int
main(int argc, char **argv)
{
        FILE *f;
        unsigned char buffer[1024];
        int br;
        int x, y, c;
        
        if (argc != 4) {
                usage();
                return 1;
        }
        
        printf(" CONVERT: %s (%s)\n", argv[1], argv[3]);
        
        png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
        info = png_create_info_struct(png);
        
        png_set_progressive_read_fn(png, NULL, info_callback, row_callback, end_callback);
        
        f = fopen(argv[1], "rb");
        if (f == NULL) {
                printf("          Unable to open %s\n", argv[1]);
                return 1;
        }
        
        do {
                br = fread(buffer, 1, 1024, f);
                if (br > 0) {
                        png_process_data(png, info, buffer, br);
                }
        } while (br > 0);
        
        if (br < 0) {
                printf("Error reading input: %s\n", strerror(errno));
                fclose(f);
                return 1;
        }
        
        fclose(f);
        
        detect_hotspot();
        
        f = fopen(argv[2], "w");
        if (f == NULL) {
                printf("          Unable to open %s\n", argv[2]);
                return 2;
        }
        
        fprintf(f, "/* This file is auto-generated from %s\n", argv[1]);
        fprintf(f, " *\n * Do not edit this file directly.\n */\n\n");
        fprintf(f, "#include <sys/types.h>\n\n");
        fprintf(f, "#include <stdint.h>\n\n");
        fprintf(f, "#include <stdbool.h>\n\n");
        fprintf(f, "#include <libnsfb.h>\n\n");
        fprintf(f, "#include \"desktop/plot_style.h\"\n");
        fprintf(f, "#include \"framebuffer/gui.h\"\n");
        fprintf(f, "#include \"framebuffer/fbtk.h\"\n\n");
        
        fprintf(f, "static uint8_t %s_pixdata[] = {\n", argv[3]);
        for (y = 0; y < HEIGHT; ++y) {
                unsigned char *rowptr = bitmap_data + (rowstride * y);
                if (is_cursor) {
                        /* If it's a cursor, skip one row and one column */
                        rowptr += rowstride + 4;
                }
                fprintf(f, "\t");
                for (x = 0; x < WIDTH; ++x) {
                        for (c = 0; c < 4; ++c) {
                                unsigned char b = *rowptr++;
                                fprintf(f, "0x%02x, ", b);
                        }
                }
                fprintf(f, "\n");
        }
        fprintf(f, "};\n\n");
        
        fprintf(f, "struct fbtk_bitmap %s = {\n", argv[3]);
        fprintf(f, "\t.width\t\t= %d,\n", WIDTH);
        fprintf(f, "\t.height\t\t= %d,\n", HEIGHT);
        fprintf(f, "\t.hot_x\t\t= %d,\n", HOT_X);
        fprintf(f, "\t.hot_y\t\t= %d,\n", HOT_Y);
        fprintf(f, "\t.pixdata\t= %s_pixdata,\n", argv[3]);
        
        fprintf(f, "};\n\n");
        fclose(f);
        
        return 0;
}

static void
info_callback(png_structp png, png_infop info)
{
	int bit_depth, color_type, interlace, intent;
	double gamma;
	unsigned long width, height;
	
	/* Read the PNG details */
	png_get_IHDR(png, info, &width, &height, &bit_depth,
			&color_type, &interlace, 0, 0);
        
        /* Set up our transformations */
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png);
	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);
	if (bit_depth == 16)
		png_set_strip_16(png);
	if (color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);
	if (!(color_type & PNG_COLOR_MASK_ALPHA))
		png_set_filler(png, 0xff, PNG_FILLER_AFTER);
	/* gamma correction - we use 2.2 as our screen gamma
	 * this appears to be correct (at least in respect to !Browse)
	 * see http://www.w3.org/Graphics/PNG/all_seven.html for a test case
	 */
	if (png_get_sRGB(png, info, &intent))
	        png_set_gamma(png, 2.2, 0.45455);
	else {
	        if (png_get_gAMA(png, info, &gamma))
	                png_set_gamma(png, 2.2, gamma);
	        else
	                png_set_gamma(png, 2.2, 0.45455);
	}


	png_read_update_info(png, info);

	rowbytes = png_get_rowbytes(png, info);
	interlace = (interlace == PNG_INTERLACE_ADAM7);
	raw_width = width;
	raw_height = height;
        
        rowstride = raw_width * 4;
        bitmap_data = malloc(rowstride * raw_height);
}

static unsigned int interlace_start[8] = {0, 16, 0, 8, 0, 4, 0};
static unsigned int interlace_step[8] = {28, 28, 12, 12, 4, 4, 0};
static unsigned int interlace_row_start[8] = {0, 0, 4, 0, 2, 0, 1};
static unsigned int interlace_row_step[8] = {8, 8, 8, 4, 4, 2, 2};

static void
row_callback(png_structp png, png_bytep new_row,
             png_uint_32 row_num, int pass)
{
	unsigned long i, j;
	unsigned int start, step;
        unsigned char *row = bitmap_data + (rowstride * row_num);
                
        if (new_row == 0)
                return;
        
        if (interlace) {
		start = interlace_start[pass];
 		step = interlace_step[pass];
		row_num = interlace_row_start[pass] +
                        interlace_row_step[pass] * row_num;

		/* Copy the data to our current row taking interlacing
		 * into consideration */
                row = bitmap_data + (rowstride * row_num);
		for (j = 0, i = start; i < rowbytes; i += step) {
			row[i++] = new_row[j++];
			row[i++] = new_row[j++];
			row[i++] = new_row[j++];
			row[i++] = new_row[j++];
		}
        } else {
                memcpy(row, new_row, rowbytes);
        }
}

static void
end_callback(png_structp png, png_infop info)
{
}



/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

