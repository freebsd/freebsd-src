/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)subr_autoconf.c	8.1 (Berkeley) 6/10/93
 *
 * from: $Header: subr_autoconf.c,v 1.12 93/02/01 19:31:48 torek Exp $ (LBL)
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>

/*
 * Autoconfiguration subroutines.
 */

/*
 * ioconf.c exports exactly two names: cfdata and cfroots.  All system
 * devices and drivers are found via these tables.
 */
extern struct cfdata cfdata[];
extern short cfroots[];

#define	ROOT ((struct device *)NULL)

struct matchinfo {
	cfmatch_t fn;
	struct	device *parent;
	void	*aux;
	struct	cfdata *match;
	int	pri;
};

/*
 * Apply the matching function and choose the best.  This is used
 * a few times and we want to keep the code small.
 */
static void
mapply(m, cf)
	register struct matchinfo *m;
	register struct cfdata *cf;
{
	register int pri;

	if (m->fn != NULL)
		pri = (*m->fn)(m->parent, cf, m->aux);
	else
		pri = (*cf->cf_driver->cd_match)(m->parent, cf, m->aux);
	if (pri > m->pri) {
		m->match = cf;
		m->pri = pri;
	}
}

/*
 * Iterate over all potential children of some device, calling the given
 * function (default being the child's match function) for each one.
 * Nonzero returns are matches; the highest value returned is considered
 * the best match.  Return the `found child' if we got a match, or NULL
 * otherwise.  The `aux' pointer is simply passed on through.
 *
 * Note that this function is designed so that it can be used to apply
 * an arbitrary function to all potential children (its return value
 * can be ignored).
 */
struct cfdata *
config_search(fn, parent, aux)
	cfmatch_t fn;
	register struct device *parent;
	void *aux;
{
	register struct cfdata *cf;
	register short *p;
	struct matchinfo m;

	m.fn = fn;
	m.parent = parent;
	m.aux = aux;
	m.match = NULL;
	m.pri = 0;
	for (cf = cfdata; cf->cf_driver; cf++) {
		/*
		 * Skip cf if no longer eligible, otherwise scan through
		 * parents for one matching `parent', and try match function.
		 */
		if (cf->cf_fstate == FSTATE_FOUND)
			continue;
		for (p = cf->cf_parents; *p >= 0; p++)
			if (parent->dv_cfdata == &cfdata[*p])
				mapply(&m, cf);
	}
	return (m.match);
}

/*
 * Find the given root device.
 * This is much like config_search, but there is no parent.
 */
struct cfdata *
config_rootsearch(fn, rootname, aux)
	register cfmatch_t fn;
	register char *rootname;
	register void *aux;
{
	register struct cfdata *cf;
	register short *p;
	struct matchinfo m;

	m.fn = fn;
	m.parent = ROOT;
	m.aux = aux;
	m.match = NULL;
	m.pri = 0;
	/*
	 * Look at root entries for matching name.  We do not bother
	 * with found-state here since only one root should ever be
	 * searched (and it must be done first).
	 */
	for (p = cfroots; *p >= 0; p++) {
		cf = &cfdata[*p];
		if (strcmp(cf->cf_driver->cd_name, rootname) == 0)
			mapply(&m, cf);
	}
	return (m.match);
}

static char *msgs[3] = { "", " not configured\n", " unsupported\n" };

/*
 * The given `aux' argument describes a device that has been found
 * on the given parent, but not necessarily configured.  Locate the
 * configuration data for that device (using the cd_match configuration
 * driver function) and attach it, and return true.  If the device was
 * not configured, call the given `print' function and return 0.
 */
int
config_found(parent, aux, print)
	struct device *parent;
	void *aux;
	cfprint_t print;
{
	struct cfdata *cf;

	if ((cf = config_search((cfmatch_t)NULL, parent, aux)) != NULL) {
		config_attach(parent, cf, aux, print);
		return (1);
	}
	printf(msgs[(*print)(aux, parent->dv_xname)]);
	return (0);
}

/*
 * As above, but for root devices.
 */
int
config_rootfound(rootname, aux)
	char *rootname;
	void *aux;
{
	struct cfdata *cf;

	if ((cf = config_rootsearch((cfmatch_t)NULL, rootname, aux)) != NULL) {
		config_attach(ROOT, cf, aux, (cfprint_t)NULL);
		return (1);
	}
	printf("root device %s not configured\n", rootname);
	return (0);
}

/* just like sprintf(buf, "%d") except that it works from the end */
static char *
number(ep, n)
	register char *ep;
	register int n;
{

	*--ep = 0;
	while (n >= 10) {
		*--ep = (n % 10) + '0';
		n /= 10;
	}
	*--ep = n + '0';
	return (ep);
}

/*
 * Attach a found device.  Allocates memory for device variables.
 */
void
config_attach(parent, cf, aux, print)
	register struct device *parent;
	register struct cfdata *cf;
	register void *aux;
	cfprint_t print;
{
	register struct device *dev;
	register struct cfdriver *cd;
	register size_t lname, lunit;
	register char *xunit;
	int myunit;
	char num[10];
	static struct device **nextp = &alldevs;

	cd = cf->cf_driver;
	if (cd->cd_devsize < sizeof(struct device))
		panic("config_attach");
	myunit = cf->cf_unit;
	if (cf->cf_fstate == FSTATE_NOTFOUND)
		cf->cf_fstate = FSTATE_FOUND;
	else
		cf->cf_unit++;

	/* compute length of name and decimal expansion of unit number */
	lname = strlen(cd->cd_name);
	xunit = number(&num[sizeof num], myunit);
	lunit = &num[sizeof num] - xunit;
	if (lname + lunit >= sizeof(dev->dv_xname))
		panic("config_attach: device name too long");

	/* get memory for all device vars */
	dev = (struct device *)malloc(cd->cd_devsize, M_DEVBUF, M_WAITOK);
					/* XXX cannot wait! */
	bzero(dev, cd->cd_devsize);
	*nextp = dev;			/* link up */
	nextp = &dev->dv_next;
	dev->dv_class = cd->cd_class;
	dev->dv_cfdata = cf;
	dev->dv_unit = myunit;
	bcopy(cd->cd_name, dev->dv_xname, lname);
	bcopy(xunit, dev->dv_xname + lname, lunit);
	dev->dv_parent = parent;
	if (parent == ROOT)
		printf("%s (root)", dev->dv_xname);
	else {
		printf("%s at %s", dev->dv_xname, parent->dv_xname);
		(void) (*print)(aux, (char *)0);
	}

	/* put this device in the devices array */
	if (dev->dv_unit >= cd->cd_ndevs) {
		/*
		 * Need to expand the array.
		 */
		int old = cd->cd_ndevs, oldbytes, new, newbytes;
		void **nsp;

		if (old == 0) {
			nsp = malloc(MINALLOCSIZE, M_DEVBUF, M_WAITOK);	/*XXX*/
			bzero(nsp, MINALLOCSIZE);
			cd->cd_ndevs = MINALLOCSIZE / sizeof(void *);
		} else {
			new = cd->cd_ndevs;
			do {
				new *= 2;
			} while (new <= dev->dv_unit);
			cd->cd_ndevs = new;
			oldbytes = old * sizeof(void *);
			newbytes = new * sizeof(void *);
			nsp = malloc(newbytes, M_DEVBUF, M_WAITOK);	/*XXX*/
			bcopy(cd->cd_devs, nsp, oldbytes);
			bzero(&nsp[old], newbytes - oldbytes);
			free(cd->cd_devs, M_DEVBUF);
		}
		cd->cd_devs = nsp;
	}
	if (cd->cd_devs[dev->dv_unit])
		panic("config_attach: duplicate %s", dev->dv_xname);
	cd->cd_devs[dev->dv_unit] = dev;

	/*
	 * Before attaching, clobber any unfound devices that are
	 * otherwise identical.
	 */
	for (cf = cfdata; cf->cf_driver; cf++)
		if (cf->cf_driver == cd && cf->cf_unit == dev->dv_unit &&
		    cf->cf_fstate == FSTATE_NOTFOUND)
			cf->cf_fstate = FSTATE_FOUND;
	(*cd->cd_attach)(parent, dev, aux);
}

/*
 * Attach an event.  These must come from initially-zero space (see
 * commented-out assignments below), but that occurs naturally for
 * device instance variables.
 */
void
evcnt_attach(dev, name, ev)
	struct device *dev;
	const char *name;
	struct evcnt *ev;
{
	static struct evcnt **nextp = &allevents;

#ifdef DIAGNOSTIC
	if (strlen(name) >= sizeof(ev->ev_name))
		panic("evcnt_attach");
#endif
	/* ev->ev_next = NULL; */
	ev->ev_dev = dev;
	/* ev->ev_count = 0; */
	strcpy(ev->ev_name, name);
	*nextp = ev;
	nextp = &ev->ev_next;
}
