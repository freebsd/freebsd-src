/* libnsfb ploygon plotter test program */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

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
    uint8_t *fbptr;
    int fbstride;

    int sides;
    int radius;
    nsfb_point_t *points;
    int loop;
//    nsfb_plot_pen_t pen;

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

    radius = (box.x1 / 3);

    for (sides = 13; sides >=3; sides--) {
	    points = malloc(sizeof(nsfb_point_t) * sides);

	    for (loop = 0; loop < sides;loop++) {
		    points[loop].x = (box.x1 / 2) + 
			    (radius * cos(loop * 2 * M_PI / sides));
		    points[loop].y = (box.y1 / 2) + 
			    (radius * sin(loop * 2 * M_PI / sides));
	    }

	    nsfb_plot_polygon(nsfb, (const int *)points, sides, 
			      0xff000000 | (0xffffff / (sides * 2)));

	    free(points);
	    radius -= 25;
    }

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
