/*
 * Copyright 1996 Massachusetts Institute of Technology
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

/*
 * Second attempt at a `tzmenu' program, using the separate description
 * files provided in newer tzdata releases.
 */

/*
 * When making changes to parser code, run baseline target, check that there are
 * no unintended changes and commit updated file.
 */

#include <sys/cdefs.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#ifdef HAVE_BSDDIALOG
#include <bsddialog.h>
#include <locale.h>
#endif

#define	_PATH_ZONETAB		"/usr/share/zoneinfo/zone1970.tab"
#define	_PATH_ISO3166		"/usr/share/misc/iso3166"
#define	_PATH_ZONEINFO		"/usr/share/zoneinfo"
#define	_PATH_LOCALTIME		"/etc/localtime"
#define	_PATH_DB		"/var/db/zoneinfo"
#define	_PATH_WALL_CMOS_CLOCK	"/etc/wall_cmos_clock"

#ifdef PATH_MAX
#define	SILLY_BUFFER_SIZE	(2 * PATH_MAX)
#else
#warning "Somebody needs to fix this to dynamically size this buffer."
#define	SILLY_BUFFER_SIZE	2048
#endif

/* special return codes for `fire' actions */
#define DITEM_FAILURE           1

/* flags - returned in upper 16 bits of return status */
#define DITEM_LEAVE_MENU        (1 << 16)
#define DITEM_RECREATE          (1 << 18)

static char	path_zonetab[MAXPATHLEN], path_iso3166[MAXPATHLEN],
		path_zoneinfo[MAXPATHLEN], path_localtime[MAXPATHLEN],
		path_db[MAXPATHLEN], path_wall_cmos_clock[MAXPATHLEN];

static int reallydoit = 1;
static int reinstall = 0;
static char *chrootenv = NULL;

static void	usage(void);
static int	install_zoneinfo(const char *zoneinfo);
static void	message_zoneinfo_file(const char *title, char *prompt);
static int	install_zoneinfo_file(const char *zoneinfo_file);

#ifdef HAVE_BSDDIALOG
static struct bsddialog_conf conf;

/* for use in describing more exotic behaviors */
typedef struct dialogMenuItem {
	char *prompt;
	char *title;
	int (*fire)(struct dialogMenuItem *self);
	void *data;
} dialogMenuItem;

static int
xdialog_menu(char *title, char *cprompt, int item_no, dialogMenuItem *ditems)
{
	int i, result, menurows, choice = 0;
	struct bsddialog_menuitem *listitems;

	/* initialize list items */
	listitems = calloc(item_no + 1, sizeof(struct bsddialog_menuitem));
	if (listitems == NULL)
		errx(1, "Failed to allocate memory in xdialog_menu");
	for (i = 0; i < item_no; i++) {
		listitems[i].prefix = "";
		listitems[i].depth = 0;
		listitems[i].bottomdesc = "";
		listitems[i].on = false;
		listitems[i].name = ditems[i].prompt;
		listitems[i].desc = ditems[i].title;
	}

again:
	conf.title = title;
	menurows = item_no < 16 ? item_no : 16;
	result = bsddialog_menu(&conf, cprompt, BSDDIALOG_AUTOSIZE,
	    BSDDIALOG_AUTOSIZE, menurows, item_no, listitems, &choice);
	switch (result) {
	case BSDDIALOG_ESC:
		result = -1;
		break;
	case BSDDIALOG_OK:
		if (ditems[choice].fire != NULL) {
			int status;

			status = ditems[choice].fire(ditems + choice);
			if (status & DITEM_RECREATE) {
				goto again;
			}
		}
		result = 0;
		break;
	case BSDDIALOG_CANCEL:
	default:
		result = 1;
		break;
	}

	free(listitems);
	return (result);
}

static int usedialog = 1;

static int	confirm_zone(const char *filename);
static int	continent_country_menu(dialogMenuItem *);
static int	set_zone(dialogMenuItem *);
static int	set_zone_menu(dialogMenuItem *);
static int	set_zone_utc(void);

struct continent {
	dialogMenuItem *menu;
	int		nitems;
};

static struct continent	africa, america, antarctica, arctic, asia, atlantic;
static struct continent	australia, europe, indian, pacific, utc;

static struct continent_names {
	const char	*name;
	struct continent *continent;
} continent_names[] = {
	{ "UTC",	&utc },
	{ "Africa",	&africa },
	{ "America",	&america },
	{ "Antarctica",	&antarctica },
	{ "Arctic",	&arctic },
	{ "Asia",	&asia },
	{ "Atlantic",	&atlantic },
	{ "Australia",	&australia },
	{ "Europe",	&europe },
	{ "Indian",	&indian },
	{ "Pacific",	&pacific },
};

static struct continent_items {
	char		prompt[3];
	char		title[30];
} continent_items[] = {
	{ "0",	"UTC" },
	{ "1",	"Africa" },
	{ "2",	"America -- North and South" },
	{ "3",	"Antarctica" },
	{ "4",	"Arctic Ocean" },
	{ "5",	"Asia" },
	{ "6",	"Atlantic Ocean" },
	{ "7",	"Australia" },
	{ "8",	"Europe" },
	{ "9",	"Indian Ocean" },
	{ "10",	"Pacific Ocean" },
};

#define	NCONTINENTS	\
    (int)((sizeof(continent_items)) / (sizeof(continent_items[0])))
static dialogMenuItem continents[NCONTINENTS];

#define	OCEANP(x)	((x) == 4 || (x) == 6 || (x) == 9 || (x) == 10)

static int
continent_country_menu(dialogMenuItem *continent)
{
	char		title[64], prompt[64];
	struct continent *contp = continent->data;
	int		isocean = OCEANP(continent - continents);
	int		rv;

	if (strcmp(continent->title, "UTC") == 0)
		return (set_zone_utc());

	/* It's amazing how much good grammar really matters... */
	if (!isocean) {
		snprintf(title, sizeof(title), "Countries in %s",
		    continent->title);
		snprintf(prompt, sizeof(prompt), "Select a country or region");
	} else {
		snprintf(title, sizeof(title), "Islands and groups in the %s",
		    continent->title);
		snprintf(prompt, sizeof(prompt), "Select an island or group");
	}

	rv = xdialog_menu(title, prompt, contp->nitems, contp->menu);
	return (rv == 0 ? DITEM_LEAVE_MENU : DITEM_RECREATE);
}

static struct continent *
find_continent(int lineno, const char *name)
{
	char		*cname, *cp;
	int		i;

	/*
	 * Both normal (the ones in zone filename, e.g. Europe/Andorra) and
	 * override (e.g. Atlantic/) entries should contain '/'.
	 */
	cp = strdup(name);
	if (cp == NULL)
		err(1, "strdup");
	cname = strsep(&cp, "/");
	if (cp == NULL)
		errx(1, "%s:%d: invalid entry `%s'", path_zonetab, lineno,
		    cname);

	for (i = 0; i < NCONTINENTS; i++)
		if (strcmp(cname, continent_names[i].name) == 0) {
			free(cname);
			return (continent_names[i].continent);
		}

	errx(1, "%s:%d: continent `%s' unknown", path_zonetab, lineno, cname);
}

static const char *
find_continent_name(struct continent *cont)
{
	int		i;

	for (i = 0; i < NCONTINENTS; i++)
		if (cont == continent_names[i].continent)
			return (continent_names[i].name);
	return ("Unknown");
}

struct country {
	char		*name;
	char		*tlc;
	int		nzones;
	struct continent *override;	/* continent override */
	struct continent *alternate;	/* extra continent */
	TAILQ_HEAD(, zone) zones;
	dialogMenuItem	*submenu;
};

struct zone {
	TAILQ_ENTRY(zone) link;
	char		*descr;
	char		*filename;
	struct continent *continent;
};

/*
 * This is the easiest organization... we use ISO 3166 country codes,
 * of the two-letter variety, so we just size this array to suit.
 * Beats worrying about dynamic allocation.
 */
#define	NCOUNTRIES	(26 * 26)
static struct country countries[NCOUNTRIES];

#define	CODE2INT(s)	((s[0] - 'A') * 26 + (s[1] - 'A'))

/*
 * Read the ISO 3166 country code database in _PATH_ISO3166
 * (/usr/share/misc/iso3166).  On error, exit via err(3).
 */
static void
read_iso3166_table(void)
{
	FILE		*fp;
	struct country	*cp;
	size_t		len;
	char		*s, *t, *name;
	int		lineno;

	fp = fopen(path_iso3166, "r");
	if (!fp)
		err(1, "%s", path_iso3166);
	lineno = 0;

	while ((s = fgetln(fp, &len)) != NULL) {
		lineno++;
		if (s[len - 1] != '\n')
			errx(1, "%s:%d: invalid format", path_iso3166, lineno);
		s[len - 1] = '\0';
		if (s[0] == '#' || strspn(s, " \t") == len - 1)
			continue;

		/* Isolate the two-letter code. */
		t = strsep(&s, "\t");
		if (t == NULL || strlen(t) != 2)
			errx(1, "%s:%d: invalid format", path_iso3166, lineno);
		if (t[0] < 'A' || t[0] > 'Z' || t[1] < 'A' || t[1] > 'Z')
			errx(1, "%s:%d: invalid code `%s'", path_iso3166,
			    lineno, t);

		/* Now skip past the three-letter and numeric codes. */
		name = strsep(&s, "\t");	/* 3-let */
		if (name == NULL || strlen(name) != 3)
			errx(1, "%s:%d: invalid format", path_iso3166, lineno);
		name = strsep(&s, "\t");	/* numeric */
		if (name == NULL || strlen(name) != 3)
			errx(1, "%s:%d: invalid format", path_iso3166, lineno);

		name = s;

		cp = &countries[CODE2INT(t)];
		if (cp->name)
			errx(1, "%s:%d: country code `%s' multiply defined: %s",
			    path_iso3166, lineno, t, cp->name);
		cp->name = strdup(name);
		if (cp->name == NULL)
			errx(1, "malloc failed");
		cp->tlc = strdup(t);
		if (cp->tlc == NULL)
			errx(1, "malloc failed");
	}

	fclose(fp);
}

static struct country *
find_country(int lineno, const char *tlc)
{
	struct country	*cp;

	if (strlen(tlc) != 2 ||
	    tlc[0] < 'A' || tlc[0] > 'Z' || tlc[1] < 'A' || tlc[1] > 'Z')
		errx(1, "%s:%d: country code `%s' invalid", path_zonetab,
		    lineno, tlc);

	cp = &countries[CODE2INT(tlc)];
	if (cp->name == NULL)
		errx(1, "%s:%d: country code `%s' unknown", path_zonetab,
		    lineno, tlc);

	return (cp);
}

static void
add_cont_to_country(struct country *cp, struct continent *cont)
{
	struct zone	*zp;

	TAILQ_FOREACH(zp, &cp->zones, link) {
		if (zp->continent == cont)
			return;
	}
	cp->alternate = cont;
}

static void
add_zone_to_country(int lineno, struct country *cp, const char *descr,
    const char *file, struct continent *cont)
{
	struct zone	*zp;

	zp = malloc(sizeof(*zp));
	if (zp == NULL)
		errx(1, "malloc(%zu)", sizeof(*zp));

	if (cp->nzones == 0)
		TAILQ_INIT(&cp->zones);

	if (descr != NULL) {
		zp->descr = strdup(descr);
		if (zp->descr == NULL)
			errx(1, "malloc failed");
	} else {
		zp->descr = NULL;
	}
	zp->filename = strdup(file);
	if (zp->filename == NULL)
		errx(1, "malloc failed");
	zp->continent = cp->override != NULL ? cp->override : cont;
	TAILQ_INSERT_TAIL(&cp->zones, zp, link);
	cp->nzones++;
}

/*
 * This comparison function intentionally sorts all of the null-named
 * ``countries''---i.e., the codes that don't correspond to a real
 * country---to the end.  Everything else is lexical by country name.
 */
static int
compare_countries(const void *xa, const void *xb)
{
	const struct country *a = xa, *b = xb;

	if (a->name == 0 && b->name == 0)
		return (0);
	if (a->name == 0 && b->name != 0)
		return (1);
	if (b->name == 0)
		return (-1);

	return (strcmp(a->name, b->name));
}

/*
 * This must be done AFTER all zone descriptions are read, since it breaks
 * CODE2INT().
 */
static void
sort_countries(void)
{

	qsort(countries, NCOUNTRIES, sizeof(countries[0]), compare_countries);
}

static void
read_zones(void)
{
	FILE		*fp;
	struct continent *cont;
	struct country	*cp;
	size_t		len;
	char		*line, *country_list, *tlc, *file, *descr;
	char		*p, *q;
	int		lineno;
	int		pass = 1;

	fp = fopen(path_zonetab, "r");
	if (!fp)
		err(1, "%s", path_zonetab);

again:
	lineno = 0;
	while ((line = fgetln(fp, &len)) != NULL) {
		lineno++;
		if (line[len - 1] != '\n')
			errx(1, "%s:%d: invalid format", path_zonetab, lineno);
		line[len - 1] = '\0';

		switch (pass)
		{
		case 1:
			/*
			 * First pass: collect overrides, only looking for
			 * single continent ones for the moment.
			 *
			 * zone1970.tab introduced continent overrides in the
			 * following format:
			 *
			 *   #@TLC[,TLC...]<tab>CONTINENT/[,CONTINENT/...]
			 */
			if (strncmp(line, "#@", strlen("#@")) != 0)
				continue;
			line += 2;
			country_list = strsep(&line, "\t");
			/* Skip multi-continent overrides */
			if (strchr(line, ',') != NULL)
				continue;
			cont = find_continent(lineno, line);
			/* Parse and store overrides */
			while (country_list != NULL) {
				tlc = strsep(&country_list, ",");
				cp = find_country(lineno, tlc);
				cp->override = cont;
			}
			break;
		case 2:
			/* Second pass: parse actual data */
			if (line[0] == '#')
				continue;

			country_list = strsep(&line, "\t");
			/* coord = */ strsep(&line, "\t");	 /* Unused */
			file = strsep(&line, "\t");
			cont = find_continent(lineno, file);
			descr = (line != NULL && *line != '\0') ? line : NULL;

			while (country_list != NULL) {
				tlc = strsep(&country_list, ",");
				cp = find_country(lineno, tlc);
				add_zone_to_country(lineno, cp, descr, file,
				    cont);
			}
			break;
		case 3:
			/* Third pass: collect multi-continent overrides */
			if (strncmp(line, "#@", strlen("#@")) != 0)
				continue;
			line += 2;
			country_list = strsep(&line, "\t");
			/* Skip single-continent overrides */
			if (strchr(line, ',') == NULL)
				continue;
			while (line != NULL) {
				cont = find_continent(lineno, line);
				p = q = strdup(country_list);
				if (p == NULL)
					errx(1, "malloc failed");
				while (q != NULL) {
					tlc = strsep(&q, ",");
					cp = find_country(lineno, tlc);
					add_cont_to_country(cp, cont);
				}
				free(p);
				strsep(&line, ",");
			}
			break;
		}
	}

	if (pass++ < 3) {
		errno = 0;
		rewind(fp);
		if (errno != 0)
			err(1, "failed to rewind %s", path_zonetab);
		goto again;
	}
	fclose(fp);
}

static void
dump_zonetab(void)
{
	struct country	*cp;
	struct zone	*zp;
	const char *cont;

	for (cp = countries; cp->name != NULL; cp++) {
		printf("%s:%s\n", cp->tlc, cp->name);
		TAILQ_FOREACH(zp, &cp->zones, link) {
			cont = find_continent_name(zp->continent);
			printf("  %s:%s\n", cont, zp->filename);
		}
	}
}

static void
make_menus(void)
{
	struct country	*cp;
	struct zone	*zp, *zp2;
	struct continent *cont;
	dialogMenuItem	*dmi;
	int		i;

	/*
	 * First, count up all the countries in each continent/ocean.
	 * Be careful to count those countries which have multiple zones
	 * only once for each.  NB: some countries are in multiple
	 * continents/oceans.
	 */
	for (cp = countries; cp->name; cp++) {
		if (cp->nzones == 0)
			continue;
		TAILQ_FOREACH(zp, &cp->zones, link) {
			cont = zp->continent;
			for (zp2 = TAILQ_FIRST(&cp->zones);
			    zp2->continent != cont;
			    zp2 = TAILQ_NEXT(zp2, link))
				;
			if (zp2 == zp)
				zp->continent->nitems++;
		}

		for (i = 0; i < NCONTINENTS; i++) {
			if (cp->alternate == continent_names[i].continent) {
				continent_names[i].continent->nitems++;
			}
		}
	}

	/*
	 * Now allocate memory for the country menus and initialize
	 * continent menus.  We set nitems back to zero so that we can
	 * use it for counting again when we actually build the menus.
	 */
	memset(continents, 0, sizeof(continents));
	for (i = 0; i < NCONTINENTS; i++) {
		continent_names[i].continent->menu =
		    malloc(sizeof(dialogMenuItem) *
		    continent_names[i].continent->nitems);
		if (continent_names[i].continent->menu == NULL)
			errx(1, "malloc for continent menu");
		continent_names[i].continent->nitems = 0;
		continents[i].prompt = continent_items[i].prompt;
		continents[i].title = continent_items[i].title;
		continents[i].fire = continent_country_menu;
		continents[i].data = continent_names[i].continent;
	}

	/*
	 * Now that memory is allocated, create the menu items for
	 * each continent.  For multiple-zone countries, also create
	 * the country's zone submenu.
	 */
	for (cp = countries; cp->name; cp++) {
		if (cp->nzones == 0)
			continue;
		cp->submenu = malloc(cp->nzones * sizeof(*dmi));
		if (cp->submenu == 0)
			errx(1, "malloc for submenu");
		cp->nzones = 0;
		TAILQ_FOREACH(zp, &cp->zones, link) {
			cont = zp->continent;
			dmi = &cp->submenu[cp->nzones];
			memset(dmi, 0, sizeof(*dmi));
			asprintf(&dmi->prompt, "%d", ++cp->nzones);
			dmi->title = zp->descr;
			dmi->fire = set_zone;
			dmi->data = zp;

			for (zp2 = TAILQ_FIRST(&cp->zones);
			    zp2->continent != cont;
			    zp2 = TAILQ_NEXT(zp2, link))
				;
			if (zp2 != zp)
				continue;

			dmi = &cont->menu[cont->nitems];
			memset(dmi, 0, sizeof(*dmi));
			asprintf(&dmi->prompt, "%d", ++cont->nitems);
			dmi->title = cp->name;
			dmi->fire = set_zone_menu;
			dmi->data = cp;
		}

		if (cp->alternate != NULL) {
			cont = cp->alternate;
			dmi = &cont->menu[cont->nitems];
			memset(dmi, 0, sizeof(*dmi));
			asprintf(&dmi->prompt, "%d", ++cont->nitems);
			dmi->title = cp->name;
			dmi->fire = set_zone_menu;
			dmi->data = cp;
		}
	}
}

static int
set_zone_menu(dialogMenuItem *dmi)
{
	char		title[64], prompt[64];
	struct country	*cp = dmi->data;
	int		rv;

	/* Short cut -- if there's only one zone, don't post a menu. */
	if (cp->nzones == 1)
		return (cp->submenu[0].fire(&cp->submenu[0]));

	snprintf(title, sizeof(title), "%s Time Zones", cp->name);
	snprintf(prompt, sizeof(prompt),
	    "Select a zone which observes the same time as your locality.");
	rv = xdialog_menu(title, prompt, cp->nzones, cp->submenu);
	return (rv != 0 ? DITEM_RECREATE : DITEM_LEAVE_MENU);
}

static int
set_zone_utc(void)
{
	if (!confirm_zone("UTC"))
		return (DITEM_FAILURE | DITEM_RECREATE);

	return (install_zoneinfo("UTC"));
}

static int
confirm_zone(const char *filename)
{
	char		prompt[64];
	time_t		t = time(0);
	struct tm	*tm;
	int		rv;

	setenv("TZ", filename, 1);
	tzset();
	tm = localtime(&t);

	snprintf(prompt, sizeof(prompt),
	    "Does the abbreviation `%s' look reasonable?", tm->tm_zone);
	conf.title = "Confirmation";
	rv = (bsddialog_yesno(&conf, prompt, 5, 72) == BSDDIALOG_YES);
	return (rv);
}

static int
set_zone(dialogMenuItem *dmi)
{
	struct zone	*zp = dmi->data;
	int		rv;

	if (!confirm_zone(zp->filename))
		return (DITEM_FAILURE | DITEM_RECREATE);

	rv = install_zoneinfo(zp->filename);
	return (rv);
}

#endif

static void message_zoneinfo_file(const char *title, char *prompt)
{
#ifdef HAVE_BSDDIALOG
	if (usedialog) {
		conf.title = title;
		bsddialog_msgbox(&conf, prompt, 8, 72);
	} else
#endif
		fprintf(stderr, "%s: %s\n", title, prompt);
}

static int
install_zoneinfo_file(const char *zoneinfo_file)
{
	char		prompt[SILLY_BUFFER_SIZE];

#ifdef VERBOSE
	snprintf(prompt, sizeof(prompt), "Creating symbolic link %s to %s",
	    path_localtime, zoneinfo_file);
	message_zoneinfo_file("Info", prompt);
#endif

	if (reallydoit) {
		if (access(zoneinfo_file, R_OK) != 0) {
			snprintf(prompt, sizeof(prompt),
			    "Cannot access %s: %s", zoneinfo_file,
			    strerror(errno));
			message_zoneinfo_file("Error", prompt);
			return (DITEM_FAILURE | DITEM_RECREATE);
		}
		if (unlink(path_localtime) < 0 && errno != ENOENT) {
			snprintf(prompt, sizeof(prompt),
			    "Could not delete %s: %s",
			    path_localtime, strerror(errno));
			message_zoneinfo_file("Error", prompt);
			return (DITEM_FAILURE | DITEM_RECREATE);
		}
		if (symlink(zoneinfo_file, path_localtime) < 0) {
			snprintf(prompt, sizeof(prompt),
			    "Cannot create symbolic link %s to %s: %s",
			    path_localtime, zoneinfo_file,
			    strerror(errno));
			message_zoneinfo_file("Error", prompt);
			return (DITEM_FAILURE | DITEM_RECREATE);
		}

#ifdef VERBOSE
		snprintf(prompt, sizeof(prompt),
		    "Created symbolic link from %s to %s", zoneinfo_file,
		    path_localtime);
		message_zoneinfo_file("Done", prompt);
#endif
	} /* reallydoit */

	return (DITEM_LEAVE_MENU);
}

static int
install_zoneinfo(const char *zoneinfo)
{
	int		rv;
	FILE		*f;
	char		path_zoneinfo_file[MAXPATHLEN];

	if ((size_t)snprintf(path_zoneinfo_file, sizeof(path_zoneinfo_file),
	    "%s/%s", path_zoneinfo, zoneinfo) >= sizeof(path_zoneinfo_file))
		errx(1, "%s/%s name too long", path_zoneinfo, zoneinfo);
	rv = install_zoneinfo_file(path_zoneinfo_file);

	/* Save knowledge for later */
	if (reallydoit && (rv & DITEM_FAILURE) == 0) {
		if ((f = fopen(path_db, "w")) != NULL) {
			fprintf(f, "%s\n", zoneinfo);
			fclose(f);
		}
	}

	return (rv);
}

static void
usage(void)
{

	fprintf(stderr, "usage: tzsetup [-nrs] [-C chroot_directory]"
	    " [zoneinfo_file | zoneinfo_name]\n");
	exit(1);
}

int
main(int argc, char **argv)
{
#ifdef HAVE_BSDDIALOG
	char		prompt[128];
	int		fd;
#endif
	int		c, rv, skiputc;
	char		vm_guest[16] = "";
	size_t		len = sizeof(vm_guest);
	char		*dztpath;

	dztpath = NULL;
	skiputc = 0;

#ifdef HAVE_BSDDIALOG
	setlocale(LC_ALL, "");
#endif

	/* Default skiputc to 1 for VM guests */
	if (sysctlbyname("kern.vm_guest", vm_guest, &len, NULL, 0) == 0 &&
	    strcmp(vm_guest, "none") != 0)
		skiputc = 1;

	while ((c = getopt(argc, argv, "C:d:nrs")) != -1) {
		switch (c) {
		case 'C':
			chrootenv = optarg;
			break;
		case 'd':
			dztpath = optarg;
			break;
		case 'n':
			reallydoit = 0;
			break;
		case 'r':
			reinstall = 1;
#ifdef HAVE_BSDDIALOG
			usedialog = 0;
#endif
			break;
		case 's':
			skiputc = 1;
			break;
		default:
			usage();
		}
	}

	if (argc - optind > 1)
		usage();

	if (chrootenv == NULL) {
		if (dztpath == NULL)
			strcpy(path_zonetab, _PATH_ZONETAB);
		else
			strlcpy(path_zonetab, dztpath, sizeof(path_zonetab));
		strcpy(path_iso3166, _PATH_ISO3166);
		strcpy(path_zoneinfo, _PATH_ZONEINFO);
		strcpy(path_localtime, _PATH_LOCALTIME);
		strcpy(path_db, _PATH_DB);
		strcpy(path_wall_cmos_clock, _PATH_WALL_CMOS_CLOCK);
	} else {
		sprintf(path_zonetab, "%s/%s", chrootenv, _PATH_ZONETAB);
		sprintf(path_iso3166, "%s/%s", chrootenv, _PATH_ISO3166);
		sprintf(path_zoneinfo, "%s/%s", chrootenv, _PATH_ZONEINFO);
		sprintf(path_localtime, "%s/%s", chrootenv, _PATH_LOCALTIME);
		sprintf(path_db, "%s/%s", chrootenv, _PATH_DB);
		sprintf(path_wall_cmos_clock, "%s/%s", chrootenv,
		    _PATH_WALL_CMOS_CLOCK);
	}

	/* Override the user-supplied umask. */
	(void)umask(S_IWGRP | S_IWOTH);

	if (reinstall == 1) {
		FILE *f;
		char zoneinfo[MAXPATHLEN];

		if ((f = fopen(path_db, "r")) != NULL) {
			if (fgets(zoneinfo, sizeof(zoneinfo), f) != NULL) {
				zoneinfo[sizeof(zoneinfo) - 1] = 0;
				if (strlen(zoneinfo) > 0) {
					zoneinfo[strlen(zoneinfo) - 1] = 0;
					rv = install_zoneinfo(zoneinfo);
					exit(rv & ~DITEM_LEAVE_MENU);
				}
				errx(1, "Error reading %s.\n", path_db);
			}
			fclose(f);
			errx(1,
			    "Unable to determine earlier installed zoneinfo "
			    "name. Check %s", path_db);
		}
		errx(1, "Cannot open %s for reading. Does it exist?", path_db);
	}

	/*
	 * If the arguments on the command-line do not specify a file,
	 * then interpret it as a zoneinfo name
	 */
	if (optind == argc - 1) {
		struct stat sb;

		if (stat(argv[optind], &sb) != 0) {
#ifdef HAVE_BSDDIALOG
			usedialog = 0;
#endif
			rv = install_zoneinfo(argv[optind]);
			exit(rv & ~DITEM_LEAVE_MENU);
		}
		/* FALLTHROUGH */
	}
#ifdef HAVE_BSDDIALOG

	read_iso3166_table();
	read_zones();
	sort_countries();
	if (dztpath != NULL) {
		dump_zonetab();
		return (0);
	}
	make_menus();

	bsddialog_initconf(&conf);
	conf.clear = true;
	conf.auto_minwidth = 24;
	conf.key.enable_esc = true;

	if (bsddialog_init() == BSDDIALOG_ERROR)
		errx(1, "Error bsddialog: %s\n", bsddialog_geterror());

	if (skiputc == 0) {
		snprintf(prompt, sizeof(prompt),
		    "Is this machine's CMOS clock set to UTC?  "
		    "If it is set to local time,\n"
		    "or you don't know, please choose NO here!");

		conf.title = "Select local or UTC (Greenwich Mean Time) clock";
		if (bsddialog_yesno(&conf, prompt, 7, 73) == BSDDIALOG_YES) {
			if (reallydoit)
				unlink(path_wall_cmos_clock);
		} else {
			if (reallydoit) {
				fd = open(path_wall_cmos_clock,
				    O_WRONLY | O_CREAT | O_TRUNC,
				    S_IRUSR | S_IRGRP | S_IROTH);
				if (fd < 0) {
					bsddialog_end();
					err(1, "create %s",
					    path_wall_cmos_clock);
				}
				close(fd);
			}
		}
	}
	if (optind == argc - 1) {
		snprintf(prompt, sizeof(prompt),
		    "\nUse the default `%s' zone?", argv[optind]);
		conf.title = "Default timezone provided";
		if (bsddialog_yesno(&conf, prompt, 7, 72) == BSDDIALOG_YES) {
			rv = install_zoneinfo_file(argv[optind]);
			bsddialog_end();
			exit(rv & ~DITEM_LEAVE_MENU);
		}
	}
	xdialog_menu("Time Zone Selector", "Select a region", NCONTINENTS,
	    continents);

	bsddialog_end();
#else
	usage();
#endif
	return (0);
}
