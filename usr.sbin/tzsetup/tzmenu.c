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

static const char rcsid[] =
	"$Id: tzmenu.c,v 1.2.4.1 1996/03/22 22:24:16 joerg Exp $";

#include <stdio.h>
#include <ncurses.h>
#include <dialog.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "tzsetup.h"

static const char *regmenu[] = {
	"1",	"Africa",
	"2",	"Asia",
	"3",	"Atlantic Ocean islands",
	"4",	"Australia",
	"5",	"Europe (including Russia)",
	"6",	"Indian Ocean islands",
	"7",	"North and South America",
	"8",	"Pacific Ocean islands"
};

static struct region *regions[] = {
	&Africa,
	&Asia,
	&Atlantic,
	&Australia,
	&Europe,
	&Indian,
	&America,
	&Pacific
};

#define NREGIONS 8
#define DEFAULT_NROWS 24	/* default height of tty */

static const char *country_menu(const struct region *, const char *);

const char *
tzmenu(void)
{
	unsigned char rbuf[_POSIX2_LINE_MAX];
	int rv;
	int item = 0;
	int sc = 0;
	const char *res;
	struct winsize win;
	char *cp;

	while(1) {
		dialog_clear();
		rv = dialog_menu("Timezone Selector",
				 "Select a region",
				 NREGIONS + 6,
				 78,
				 NREGIONS,
				 NREGIONS,
				 (unsigned char **)regmenu,
				 rbuf,
				 &item,
				 &sc);


		if (rv != 0) {
			return 0;
		}

		res = country_menu(regions[item],
				   regmenu[2 * item + 1]);

		if (res)
			return res;
	}
}

static const char *location_menu(const struct country *, const char *);

static const char *
country_menu(const struct region *reg, const char *name)
{
	unsigned char rbuf[_POSIX2_LINE_MAX];
	unsigned char title[_POSIX2_LINE_MAX];
	int rv;
	int item = 0;
	int sc = 0;
	const char *res;

	snprintf(title, sizeof title, "Timezone Selector - %s", name);

	while(1) {
		dialog_clear();
		rv = dialog_menu(title, "Select a country",
				 reg->r_count > LINES - 6 ?
				 LINES : reg->r_count + 6,
				 78,
				 reg->r_count > LINES - 6 ?
				 LINES - 6 : reg->r_count,
				 reg->r_count,
				 (unsigned char **)reg->r_menu,
				 rbuf,
				 &item,
				 &sc);


		if (rv != 0) {
			return 0;
		}

		sscanf(rbuf, "%d", &rv);

		res = location_menu(reg->r_ctrylist[rv - 1],
				    reg->r_menu[2 * (rv - 1) + 1]);

		if (res)
			return res;
	}
}

static const char *
location_menu(const struct country *ctry, const char *name)
{
	unsigned char rbuf[_POSIX2_LINE_MAX];
	unsigned char title[_POSIX2_LINE_MAX];
	int rv;
	int item = 0;
	int sc = 0;
	const char *res;

	snprintf(title, sizeof title, "Timezone Selector - %s", name);

	while(1) {
		dialog_clear();
		rv = dialog_menu(title, "Select a location",
				 ctry->c_count > LINES - 6?
				 LINES : ctry->c_count + 6,
				 78,
				 ctry->c_count > LINES - 6?
				 LINES - 6 : ctry->c_count,
				 ctry->c_count,
				 (unsigned char **)ctry->c_menu,
				 rbuf,
				 &item,
				 &sc);


		if (rv != 0) {
			return 0;
		}

		sscanf(rbuf, "%d", &rv);

		rv = setzone(ctry->c_filelist[rv - 1]);

		if (rv == 0)
			return ctry->c_filelist[rv - 1];
	}
}

