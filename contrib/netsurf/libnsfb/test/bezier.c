/* libnsfb plotetr test program */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"

#define UNUSED(x) ((x) = (x))


int main(int argc, char **argv)
{
    const char *fename;
    enum nsfb_type_e fetype;
    nsfb_t *nsfb;
    nsfb_event_t event;
    int waitloop = 3;

    nsfb_bbox_t box;
    nsfb_bbox_t box2;
    uint8_t *fbptr;
    int fbstride;
    nsfb_point_t ctrla;
    nsfb_point_t ctrlb;
    int loop;
    nsfb_plot_pen_t pen;

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

    box2.x0=100;
    box2.y0=100;

    box2.x1=400;
    box2.y1=400;

    pen.stroke_colour = 0xff000000;
    pen.fill_colour = 0xffff0000;
    pen.stroke_type = NFSB_PLOT_OPTYPE_SOLID;
    pen.fill_type = NFSB_PLOT_OPTYPE_NONE;

    for (loop=-300;loop < 600;loop+=100) {
        ctrla.x = 100;
        ctrla.y = loop;

        ctrlb.x = 400;
        ctrlb.y = 500 - loop;

        nsfb_plot_cubic_bezier(nsfb, &box2, &ctrla, &ctrlb, &pen);
    }


    box2.x0=400;
    box2.y0=100;

    box2.x1=600;
    box2.y1=400;

    nsfb_plot_line(nsfb, &box2, &pen);

    box2.x0=800;
    box2.y0=100;

    box2.x1=600;
    box2.y1=400;

    nsfb_plot_line(nsfb, &box2, &pen);

    box2.x0=400;
    box2.y0=100;

    box2.x1=800;
    box2.y1=100;

    ctrla.x = 600;
    ctrla.y = 400;

    pen.stroke_colour = 0xffff0000;

    nsfb_plot_cubic_bezier(nsfb, &box2, &ctrla, &ctrla, &pen);

    box2.x0=400;
    box2.y0=100;

    box2.x1=800;
    box2.y1=100;

    ctrla.x = 600;
    ctrla.y = 400;

    pen.stroke_colour = 0xff0000ff;

    nsfb_plot_quadratic_bezier(nsfb, &box2, &ctrla, &pen);

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
