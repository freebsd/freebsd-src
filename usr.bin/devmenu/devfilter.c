/*
 * Copyright 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/devconf.h>
#include <sys/proc.h>
#include <vm/vm.h>
#include <sys/sysctl.h>
#include <err.h>
#include <sysexits.h>

#include "devmenu.h"

char *
devmenu_toname(const struct devconf *dev)
{
	static char buf[2*MAXDEVNAME];
	snprintf(buf, sizeof buf, "%s%d", dev->dc_name, dev->dc_unit);
	return buf;
}

int
devmenu_filter(const struct devconf *dev, char **userlist)
{
	int i;
	char *name;

	if (!userlist)
		return 1;

	name = devmenu_toname(dev);

	for (i = 0; userlist[i]; i++) {
		if (strcmp(name, userlist[i]) == 0) {
			return 1;
		}
	}

	return 0;
}

struct devconf **
devmenu_alldevs(void)
{
	int mib[3];
	size_t size;
	int ndevs, i, ndx;
	struct devconf **rv;

	size = sizeof ndevs;
	mib[0] = CTL_HW;
	mib[1] = HW_DEVCONF;
	mib[2] = DEVCONF_NUMBER;

	if (sysctl(mib, 3, &ndevs, &size, 0, 0) < 0) {
		err(EX_OSERR, "sysctl failed reading hw.devconf.number");
	}

	rv = malloc((ndevs + 1) * sizeof *rv);
	if (!rv) {
		err(EX_UNAVAILABLE, "malloc(%lu)", 
		    (unsigned long)(ndevs * sizeof *rv));
	}

	for (ndx = 0, i = 1; i <= ndevs; i++) {
		mib[2] = i;
		if (sysctl(mib, 3, 0, &size, 0, 0) < 0) {
			continue;
		}

		rv[ndx] = malloc(size);
		if (!rv[ndx]) {
			err(EX_UNAVAILABLE, "malloc(%lu)", 
			    (unsigned long)size);
		}

		if (sysctl(mib, 3, rv[ndx], &size, 0, 0) < 0) {
			err(EX_OSERR, "sysctl reading hw.devconf.%d", i);
		}

		ndx++;
	}

	rv[ndx] = 0;

	return rv;
}

void
devmenu_freedevs(struct devconf ***devpp)
{
	struct devconf **devp = *devpp;
	int i;

	for (i = 0; devp[i]; i++) {
		free(devp[i]);
	}

	free(devp);
	*devpp = 0;
}

const char *
devmenu_common(const char *title, const char *hfile, char **devnames,
	       const char *prompt, const char *none, enum dc_class class,
	       int states)
{
	struct devconf **devs;
	int s;
	unsigned char **items = 0;
	int nitems = 0;
	int itemindex = 0;
	char *name;
	int i;
	static char resbuf[2*MAXDEVNAME];

	if (hfile) {
		use_helpfile((char *)hfile);
	}
	devs = devmenu_alldevs();

	for (i = 0; devs[i]; i++) {
		if (states && !((1 << devs[i]->dc_state) & states))
			continue;
		if (class && !(devs[i]->dc_class & class))
			continue;
		if (devmenu_filter(devs[i], devnames)) {
			++nitems;
			items = realloc(items, 2 * nitems * sizeof *items);
			if (!items) {
				err(EX_UNAVAILABLE, "malloc(%lu)",
				    (unsigned long)(2 * nitems * sizeof *items));
			}

			name = devmenu_toname(devs[i]);

			items[itemindex] = strdup(name);
			if (!items[itemindex]) {
				err(EX_UNAVAILABLE, "strdup-malloc(%lu)",
				    (unsigned long)(strlen(name) + 1));
			}

			items[itemindex + 1] = devs[i]->dc_descr;
			itemindex += 2;
		}
	}

	if (!nitems) {
		dialog_msgbox((char *)title, none, -1, -1, 1);
		return "none";
	}

	name = resbuf;

	if(dialog_menu((char *)title, prompt, 24, 78, nitems, nitems, items,
		       resbuf, 0, 0) != 0) {
		name = "none";
	} 

	for (i = 0; i < 2 * nitems; i += 2) {
		free(items[i]);
	}

	free(items);

	devmenu_freedevs(&devs);
	return name;
}
