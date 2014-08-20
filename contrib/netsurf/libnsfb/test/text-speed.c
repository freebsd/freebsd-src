/* libnsfb plotter test program */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"

#define UNUSED(x) ((x) = (x))

const struct {
	unsigned int w;
	unsigned int h;
	unsigned char data[16];
} Mglyph1 = {
	8, 16,
	{
		0x00, /* 00000000 */
		0x00, /* 00000000 */
		0xc6, /* 11000110 */
		0xee, /* 11101110 */
		0xfe, /* 11111110 */
		0xfe, /* 11111110 */
		0xd6, /* 11010110 */
		0xc6, /* 11000110 */
		0xc6, /* 11000110 */
		0xc6, /* 11000110 */
		0xc6, /* 11000110 */
		0xc6, /* 11000110 */
		0x00, /* 00000000 */
		0x00, /* 00000000 */
		0x00, /* 00000000 */
		0x00, /* 00000000 */
	}
};

int main(int argc, char **argv)
{
	const char *fename;
	enum nsfb_type_e fetype;
	nsfb_t *nsfb;

	nsfb_bbox_t box;
	nsfb_bbox_t box3;
	uint8_t *fbptr;
	int fbstride;
	int i;
	unsigned int x, y;

	if (argc < 2) {
		fename="sdl";
	} else {
		fename = argv[1];
	}

	fetype = nsfb_type_from_name(fename);
	if (fetype == NSFB_SURFACE_NONE) {
		printf("Unable to convert \"%s\" to nsfb surface type\n",
				fename);
		return EXIT_FAILURE;
	}

	nsfb = nsfb_new(fetype);
	if (nsfb == NULL) {
		printf("Unable to allocate \"%s\" nsfb surface\n", fename);
		return EXIT_FAILURE;
	}

	if (nsfb_init(nsfb) == -1) {
		printf("Unable to initialise nsfb surface\n");
		nsfb_free(nsfb);
		return EXIT_FAILURE;
	}

	/* get the geometry of the whole screen */
	box.x0 = box.y0 = 0;
	nsfb_get_geometry(nsfb, &box.x1, &box.y1, NULL);

	nsfb_get_buffer(nsfb, &fbptr, &fbstride);
	nsfb_claim(nsfb, &box);

	/* Clear to white */
	nsfb_plot_clg(nsfb, 0xffffffff);
	nsfb_update(nsfb, &box);

	/* test glyph plotting */
	for (i = 0; i < 1000; i++) {
		for (y = 0; y < box.y1 - Mglyph1.h; y += Mglyph1.h) {
			for (x = 0; x < box.x1 - Mglyph1.w; x += Mglyph1.w) {
				box3.x0 = x;
				box3.y0 = y;
				box3.x1 = box3.x0 + Mglyph1.w;
				box3.y1 = box3.y0 + Mglyph1.h;

				nsfb_plot_glyph1(nsfb, &box3,  Mglyph1.data,
						Mglyph1.w, 0xff000000);
			}
		}
		nsfb_update(nsfb, &box);
	}

	nsfb_update(nsfb, &box);
	nsfb_free(nsfb);

	return 0;
}
