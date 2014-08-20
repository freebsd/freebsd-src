/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"
#include "nsfb.h"
#include "surface.h"

/* exported interface documented in libnsfb.h */
bool
nsfb_dump(nsfb_t *nsfb, int fd)
{
    FILE *outf;
    int x;
    int y;

    outf = fdopen(dup(fd), "w");
    if (outf == NULL) {
	    return false;
    }

    fprintf(outf,"P3\n#libnsfb buffer dump\n%d %d\n255\n",
	    nsfb->width, nsfb->height);
    for (y=0; y < nsfb->height; y++) {
	for (x=0; x < nsfb->width; x++) {
	    fprintf(outf,"%d %d %d ", 
		    *(nsfb->ptr + (((nsfb->width * y) + x) * 4) + 2),
		    *(nsfb->ptr + (((nsfb->width * y) + x) * 4) + 1),
		    *(nsfb->ptr + (((nsfb->width * y) + x) * 4) + 0));
	    
	}
	fprintf(outf,"\n");
    }

    fclose(outf);

    return true;
}

/*
 * Local variables:
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
