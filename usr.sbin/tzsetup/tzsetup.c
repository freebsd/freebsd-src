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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <dialog.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/fcntl.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include "paths.h"

static int reallydoit = 1;

static int continent_country_menu(dialogMenuItem *);
static int set_zone_multi(dialogMenuItem *);
static int set_zone_whole_country(dialogMenuItem *);
static int set_zone_menu(dialogMenuItem *);

struct continent {
	dialogMenuItem *menu;
	int nitems;
	int ch;
	int sc;
};

static struct continent africa, america, antarctica, arctic, asia, atlantic;
static struct continent australia, europe, indian, pacific;

static struct continent_names {
	char *name;
	struct continent *continent;
} continent_names[] = {
	{ "Africa", &africa }, { "America", &america },
	{ "Antarctica", &antarctica }, { "Arctic", &arctic }, 
	{ "Asia", &asia },
	{ "Atlantic", &atlantic }, { "Australia", &australia },
	{ "Europe", &europe }, { "Indian", &indian }, { "Pacific", &pacific }
};

static dialogMenuItem continents[] = {
	{ "1", "Africa", 0, continent_country_menu, 0, &africa },
	{ "2", "America -- North and South", 0, continent_country_menu, 0, 
		  &america },
	{ "3", "Antarctica", 0, continent_country_menu, 0, &antarctica },
	{ "4", "Arctic Ocean", 0, continent_country_menu, 0, &arctic },
	{ "5", "Asia", 0, continent_country_menu, 0, &asia },
	{ "6", "Atlantic Ocean", 0, continent_country_menu, 0, &atlantic },
	{ "7", "Australia", 0, continent_country_menu, 0, &australia },
	{ "8", "Europe", 0, continent_country_menu, 0, &europe },
	{ "9", "Indian Ocean", 0, continent_country_menu, 0, &indian },
	{ "0", "Pacific Ocean", 0, continent_country_menu, 0, &pacific }
};
#define NCONTINENTS ((sizeof continents)/(sizeof continents[0]))
#define OCEANP(x) ((x) == 3 || (x) == 5 || (x) == 8 || (x) == 9)

static int
continent_country_menu(dialogMenuItem *continent)
{
	int rv;
	struct continent *contp = continent->data;
	char title[256];
	int isocean = OCEANP(continent - continents);
	int menulen;

	/* Short cut -- if there's only one country, don't post a menu. */
	if (contp->nitems == 1) {
		return set_zone_menu(&contp->menu[0]);
	}

	/* It's amazing how much good grammar really matters... */
	if (!isocean)
		snprintf(title, sizeof title, "Countries in %s", 
			 continent->title);
	else 
		snprintf(title, sizeof title, "Islands and groups in the %s",
			 continent->title);

	menulen = contp->nitems < 16 ? contp->nitems : 16;
	rv = dialog_menu(title, (isocean ? "Select an island or group"
                                 : "Select a country"), -1, -1, menulen,
			 -contp->nitems, contp->menu, 0, &contp->ch,
			 &contp->sc);
	if (rv == 0)
		return DITEM_LEAVE_MENU;
	return DITEM_RECREATE;
}

static struct continent *
find_continent(const char *name)
{
	int i;

	for (i = 0; i < NCONTINENTS; i++) {
		if (strcmp(name, continent_names[i].name) == 0)
			return continent_names[i].continent;
	}
	return 0;
}

struct country {
	char *name;
	char *tlc;
	int nzones;
	char *filename;		/* use iff nzones < 0 */
	struct continent *continent; /* use iff nzones < 0 */
	TAILQ_HEAD(, zone) zones; /* use iff nzones > 0 */
	dialogMenuItem *submenu; /* use iff nzones > 0 */
};

struct zone {
	TAILQ_ENTRY(zone) link;
	char *descr;
	char *filename;
	struct continent *continent;
};

/*
 * This is the easiest organization... we use ISO 3166 country codes,
 * of the two-letter variety, so we just size this array to suit.
 * Beats worrying about dynamic allocation.
 */
#define NCOUNTRIES	(26*26)
static struct country countries[NCOUNTRIES];
#define CODE2INT(s) ((s[0] - 'A') * 26 + (s[1] - 'A'))

/*
 * Read the ISO 3166 country code database in _PATH_ISO3166
 * (/usr/share/misc/iso3166).  On error, exit via err(3).
 */
static void
read_iso3166_table(void)
{
	FILE *fp;
	char *s, *t, *name;
	size_t len;
	int lineno;
	struct country *cp;

	fp = fopen(_PATH_ISO3166, "r");
	if (!fp)
		err(1, _PATH_ISO3166);
	lineno = 0;

	while ((s = fgetln(fp, &len)) != 0) {
		lineno++;
		if (s[len - 1] != '\n')
			errx(1, _PATH_ISO3166 ":%d: invalid format", lineno);
		s[len - 1] = '\0';
		if (s[0] == '#' || strspn(s, " \t") == len - 1)
			continue;

		/* Isolate the two-letter code. */
		t = strsep(&s, "\t");
		if (t == 0 || strlen(t) != 2)
			errx(1, _PATH_ISO3166 ":%d: invalid format", lineno);
		if (t[0] < 'A' || t[0] > 'Z' || t[1] < 'A' || t[1] > 'Z')
			errx(1, _PATH_ISO3166 ":%d: invalid code `%s'",
			     lineno, t);

		/* Now skip past the three-letter and numeric codes. */
		name = strsep(&s, "\t"); /* 3-let */
		if (name == 0 || strlen(name) != 3)
			errx(1, _PATH_ISO3166 ":%d: invalid format", lineno);
		name = strsep(&s, "\t"); /* numeric */
		if (name == 0 || strlen(name) != 3)
			errx(1, _PATH_ISO3166 ":%d: invalid format", lineno);

		name = s;

		cp = &countries[CODE2INT(t)];
		if (cp->name)
			errx(1, _PATH_ISO3166 
			     ":%d: country code `%s' multiply defined: %s",
			     lineno, t, cp->name);
		cp->name = strdup(name);
		if (cp->name == NULL)
			errx(1, "malloc failed");
		cp->tlc = strdup(t);
		if (cp->tlc == NULL)
			errx(1, "malloc failed");
	}

	fclose(fp);
}

static void
add_zone_to_country(int lineno, const char *tlc, const char *descr,
		    const char *file, struct continent *cont)
{
	struct zone *zp;
	struct country *cp;

	if (tlc[0] < 'A' || tlc[0] > 'Z' || tlc[1] < 'A' || tlc[1] > 'Z')
		errx(1, _PATH_ZONETAB ":%d: country code `%s' invalid",
		     lineno, tlc);
	
	cp = &countries[CODE2INT(tlc)];
	if (cp->name == 0)
		errx(1, _PATH_ZONETAB ":%d: country code `%s' unknown",
		     lineno, tlc);

	if (descr) {
		if (cp->nzones < 0)
			errx(1, _PATH_ZONETAB 
			     ":%d: conflicting zone definition", lineno);

		zp = malloc(sizeof *zp);
		if (zp == 0)
			errx(1, "malloc(%lu)", (unsigned long)sizeof *zp);
		
		if (cp->nzones == 0)
			TAILQ_INIT(&cp->zones);

		zp->descr = strdup(descr);
		if (zp->descr == NULL)
			errx(1, "malloc failed");
		zp->filename = strdup(file);
		if (zp->filename == NULL)
			errx(1, "malloc failed");
		zp->continent = cont;
		TAILQ_INSERT_TAIL(&cp->zones, zp, link);
		cp->nzones++;
	} else {
		if (cp->nzones > 0)
			errx(1, _PATH_ZONETAB 
			     ":%d: zone must have description", lineno);
		if (cp->nzones < 0)
			errx(1, _PATH_ZONETAB
			     ":%d: zone multiply defined", lineno);
		cp->nzones = -1;
		cp->filename = strdup(file);
		if (cp->filename == NULL)
			errx(1, "malloc failed");
		cp->continent = cont;
	}
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
		return 0;
	if (a->name == 0 && b->name != 0)
		return 1;
	if (b->name == 0)
		return -1;

	return strcmp(a->name, b->name);
}

/*
 * This must be done AFTER all zone descriptions are read, since it breaks
 * CODE2INT().
 */
static void
sort_countries(void)
{
	qsort(countries, NCOUNTRIES, sizeof countries[0], compare_countries);
}

static void
read_zones(void)
{
	FILE *fp;
	char *line;
	size_t len;
	int lineno;
	char *tlc, *coord, *file, *descr, *p;
	char contbuf[16];
	struct continent *cont;

	fp = fopen(_PATH_ZONETAB, "r");
	if (!fp)
		err(1, _PATH_ZONETAB);
	lineno = 0;

	while ((line = fgetln(fp, &len)) != 0) {
		lineno++;
		if (line[len - 1] != '\n')
			errx(1, _PATH_ZONETAB ":%d: invalid format", lineno);
		line[len - 1] = '\0';
		if (line[0] == '#')
			continue;

		tlc = strsep(&line, "\t");
		if (strlen(tlc) != 2)
			errx(1, _PATH_ZONETAB ":%d: invalid country code `%s'",
			     lineno, tlc);
		coord = strsep(&line, "\t");
		file = strsep(&line, "\t");
		p = strchr(file, '/');
		if (p == 0)
			errx(1, _PATH_ZONETAB ":%d: invalid zone name `%s'",
			     lineno, file);
		contbuf[0] = '\0';
		strncat(contbuf, file, p - file);
		cont = find_continent(contbuf);
		if (!cont)
			errx(1, _PATH_ZONETAB ":%d: invalid region `%s'",
			     lineno, contbuf);

		descr = (line && *line) ? line : 0;

		add_zone_to_country(lineno, tlc, descr, file, cont);
	}
	fclose(fp);
}

static void
make_menus(void)
{
	struct country *cp;
	struct zone *zp, *zp2;
	struct continent *cont;
	dialogMenuItem *dmi;
	int i;

	/*
	 * First, count up all the countries in each continent/ocean.
	 * Be careful to count those countries which have multiple zones
	 * only once for each.  NB: some countries are in multiple
	 * continents/oceans.
	 */
	for (cp = countries; cp->name; cp++) {
		if (cp->nzones == 0)
			continue;
		if (cp->nzones < 0) {
			cp->continent->nitems++;
		} else {
			for (zp = cp->zones.tqh_first; zp; 
			     zp = zp->link.tqe_next) {
				cont = zp->continent;
				for (zp2 = cp->zones.tqh_first;
				     zp2->continent != cont;
				     zp2 = zp2->link.tqe_next)
					;
				if (zp2 == zp)
					zp->continent->nitems++;
			}
		}
	}

	/*
	 * Now allocate memory for the country menus.  We set
	 * nitems back to zero so that we can use it for counting
	 * again when we actually build the menus.
	 */
	for (i = 0; i < NCONTINENTS; i++) {
		continent_names[i].continent->menu =
			malloc(sizeof(dialogMenuItem) *
			       continent_names[i].continent->nitems);
		if (continent_names[i].continent->menu == 0)
			errx(1, "malloc for continent menu");
		continent_names[i].continent->nitems = 0;
	}

	/*
	 * Now that memory is allocated, create the menu items for
	 * each continent.  For multiple-zone countries, also create
	 * the country's zone submenu.
	 */
	for (cp = countries; cp->name; cp++) {
		if (cp->nzones == 0)
			continue;
		if (cp->nzones < 0) {
			dmi = &cp->continent->menu[cp->continent->nitems];
			memset(dmi, 0, sizeof *dmi);
			asprintf(&dmi->prompt, "%d", 
				 ++cp->continent->nitems);
			dmi->title = cp->name;
			dmi->checked = 0;
			dmi->fire = set_zone_whole_country;
			dmi->selected = 0;
			dmi->data = cp;
		} else {
			cp->submenu = malloc(cp->nzones * sizeof *dmi);
			if (cp->submenu == 0)
				errx(1, "malloc for submenu");
			cp->nzones = 0;
			for (zp = cp->zones.tqh_first; zp; 
			     zp = zp->link.tqe_next) {
				cont = zp->continent;
				dmi = &cp->submenu[cp->nzones];
				memset(dmi, 0, sizeof *dmi);
				asprintf(&dmi->prompt, "%d",
					 ++cp->nzones);
				dmi->title = zp->descr;
				dmi->checked = 0;
				dmi->fire = set_zone_multi;
				dmi->selected = 0;
				dmi->data = zp;

				for (zp2 = cp->zones.tqh_first;
				     zp2->continent != cont;
				     zp2 = zp2->link.tqe_next)
					;
				if (zp2 != zp)
					continue;

				dmi = &cont->menu[cont->nitems];
				memset(dmi, 0, sizeof *dmi);
				asprintf(&dmi->prompt, "%d", ++cont->nitems);
				dmi->title = cp->name;
				dmi->checked = 0;
				dmi->fire = set_zone_menu;
				dmi->selected = 0;
				dmi->data = cp;
			}
		}
	}
}

static int
set_zone_menu(dialogMenuItem *dmi)
{
	int rv;
	char buf[256];
	struct country *cp = dmi->data;
	int menulen;

	snprintf(buf, sizeof buf, "%s Time Zones", cp->name);
	menulen = cp->nzones < 16 ? cp->nzones : 16;
	rv = dialog_menu(buf, "Select a zone which observes the same time as "
			 "your locality.", -1, -1, menulen, -cp->nzones,
			 cp->submenu, 0, 0, 0);
	if (rv != 0)
		return DITEM_RECREATE;
	return DITEM_LEAVE_MENU;
}

static int
install_zone_file(const char *filename)
{
	struct stat sb;
	int fd1, fd2;
	int copymode;
	char *msg;
	ssize_t len;
	char buf[1024];

	if (lstat(_PATH_LOCALTIME, &sb) < 0)
		/* Nothing there yet... */
		copymode = 1;
	else if(S_ISLNK(sb.st_mode))
		copymode = 0;
	else
		copymode = 1;

#ifdef VERBOSE
	if (copymode)
		asprintf(&msg, "Copying %s to " _PATH_LOCALTIME, filename);
	else
		asprintf(&msg, "Creating symbolic link " _PATH_LOCALTIME
			 " to %s", filename);

	dialog_notify(msg);
	free(msg);
#endif

	if (reallydoit) {
		if (copymode) {
			fd1 = open(filename, O_RDONLY, 0);
			if (fd1 < 0) {
				asprintf(&msg, "Could not open %s: %s",
					 filename, strerror(errno));
				dialog_mesgbox("Error", msg, 8, 72);
				free(msg);
				return DITEM_FAILURE | DITEM_RECREATE;
			}

			unlink(_PATH_LOCALTIME);
			fd2 = open(_PATH_LOCALTIME, 
				   O_CREAT|O_EXCL|O_WRONLY,
				   S_IRUSR|S_IRGRP|S_IROTH);
			if (fd2 < 0) {
				asprintf(&msg, "Could not open "
					 _PATH_LOCALTIME ": %s", 
					 strerror(errno));
				dialog_mesgbox("Error", msg, 8, 72);
				free(msg);
				return DITEM_FAILURE | DITEM_RECREATE;
			}

			while ((len = read(fd1, buf, sizeof buf)) > 0)
				len = write(fd2, buf, len);

			if (len == -1) {
				asprintf(&msg, "Error copying %s to "
					 _PATH_LOCALTIME ": %s",
					 filename, strerror(errno));
				dialog_mesgbox("Error", msg, 8, 72);
				free(msg);
				/* Better to leave none than a corrupt one. */
				unlink(_PATH_LOCALTIME);
				return DITEM_FAILURE | DITEM_RECREATE;
			}
			close(fd1);
			close(fd2);
		} else {
			if (access(filename, R_OK) != 0) {
				asprintf(&msg, "Cannot access %s: %s",
					 filename, strerror(errno));
				dialog_mesgbox("Error", msg, 8, 72);
				free(msg);
				return DITEM_FAILURE | DITEM_RECREATE;
			}
			unlink(_PATH_LOCALTIME);
			if (symlink(filename, _PATH_LOCALTIME) < 0) {
				asprintf(&msg, "Cannot create symbolic link "
					 _PATH_LOCALTIME " to %s: %s",
					 filename, strerror(errno));
				dialog_mesgbox("Error", msg, 8, 72);
				free(msg);
				return DITEM_FAILURE | DITEM_RECREATE;
			}
		}
	}

#ifdef VERBOSE
	if (copymode)
		asprintf(&msg, "Copied timezone file from %s to " 
			 _PATH_LOCALTIME, filename);
	else
		asprintf(&msg, "Created symbolic link from " _PATH_LOCALTIME
			 " to %s", filename);

	dialog_mesgbox("Done", msg, 8, 72);
	free(msg);
#endif
	return DITEM_LEAVE_MENU;
}

static int
confirm_zone(const char *filename)
{
	char *msg;
	struct tm *tm;
	time_t t = time(0);
	int rv;
	
	setenv("TZ", filename, 1);
	tzset();
	tm = localtime(&t);

	asprintf(&msg, "Does the abbreviation `%s' look reasonable?",
		 tm->tm_zone);
	rv = !dialog_yesno("Confirmation", msg, 4, 72);
	free(msg);
	return rv;
}

static int
set_zone_multi(dialogMenuItem *dmi)
{
	char *fn;
	struct zone *zp = dmi->data;
	int rv;

	if (!confirm_zone(zp->filename))
		return DITEM_FAILURE | DITEM_RECREATE;

	asprintf(&fn, "%s/%s", _PATH_ZONEINFO, zp->filename);
	rv = install_zone_file(fn);
	free(fn);
	return rv;
}

static int
set_zone_whole_country(dialogMenuItem *dmi)
{
	char *fn;
	struct country *cp = dmi->data;
	int rv;

	if (!confirm_zone(cp->filename))
		return DITEM_FAILURE | DITEM_RECREATE;

	asprintf(&fn, "%s/%s", _PATH_ZONEINFO, cp->filename);
	rv = install_zone_file(fn);
	free(fn);
	return rv;
}

static void
usage()
{
	fprintf(stderr, "usage: tzsetup [-n]\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	int c, fd;
	int (*dialog_utc)(unsigned char *, unsigned char *, int, int);

#if defined(__alpha__)
	dialog_utc = dialog_yesno;
#else
	dialog_utc = dialog_noyes;
#endif

	while ((c = getopt(argc, argv, "n")) != -1) {
		switch(c) {
		case 'n':
			reallydoit = 0;
			break;

		default:
			usage();
		}
	}

	if (argc - optind > 1)
		usage();

	/* Override the user-supplied umask. */
	(void)umask(S_IWGRP|S_IWOTH);

	read_iso3166_table();
	read_zones();
	sort_countries();
	make_menus();

	init_dialog();
	if (!dialog_utc("Select local or UTC (Greenwich Mean Time) clock",
			"Is this machine's CMOS clock set to UTC?  If it is set to local time,\n"
			"or you don't know, please choose NO here!", 7, 72)) {
		if (reallydoit)
			unlink(_PATH_WALL_CMOS_CLOCK);
	} else {
		if (reallydoit) {
			fd = open(_PATH_WALL_CMOS_CLOCK,
				  O_WRONLY|O_CREAT|O_TRUNC,
				  S_IRUSR|S_IRGRP|S_IROTH);
			if (fd < 0)
				err(1, "create %s", _PATH_WALL_CMOS_CLOCK);
			close(fd);
		}
	}
	dialog_clear_norefresh();
	if (optind == argc - 1) {
		char *msg;
		asprintf(&msg, "\nUse the default `%s' zone?", argv[optind]);
		if (!dialog_yesno("Default timezone provided", msg, 7, 72)) {
			install_zone_file(argv[optind]);
			dialog_clear();
			end_dialog();
			return 0;
		}
		free(msg);
		dialog_clear_norefresh();
	}
	dialog_menu("Time Zone Selector", "Select a region", -1, -1, 
		    NCONTINENTS, -NCONTINENTS, continents, 0, NULL, NULL);

	dialog_clear();
	end_dialog();
	return 0;
}

