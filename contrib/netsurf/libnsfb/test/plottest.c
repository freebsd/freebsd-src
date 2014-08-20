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
    8, 16, {
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

const struct {
    unsigned int w;
    unsigned int h;
    unsigned char data[16 * 8];
} Mglyph8 = {
    8, 16, {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 00000000 */
        0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x00, /* 00000000 */
        0xaa, 0xff, 0x00, 0x00, 0x00, 0xff, 0xaa, 0x00, /* 11000110 */
        0xaa, 0xff, 0xff, 0x00, 0xff, 0xff, 0xaa, 0x00, /* 11101110 */
        0xaa, 0xff, 0xff, 0xff, 0xff, 0xff, 0xaa, 0x00, /* 11111110 */
        0xaa, 0xff, 0xff, 0xff, 0xff, 0xff, 0xaa, 0x00, /* 11111110 */
        0xaa, 0xff, 0x00, 0x00, 0x00, 0xff, 0xaa, 0x00, /* 11010110 */
        0xaa, 0xff, 0x00, 0x00, 0x00, 0xff, 0xaa, 0x00, /* 11000110 */
        0xaa, 0xff, 0x00, 0x00, 0x00, 0xff, 0xaa, 0x00, /* 11000110 */
        0xaa, 0xff, 0x00, 0x00, 0x00, 0xff, 0xaa, 0x00, /* 11000110 */
        0xaa, 0xff, 0x00, 0x00, 0x00, 0xff, 0xaa, 0x00, /* 11000110 */
        0xaa, 0xff, 0x00, 0x00, 0x00, 0xff, 0xaa, 0x00, /* 11000110 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 00000000 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 00000000 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 00000000 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 00000000 */
    }
};


static bool
dump(nsfb_t *nsfb, const char *filename)
{
    int fd;

    if (filename  == NULL)
	return false;

    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    if (fd < 0)
	return false;

    nsfb_dump(nsfb, fd);
    
    close(fd);

    return true;
}

int main(int argc, char **argv)
{
    const char *fename;
    enum nsfb_type_e fetype;
    nsfb_t *nsfb;
    nsfb_event_t event;
    int waitloop = 3;

    nsfb_bbox_t box;
    nsfb_bbox_t box2;
    nsfb_bbox_t box3;
    uint8_t *fbptr;
    int fbstride;
    int p[] = { 300,300,  350,350, 400,300, 450,250, 400,200};
    int loop;
    nsfb_plot_pen_t pen;
    const char *dumpfile = NULL;

    if (argc < 2) {
        fename="sdl";
    } else {
        fename = argv[1];
	if (argc >= 3) {
	    dumpfile = argv[2];
	}
    }

    fetype = nsfb_type_from_name(fename);
    if (fetype == NSFB_SURFACE_NONE) {
        fprintf(stderr, "Unable to convert \"%s\" to nsfb surface type\n", fename);
        return 1;
    }

    nsfb = nsfb_new(fetype);
    if (nsfb == NULL) {
        fprintf(stderr, "Unable to allocate \"%s\" nsfb surface\n", fename);
        return 2;
    }

    if (nsfb_init(nsfb) == -1) {
        fprintf(stderr, "Unable to initialise nsfb surface\n");
        nsfb_free(nsfb);
        return 4;
    }

    /* get the geometry of the whole screen */
    box.x0 = box.y0 = 0;
    nsfb_get_geometry(nsfb, &box.x1, &box.y1, NULL);

    nsfb_get_buffer(nsfb, &fbptr, &fbstride);

    /* claim the whole screen for update */
    nsfb_claim(nsfb, &box);

    /* first test, repeatedly clear the graphics area, should result in the
     * same operation as a single clear to the final colour 
     */
    for (loop = 0; loop < 256;loop++) {
        nsfb_plot_clg(nsfb, 0xffffff00 | loop);
    }

    /* draw black radial lines from the origin */
    pen.stroke_colour = 0xff000000;
    for (loop = 0; loop < box.x1; loop += 20) {
        box2 = box;
        box2.x1 = loop;
        nsfb_plot_line(nsfb, &box2, &pen);
    }
    
    /* draw blue radial lines from the bottom right */
    pen.stroke_colour = 0xffff0000;
    for (loop = 0; loop < box.x1; loop += 20) {
        box2 = box;
        box2.x0 = loop;
        nsfb_plot_line(nsfb, &box2, &pen);
    }
    
    /* draw green radial lines from the bottom left */
    pen.stroke_colour = 0xff00ff00;
    for (loop = 0; loop < box.x1; loop += 20) {
        box2.x0 = box.x0;
        box2.x1 = loop;
        box2.y0 = box.y1;
        box2.y1 = box.y0;
        nsfb_plot_line(nsfb, &box2, &pen);
    }

    /* draw red radial lines from the top right */
    pen.stroke_colour = 0xff0000ff;
    for (loop = 0; loop < box.x1; loop += 20) {
        box2.x0 = box.x1;
        box2.x1 = loop;
        box2.y0 = box.y0;
        box2.y1 = box.y1;
        nsfb_plot_line(nsfb, &box2, &pen);
    }

    /* draw an unclipped rectangle */
    box2.x0 = box2.y0 = 100;
    box2.x1 = box2.y1 = 300;

    nsfb_plot_rectangle_fill(nsfb, &box2, 0xff0000ff);

    nsfb_plot_rectangle(nsfb, &box2, 1, 0xff00ff00, false, false);

    nsfb_plot_polygon(nsfb, p, 5, 0xffff0000);

    nsfb_plot_set_clip(nsfb, &box2);

    box3.x0 = box3.y0 = 200;
    box3.x1 = box3.y1 = 400;

    nsfb_plot_rectangle_fill(nsfb, &box3, 0xff00ffff);

    nsfb_plot_rectangle(nsfb, &box3, 1, 0xffffff00, false, false);

    for (loop = 100; loop < 400;loop++) {
        nsfb_plot_point(nsfb, loop, 150, 0xffaa1111);
        nsfb_plot_point(nsfb, loop, 160, 0x99aa1111);
    }

    nsfb_plot_set_clip(nsfb, NULL);

    box3.x0 = box3.y0 = 400;
    box3.x1 = box3.y1 = 600;

    nsfb_plot_ellipse_fill(nsfb, &box3, 0xffff0000);

    nsfb_plot_ellipse(nsfb, &box3, 0xff0000ff);

    box3.x0 = 500;
    box3.x1 = 700;
    box3.y0 = 400;
    box3.y1 = 500;

    nsfb_plot_ellipse_fill(nsfb, &box3, 0xffff0000);

    nsfb_plot_ellipse(nsfb, &box3, 0xff0000ff);

    box3.x0 = 600;
    box3.x1 = 700;
    box3.y0 = 300;
    box3.y1 = 500;

    nsfb_plot_ellipse_fill(nsfb, &box3, 0xff0000ff);

    nsfb_plot_ellipse(nsfb, &box3, 0xffff0000);

    box2.x0 = 400;
    box2.y0 = 400;
    box2.x1 = 500;
    box2.y1 = 500;

    box3.x0 = 600;
    box3.y0 = 200;
    box3.x1 = 700;
    box3.y1 = 300;

    nsfb_plot_copy(nsfb, &box2, nsfb, &box3);

    /* test glyph plotting */
    for (loop = 100; loop < 200; loop+= Mglyph1.w) {
        box3.x0 = loop;
        box3.y0 = 20;
        box3.x1 = box3.x0 + Mglyph8.w;
        box3.y1 = box3.y0 + Mglyph8.h;

        nsfb_plot_glyph1(nsfb, &box3,  Mglyph1.data, Mglyph1.w, 0xff000000);
    }

    /* test glyph plotting */
    for (loop = 100; loop < 200; loop+= Mglyph8.w) {
        box3.x0 = loop;
        box3.y0 = 50;
        box3.x1 = box3.x0 + Mglyph8.w;
        box3.y1 = box3.y0 + Mglyph8.h;

        nsfb_plot_glyph8(nsfb, &box3,  Mglyph8.data, Mglyph8.w, 0xff000000);
    }

    nsfb_update(nsfb, &box);

    /* random rectangles in clipped area*/
    box2.x0 = 400;
    box2.y0 = 50;
    box2.x1 = 600;
    box2.y1 = 100;

    nsfb_plot_set_clip(nsfb, &box2);

    srand(1234);

    for (loop=0; loop < 10000; loop++) {
        nsfb_claim(nsfb, &box2);
        box3.x0 = rand() / (RAND_MAX / box.x1);
        box3.y0 = rand() / (RAND_MAX / box.y1);
        box3.x1 = rand() / (RAND_MAX / 400);
        box3.y1 = rand() / (RAND_MAX / 400);
        nsfb_plot_rectangle_fill(nsfb, &box3, 0xff000000 | rand());
        nsfb_update(nsfb, &box2);
    }

    /* wait for quit event or timeout */
    while (waitloop > 0) {
	if (nsfb_event(nsfb, &event, 1000)  == false) {
	    break;
	}
	if (event.type == NSFB_EVENT_CONTROL) {
	    if (event.value.controlcode == NSFB_CONTROL_TIMEOUT) {
		/* timeout */
		waitloop--;
	    } else if (event.value.controlcode == NSFB_CONTROL_QUIT) {
		break;
	    }
	}
    }

    dump(nsfb, dumpfile);

    nsfb_free(nsfb);

    return 0;
}

/*
 * Local variables:
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
