#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "libnsfb.h"
#include "libnsfb_event.h"

int main(int argc, char **argv)
{
    nsfb_t *nsfb;
    const char *fename;
    enum nsfb_type_e fetype;
    nsfb_event_t event;

    int waitloop = 3;

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

    if (nsfb_set_geometry(nsfb, 0, 0, NSFB_FMT_ANY) == -1) {
        fprintf(stderr, "Unable to set surface geometry\n");
        nsfb_free(nsfb);
        return 3;
    }

    if (nsfb_init(nsfb) == -1) {
        fprintf(stderr, "Unable to initialise nsfb surface\n");
        nsfb_free(nsfb);
        return 4;
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

    nsfb_free(nsfb);
    return 0;
}

/*
 * Local variables:
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
