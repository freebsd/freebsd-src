/*
 * Copyright (c) 2021 Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */


#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/resource.h>

#include <devstat.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sys/queue.h>
#include <sys/sysctl.h>

#include "systat.h"
#include "extern.h"
#include "devs.h"

#define CAM_BASE "kern.cam"
#define LATENCY ".latencies"
#define CAM_IOSCHED_BASE "kern.cam.iosched.bucket_base_us"

#define DEV_NAMSIZE	32
#define OP_NAMSIZE	16
#define MAX_LATS	32

static double high_thresh = 500;
static double med_thresh = 300;
static bool docolor = true;

static int ndevs;
static SLIST_HEAD(, iosched_stat)	curlist;

struct iosched_op_stat {
	int		nlats;
	uint64_t	lats[MAX_LATS];
	uint64_t	prev_lats[MAX_LATS];
};

enum { OP_READ = 0, OP_WRITE, OP_TRIM, NUM_OPS };
static const char *ops[NUM_OPS] = { "read", "write", "trim" };
#define OP_READ_MASK (1 << OP_READ)
#define OP_WRITE_MASK (1 << OP_WRITE)
#define OP_TRIM_MASK (1 << OP_TRIM)

static uint32_t flags = OP_READ_MASK | OP_WRITE_MASK | OP_TRIM_MASK;

struct iosched_stat {
	SLIST_ENTRY(iosched_stat)	 link;
	char		dev_name[DEV_NAMSIZE];
	int		unit;
	struct iosched_op_stat op_stats[NUM_OPS];
};

static int	name2oid(const char *, int *);
static int	walk_sysctl(int *, size_t);

static int
name2oid(const char *name, int *oidp)
{
	int oid[2];
	int i;
	size_t j;

	oid[0] = CTL_SYSCTL;
	oid[1] = CTL_SYSCTL_NAME2OID;

	j = CTL_MAXNAME * sizeof(int);
	i = sysctl(oid, 2, oidp, &j, name, strlen(name));
	if (i < 0)
		return (i);
	j /= sizeof(int);
	return (j);
}

static size_t /* Includes the trailing NUL */
oid2name(int *oid, size_t nlen, char *name, size_t namlen)
{
	int qoid[CTL_MAXNAME + 2];
	int i;
	size_t j;

	bzero(name, namlen);
	qoid[0] = CTL_SYSCTL;
	qoid[1] = CTL_SYSCTL_NAME;
	memcpy(qoid + 2, oid, nlen * sizeof(int));
	j = namlen;
	i = sysctl(qoid, nlen + 2, name, &j, 0, 0);
	if (i || !j)
		err(1, "sysctl name %d %zu %d", i, j, errno);
	return (j);
}

static int
oidfmt(int *oid, int len, u_int *kind)
{
	int qoid[CTL_MAXNAME+2];
	u_char buf[BUFSIZ];
	int i;
	size_t j;

	qoid[0] = CTL_SYSCTL;
	qoid[1] = CTL_SYSCTL_OIDFMT;
	memcpy(qoid + 2, oid, len * sizeof(int));

	j = sizeof(buf);
	i = sysctl(qoid, len + 2, buf, &j, 0, 0);
	if (i)
		err(1, "sysctl fmt %d %zu %d", i, j, errno);
	*kind = *(u_int *)buf;
	return (0);
}

static int
split_u64(char *str, const char *delim, uint64_t *buckets, int *nbuckets)
{
	int n = *nbuckets, i;
	char *v;

	memset(buckets, 0, n * sizeof(buckets[0]));
	for (i = 0; (v = strsep(&str, delim)) != NULL && i < n; i++) {
		buckets[i] = strtoull(v, NULL, 10);
	}
	if (i < n)
		*nbuckets = i;
	return (i < n);
}

static double baselat = 0.000020;

static float
pest(int permill, uint64_t *lats, int nlat)
{
	uint64_t tot, samp;
	int i;
	float b1, b2;

	for (tot = 0, i = 0; i < nlat; i++)
		tot += lats[i];
	if (tot == 0)
		return -nanf("");
	if (tot < (uint64_t)2000 / (1000 - permill))
		return nanf("");
	samp = tot * permill / 1000;
	if (samp < lats[0])
		return baselat * (float)samp / lats[0]; /* linear interpolation 0 and baselat */
	for (tot = 0, i = 0; samp >= tot && i < nlat; i++)
		tot += lats[i];
	i--;
	b1 = baselat * (1 << (i - 1));
	b2 = baselat * (1 << i);
	/* Should expoentially interpolate between buckets -- doing linear instead */
	return b1 + (b2 - b1) * (float)(lats[i] - (tot - samp)) / lats[i];
}

static int
op2num(const char *op)
{
	for (int i = 0; i < NUM_OPS; i++)
		if (strcmp(op, ops[i]) == 0)
			return i;
	return -1;
}

static struct iosched_op_stat *
find_dev(const char *dev, int unit, int op)
{
	struct iosched_stat *isp;
	struct iosched_op_stat *iosp;

	SLIST_FOREACH(isp, &curlist, link) {
		if (strcmp(isp->dev_name, dev) != 0 || isp->unit != unit)
			continue;
		iosp = &isp->op_stats[op];
		return iosp;
	}
	return NULL;
}

static struct iosched_op_stat *
alloc_dev(const char *dev, int unit, int op)
{
	struct iosched_stat *isp;
	struct iosched_op_stat *iosp;

	isp = malloc(sizeof(*isp));
	if (isp == NULL)
		return NULL;
	strlcpy(isp->dev_name, dev, sizeof(isp->dev_name));
	isp->unit = unit;
	SLIST_INSERT_HEAD(&curlist, isp, link);
	ndevs++;
	iosp = &isp->op_stats[op];
	return iosp;
}

#define E3 1000.0
static void
update_dev(const char *dev, int unit, int op, uint64_t *lats, int nlat)
{
	struct iosched_op_stat *iosp;

	iosp = find_dev(dev, unit, op);
	if (iosp == NULL)
		iosp = alloc_dev(dev, unit, op);
	if (iosp == NULL)
		return;
	iosp->nlats = nlat;
	memcpy(iosp->prev_lats, iosp->lats, iosp->nlats * sizeof(uint64_t));
	memcpy(iosp->lats, lats, iosp->nlats * sizeof(uint64_t));
//	printf("%s%d: %-6s %.3f %.3f %.3f %.3f\r\n",
//	    dev, unit, operation, E3 * pest(500, lats, nlat), E3 * pest(900, lats, nlat),
//	    E3 * pest(990, lats, nlat), E3 * pest(999, lats, nlat));
}

static int
walk_sysctl(int *base_oid, size_t len)
{
	int qoid[CTL_MAXNAME + 2], oid[CTL_MAXNAME];
	size_t l1, l2;
	char name[BUFSIZ];

	if (len > CTL_MAXNAME)
		err(1, "Length %zd too long", len);

	qoid[0] = CTL_SYSCTL;
	qoid[1] = CTL_SYSCTL_NEXT;
	l1 = 2;
	memcpy(qoid + 2, base_oid, len * sizeof(int));
	l1 += len;
	for (;;) {
		/*
		 * Get the next one or return when we get to the end of the
		 * sysctls in the kernel.
		 */
		l2 = sizeof(oid);
		if (sysctl(qoid, l1, oid, &l2, 0, 0) != 0) {
			if (errno == ENOENT)
				return (0);
			err(1, "sysctl(getnext) %zu", l2);
		}

		l2 /= sizeof(int);

		/*
		 * Bail if we're seeing OIDs that don't have the
		 * same prefix or can't have the same prefix.
		 */
		if (l2 < len ||
		    memcmp(oid, base_oid, len * sizeof(int)) != 0)
			return (0);

		/*
		 * Get the name, validate it's one we're looking for,
		 * parse the latency and add to list.
		 */
		do {
			int nlat;
			size_t l3;
			char val[BUFSIZ];
			char *walker, *dev, *opstr;
			uint64_t latvals[MAX_LATS];
			u_int kind;
			int unit, op;

			l1 = oid2name(oid, l2, name, sizeof(name));
			if (strcmp(name + l1 - strlen(LATENCY) - 1, LATENCY) != 0)
				break;
			if (oidfmt(oid, l2, &kind) != 0)
				err(1, "oidfmt");
			if ((kind & CTLTYPE) != CTLTYPE_STRING)
				errx(1, "string");
			l3 = sizeof(val);
			if (sysctl(oid, l2, val, &l3, 0, 0) != 0)
				err(1, "sysctl");
			val[l3] = '\0';
			nlat = nitems(latvals);
			if (split_u64(val, ",", latvals, &nlat) == 0)
				break;
			walker = name + strlen(CAM_BASE) + 1;
			dev = strsep(&walker, ".");
			unit = (int)strtol(strsep(&walker, "."), NULL, 10);
			strsep(&walker, ".");
			opstr = strsep(&walker, ".");
			op = op2num(opstr);
			if (op < 0)
				break;
			update_dev(dev, unit, op, latvals, nlat);
		} while (false);

		memcpy(qoid + 2, oid, l2 * sizeof(int));
		l1 = 2 + l2;
	}
}

void
closeiolat(WINDOW *w)
{
	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

static void
doublecmd(const char *cmd, double *v)
{
	const char *p;
	double tv;

	p = strchr(cmd, '=');
	if (p == NULL)
		return;	/* XXX Tell the user something? */
	if (sscanf(p + 1, "%lf", &tv) != 1)
		return;	/* XXX Tell the user something? */
	*v = tv;
}

int
cmdiolat(const char *cmd __unused, const char *args __unused)
{
	fprintf(stderr, "CMD IS '%s'\n\n", cmd);
	if (prefix(cmd, "trim"))
		flags ^= OP_TRIM_MASK;
	else if (prefix(cmd, "read"))
		flags ^= OP_READ_MASK;
	else if (prefix(cmd, "write"))
		flags ^= OP_WRITE_MASK;
	else if (prefix(cmd, "color"))
		docolor = !docolor;
	else if (prefix("high", cmd))
		doublecmd(cmd, &high_thresh);
	else if (prefix("med", cmd))
		doublecmd(cmd, &med_thresh);
	else
		return (0);
	wclear(wnd);
	labeliolat();
	refresh();
	return (1);
}

int
initiolat(void)
{
	int cam[CTL_MAXNAME];
	uint64_t sbt_base;
	size_t len = sizeof(sbt_base);

	SLIST_INIT(&curlist);

	baselat = 1e-3;		/* old default */
	if (sysctlbyname(CAM_IOSCHED_BASE, &sbt_base, &len, NULL, 0) == 0)
		baselat = sbt_base * 1e-6;	/* Convert to microseconds */

	name2oid(CAM_BASE, cam);
	walk_sysctl(cam, 2);
	return (1);
}

void
fetchiolat(void)
{
	int cam[CTL_MAXNAME];

	name2oid(CAM_BASE, cam);
	walk_sysctl(cam, 2);
}

#define	INSET	10

void
labeliolat(void)
{
	int _col, ndrives, lpr, row, j;
	int regions __unused;
	struct iosched_stat *isp;
	char tmpstr[32];
#define COLWIDTH	29
#define DRIVESPERLINE	((getmaxx(wnd) - 1 - INSET) / COLWIDTH)
	ndrives = ndevs; // XXX FILTER XXX
	regions = howmany(ndrives, DRIVESPERLINE);
	lpr = 2; /* for headers */
	for (int i = 0; i < NUM_OPS; i++) {
		if (flags & (1 << i))
			lpr++;
	}
	row = 0;
	_col = INSET;
	j = 2;
	if (flags & OP_READ_MASK)
		mvwaddstr(wnd, row + j++, 1, "read");
	if (flags & OP_WRITE_MASK)
		mvwaddstr(wnd, row + j++, 1, "write");
	if (flags & OP_TRIM_MASK)
		mvwaddstr(wnd, row + j++, 1, "trim");
	SLIST_FOREACH(isp, &curlist, link) {
		if (_col + COLWIDTH >= getmaxx(wnd) - 1 - INSET) {
			_col = INSET;
			row += lpr + 1;
			if (row > getmaxy(wnd) - 1 - (lpr + 1))
				break;
			j = 2;
			if (flags & OP_READ_MASK)
				mvwaddstr(wnd, row + j++, 1, "read");
			if (flags & OP_WRITE_MASK)
				mvwaddstr(wnd, row + j++, 1, "write");
			if (flags & OP_TRIM_MASK)
				mvwaddstr(wnd, row + j++, 1, "trim");
		}
		snprintf(tmpstr, sizeof(tmpstr), "%s%d", isp->dev_name, isp->unit);
		mvwaddstr(wnd, row, _col + (COLWIDTH - strlen(tmpstr)) / 2, tmpstr);
		mvwaddstr(wnd, row + 1, _col, "   p50    p90    p99  p99.9");
		_col += COLWIDTH;
	}
}

WINDOW *
openiolat(void)
{
	return (subwin(stdscr, LINES-3-1, 0, MAINWIN_ROW, 0));
}

static void
fmt(float f, char *buf, size_t len)
{
	if (isnan(f))
		strlcpy(buf, "   -  ", len);
	else if (f >= 1000.0)
		snprintf(buf, len, "%6d", (int)f);
	else if (f >= 100.0)
		snprintf(buf, len, "%6.1f", f);
	else if (f >= 10.0)
		snprintf(buf, len, "%6.2f", f);
	else
		snprintf(buf, len, "%6.3f", f);
}

static void
latout(double lat, int y, int x)
{
	int i;
	char tmpstr[32];

	fmt(lat, tmpstr, sizeof(tmpstr));
	if (isnan(lat))
		i = 4;
	else if (lat > high_thresh)
		i = 3;
	else if (lat > med_thresh)
		i = 2;
	else
		i = 1;
	if (docolor)
		wattron(wnd, COLOR_PAIR(i));
	mvwaddstr(wnd, y, x, tmpstr);
	if (docolor)
		wattroff(wnd, COLOR_PAIR(i));
}

void
showiolat(void)
{
	int _col, ndrives, lpr, row, k;
	int regions __unused;
	struct iosched_stat *isp;
	struct iosched_op_stat *iosp;
#define COLWIDTH	29
#define DRIVESPERLINE	((getmaxx(wnd) - 1 - INSET) / COLWIDTH)
	ndrives = ndevs; // XXX FILTER XXX
	regions = howmany(ndrives, DRIVESPERLINE);
	lpr = 2; /* XXX */
	for (int i = 0; i < NUM_OPS; i++) {
		if (flags & (1 << i))
			lpr++;
	}
	row = 0;
	_col = INSET;
	SLIST_FOREACH(isp, &curlist, link) {
		if (_col + COLWIDTH >= getmaxx(wnd) - 1 - INSET) {
			_col = INSET;
			row += lpr + 1;
			if (row > getmaxy(wnd) - 1 - (lpr + 1))
				break;
		}
		k = 2;
		for (int i = 0; i < NUM_OPS; i++) {
			uint64_t lats[MAX_LATS];
			int nlats;
			float p50, p90, p99, p999;

			if ((flags & (1 << i)) == 0)
				continue;
			iosp = &isp->op_stats[i];
			nlats = iosp->nlats;
			memset(lats, 0, sizeof(lats));
			for (int j = 0; j < iosp->nlats; j++)
				lats[j] = iosp->lats[j] - iosp->prev_lats[j];
			p50 = pest(500, lats, nlats) * E3;
			p90 = pest(900, lats, nlats) * E3;
			p99 = pest(990, lats, nlats) * E3;
			p999 = pest(999, lats, nlats) * E3;
			latout(p50, row + k, _col);
			latout(p90, row + k, _col + 7);
			latout(p99, row + k, _col + 14);
			latout(p999, row + k, _col + 21);
			k++;
		}
		_col += COLWIDTH;
	}
}
