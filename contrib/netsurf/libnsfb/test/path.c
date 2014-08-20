/* libnsfb plotetr test program */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"

#define UNUSED(x) ((x) = (x))

#define PENT(op, xco, yco) path[count].operation = op;  \
    path[count].point.x = (xco);                        \
    path[count].point.y = (yco);                        \
    count++

static int fill_shape(nsfb_plot_pathop_t *path, int xoff, int yoff) 
{
    int count = 0;

    PENT(NFSB_PLOT_PATHOP_MOVE, xoff, yoff);
    PENT(NFSB_PLOT_PATHOP_LINE, xoff + 100, yoff + 100);
    PENT(NFSB_PLOT_PATHOP_LINE, xoff + 100, yoff );
    PENT(NFSB_PLOT_PATHOP_LINE, xoff + 200, yoff + 100);
    PENT(NFSB_PLOT_PATHOP_MOVE, xoff + 200, yoff - 200);
    PENT(NFSB_PLOT_PATHOP_MOVE, xoff + 300, yoff + 300);
    PENT(NFSB_PLOT_PATHOP_CUBIC, xoff + 300, yoff );
    PENT(NFSB_PLOT_PATHOP_LINE, xoff + 400, yoff + 100);
    PENT(NFSB_PLOT_PATHOP_LINE, xoff + 400, yoff );
    PENT(NFSB_PLOT_PATHOP_MOVE, xoff + 500, yoff + 200);
    PENT(NFSB_PLOT_PATHOP_QUAD, xoff + 500, yoff );
    PENT(NFSB_PLOT_PATHOP_LINE, xoff + 600, yoff + 150);
    PENT(NFSB_PLOT_PATHOP_LINE, xoff, yoff + 150);
    PENT(NFSB_PLOT_PATHOP_LINE, xoff, yoff);

    return count;
}

int main(int argc, char **argv)
{
    const char *fename;
    enum nsfb_type_e fetype;
    nsfb_t *nsfb;

    int waitloop = 3;

    nsfb_event_t event;
    nsfb_bbox_t box;
    uint8_t *fbptr;
    int fbstride;
    nsfb_plot_pen_t pen;
    nsfb_plot_pathop_t path[20];

    if (argc < 2) {
        fename="sdl";
    } else {
        fename = argv[1];
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

    nsfb_plot_clg(nsfb, 0xffffffff);

    pen.stroke_colour = 0xff0000ff;
    pen.fill_colour = 0xffff0000;
    pen.stroke_type = NFSB_PLOT_OPTYPE_SOLID;
    pen.fill_type = NFSB_PLOT_OPTYPE_NONE;

    nsfb_plot_path(nsfb, fill_shape(path, 100, 50), path, &pen);

    pen.fill_type = NFSB_PLOT_OPTYPE_SOLID;

    nsfb_plot_path(nsfb, fill_shape(path, 100, 200), path, &pen);

    pen.stroke_type = NFSB_PLOT_OPTYPE_NONE;

    nsfb_plot_path(nsfb, fill_shape(path, 100, 350), path, &pen);

    nsfb_update(nsfb, &box);

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

    nsfb_free(nsfb);

    return 0;
}

/*
 * Local variables:
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
