/*	$FreeBSD$	*/

/*
 * Copyright (C) 1995-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ipft_hx.c	1.1 3/9/96 (C) 1996 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipft_hx.c,v 1.11.4.3 2005/12/04 10:07:21 darrenr Exp $";
#endif

#include <ctype.h>

#include "ipf.h"
#include "ipt.h"


extern	int	opts;

static	int	hex_open __P((char *));
static	int	hex_close __P((void));
static	int	hex_readip __P((char *, int, char **, int *));
static	char	*readhex __P((char *, char *));

struct	ipread	iphex = { hex_open, hex_close, hex_readip, 0 };
static	FILE	*tfp = NULL;
static	int	tfd = -1;

static	int	hex_open(fname)
char	*fname;
{
	if (tfp && tfd != -1) {
		rewind(tfp);
		return tfd;
	}

	if (!strcmp(fname, "-")) {
		tfd = 0;
		tfp = stdin;
	} else {
		tfd = open(fname, O_RDONLY);
		if (tfd != -1)
			tfp = fdopen(tfd, "r");
	}
	return tfd;
}


static	int	hex_close()
{
	int	cfd = tfd;

	tfd = -1;
	return close(cfd);
}


static	int	hex_readip(buf, cnt, ifn, dir)
char	*buf, **ifn;
int	cnt, *dir;
{
	register char *s, *t, *u;
	char	line[513];
	ip_t	*ip;

	/*
	 * interpret start of line as possibly "[ifname]" or
	 * "[in/out,ifname]".
	 */
	if (ifn)
		*ifn = NULL;
	if (dir)
		*dir = 0;
 	ip = (ip_t *)buf;
	while (fgets(line, sizeof(line)-1, tfp)) {
		if ((s = strchr(line, '\n'))) {
			if (s == line)
				return (char *)ip - buf;
			*s = '\0';
		}
		if ((s = strchr(line, '#')))
			*s = '\0';
		if (!*line)
			continue;
		if ((opts & OPT_DEBUG) != 0) {
			printf("input: %s", line);
		}

		if ((*line == '[') && (s = strchr(line, ']'))) {
			t = line + 1;
			if (s - t > 0) {
				*s++ = '\0';
				if ((u = strchr(t, ',')) && (u < s)) {
					u++;
					if (ifn)
						*ifn = strdup(u);
					if (dir) {
						if (*t == 'i')
							*dir = 0;
						else if (*t == 'o')
							*dir = 1;
					}
				} else if (ifn)
					*ifn = t;
			}
		} else
			s = line;
		t = (char *)ip;
		ip = (ip_t *)readhex(s, (char *)ip);
		if ((opts & OPT_DEBUG) != 0) {
			if (opts & OPT_ASCII) {
				if (t < (char *)ip)
					putchar('\t');
				while (t < (char *)ip) {
					if (ISPRINT(*t) && ISASCII(*t))
						putchar(*t);
					else
						putchar('.');
					t++;
				}
			}
			putchar('\n');
			fflush(stdout);
		}
	}
	if (feof(tfp))
		return 0;
	return -1;
}


static	char	*readhex(src, dst)
register char	*src, *dst;
{
	int	state = 0;
	char	c;

	while ((c = *src++)) {
		if (ISSPACE(c)) {
			if (state) {
				dst++;
				state = 0;
			}
			continue;
		} else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
			   (c >= 'A' && c <= 'F')) {
			c = ISDIGIT(c) ? (c - '0') : (TOUPPER(c) - 55);
			if (state == 0) {
				*dst = (c << 4);
				state++;
			} else {
				*dst++ |= c;
				state = 0;
			}
		} else
			break;
	}
	return dst;
}
