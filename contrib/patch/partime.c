/* Parse a string, yielding a struct partime that describes it.  */

/* Copyright 1993, 1994, 1995, 1997 Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

   This file is part of RCS.

   RCS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   RCS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with RCS; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Report problems and direct all questions to:

	rcs-bugs@cs.purdue.edu

 */

#if has_conf_h
# include <conf.h>
#else
# if HAVE_CONFIG_H
#  include <config.h>
# else
#  ifndef __STDC__
#   define const
#  endif
# endif
# if HAVE_LIMITS_H
#  include <limits.h>
# endif
# ifndef LONG_MIN
# define LONG_MIN (-1-2147483647L)
# endif
# if STDC_HEADERS
#  include <stdlib.h>
# endif
# include <time.h>
# ifdef __STDC__
#  define P(x) x
# else
#  define P(x) ()
# endif
#endif

#include <ctype.h>
#if STDC_HEADERS
# define CTYPE_DOMAIN(c) 1
#else
# define CTYPE_DOMAIN(c) ((unsigned) (c) <= 0177)
#endif
#define ISALNUM(c)	(CTYPE_DOMAIN (c) && isalnum (c))
#define ISALPHA(c)	(CTYPE_DOMAIN (c) && isalpha (c))
#define ISSPACE(c)	(CTYPE_DOMAIN (c) && isspace (c))
#define ISUPPER(c)	(CTYPE_DOMAIN (c) && isupper (c))
#define ISDIGIT(c)	((unsigned) (c) - '0' <= 9)

#include <partime.h>

char const partimeId[] =
  "$Id: partime.c,v 5.16 1997/05/19 06:33:53 eggert Exp $";


/* Lookup tables for names of months, weekdays, time zones.  */

#define NAME_LENGTH_MAXIMUM 4

struct name_val
  {
    char name[NAME_LENGTH_MAXIMUM];
    int val;
  };


static char const *parse_decimal P ((char const *, int, int, int, int, int *, int *));
static char const *parse_fixed P ((char const *, int, int *));
static char const *parse_pattern_letter P ((char const *, int, struct partime *));
static char const *parse_prefix P ((char const *, struct partime *, int *));
static char const *parse_ranged P ((char const *, int, int, int, int *));
static int lookup P ((char const *, struct name_val const[]));
static int merge_partime P ((struct partime *, struct partime const *));
static void undefine P ((struct partime *));


static struct name_val const month_names[] =
{
  {"jan", 0},
  {"feb", 1},
  {"mar", 2},
  {"apr", 3},
  {"may", 4},
  {"jun", 5},
  {"jul", 6},
  {"aug", 7},
  {"sep", 8},
  {"oct", 9},
  {"nov", 10},
  {"dec", 11},
  {"", TM_UNDEFINED}
};

static struct name_val const weekday_names[] =
{
  {"sun", 0},
  {"mon", 1},
  {"tue", 2},
  {"wed", 3},
  {"thu", 4},
  {"fri", 5},
  {"sat", 6},
  {"", TM_UNDEFINED}
};

#define hr60nonnegative(t) ((t)/100 * 60  +  (t)%100)
#define hr60(t) ((t)<0 ? -hr60nonnegative(-(t)) : hr60nonnegative(t))
#define zs(t,s) {s, hr60(t)}
#define zd(t,s,d) zs(t, s),  zs((t)+100, d)

static struct name_val const zone_names[] =
{
  zs (-1000, "hst"),		/* Hawaii */
  zd (-1000, "hast", "hadt"),	/* Hawaii-Aleutian */
  zd (- 900, "akst", "akdt"),	/* Alaska */
  zd (- 800, "pst" , "pdt" ),	/* Pacific */
  zd (- 700, "mst" , "mdt" ),	/* Mountain */
  zd (- 600, "cst" , "cdt" ),	/* Central */
  zd (- 500, "est" , "edt" ),	/* Eastern */
  zd (- 400, "ast" , "adt" ),	/* Atlantic */
  zd (- 330, "nst" , "ndt" ),	/* Newfoundland */
  zs (  000, "utc" ),		/* Coordinated Universal */
  zs (  000, "uct" ),		/* " */
  zs (  000, "cut" ),		/* " */
  zs (  000, "ut"),		/* Universal */
  zs (  000, "z"),		/* Zulu (required by ISO 8601) */
  zd (  000, "gmt" , "bst" ),	/* Greenwich Mean, British Summer */
  zd (  000, "wet" , "west"),	/* Western European */
  zd (  100, "cet" , "cest"),	/* Central European */
  zd (  100, "met" , "mest"),	/* Middle European (bug in old tz versions) */
  zd (  100, "mez" , "mesz"),	/* Mittel-Europaeische Zeit */
  zd (  200, "eet" , "eest"),	/* Eastern European */
  zs (  530, "ist" ),		/* India */
  zd (  900, "jst" , "jdt" ),	/* Japan */
  zd (  900, "kst" , "kdt" ),	/* Korea */
  zd ( 1200, "nzst", "nzdt"),	/* New Zealand */
  {"lt", 1},
#if 0
  /* The following names are duplicates or are not well attested.
     There are lots more where these came from.  */
  zs (-1100, "sst" ),		/* Samoan */
  zd (- 900, "yst" , "ydt" ),	/* Yukon - name is no longer used */
  zd (- 500, "ast" , "adt" ),	/* Acre */
  zd (- 400, "wst" , "wdt" ),	/* Western Brazil */
  zd (- 400, "cst" , "cdt" ),	/* Chile */
  zd (- 200, "fst" , "fdt" ),	/* Fernando de Noronha */
  zs (  000, "wat" ),		/* West African */
  zs (  100, "cat" ),		/* Central African */
  zs (  200, "sat" ),		/* South African */
  zd (  200, "ist" , "idt" ),	/* Israel */
  zs (  300, "eat" ),		/* East African */
  zd (  300, "msk" , "msd" ),	/* Moscow */
  zd (  330, "ist" , "idt" ),	/* Iran */
  zs (  800, "hkt" ),		/* Hong Kong */
  zs (  800, "sgt" ),		/* Singapore */
  zd (  800, "cst" , "cdt" ),	/* China */
  zd (  800, "wst" , "wst" ),	/* Western Australia */
  zd (  930, "cst" , "cst" ),	/* Central Australia */
  zs ( 1000, "gst" ),		/* Guam */
  zd ( 1000, "est" , "est" ),	/* Eastern Australia */
#endif
  {"", -1}
};

/* Look for a prefix of S in TABLE, returning val for first matching entry.  */
static int
lookup (s, table)
     char const *s;
     struct name_val const table[];
{
  int j;
  char buf[NAME_LENGTH_MAXIMUM];

  for (j = 0; j < NAME_LENGTH_MAXIMUM; j++)
    {
      unsigned char c = *s++;
      if (! ISALPHA (c))
	{
	  buf[j] = '\0';
	  break;
	}
      buf[j] = ISUPPER (c) ? tolower (c) : c;
    }

  for (;; table++)
    for (j = 0; ; j++)
      if (j == NAME_LENGTH_MAXIMUM  ||  ! table[0].name[j])
	return table[0].val;
      else if (buf[j] != table[0].name[j])
	break;
}


/* Set *T to ``undefined'' values.  */
static void
undefine (t)
     struct partime *t;
{
  t->tm.tm_sec = t->tm.tm_min = t->tm.tm_hour = t->tm.tm_mday = t->tm.tm_mon
    = t->tm.tm_year = t->tm.tm_wday = t->tm.tm_yday
    = t->ymodulus = t->yweek
    = TM_UNDEFINED;
  t->zone = TM_UNDEFINED_ZONE;
}

/* Array of patterns to look for in a date string.
   Order is important: we look for the first matching pattern
   whose values do not contradict values that we already know about.
   See `parse_pattern_letter' below for the meaning of the pattern codes.  */
static char const *const patterns[] =
{
  /* These traditional patterns must come first,
     to prevent an ISO 8601 format from misinterpreting their prefixes.  */
  "E_n_y", "x", /* RFC 822 */
  "E_n", "n_E", "n", "t:m:s_A", "t:m_A", "t_A", /* traditional */
  "y/N/D$", /* traditional RCS */

  /* ISO 8601:1988 formats, generalized a bit.  */
  "y-N-D$", "4ND$", "Y-N$",
  "RND$", "-R=N$", "-R$", "--N=D$", "N=DT",
  "--N$", "---D$", "DT",
  "Y-d$", "4d$", "R=d$", "-d$", "dT",
  "y-W-X", "yWX", "y=W",
  "-r-W-X", "r-W-XT", "-rWX", "rWXT", "-W=X", "W=XT", "-W",
  "-w-X", "w-XT", "---X$", "XT", "4$",
  "T",
  "h:m:s$", "hms$", "h:m$", "hm$", "h$", "-m:s$", "-ms$", "-m$", "--s$",
  "Y", "Z",

  0
};

/* Parse an initial prefix of STR, setting *T accordingly.
   Return the first character after the prefix, or 0 if it couldn't be parsed.
   Start with pattern *PI; if success, set *PI to the next pattern to try.
   Set *PI to -1 if we know there are no more patterns to try;
   if *PI is initially negative, give up immediately.  */
static char const *
parse_prefix (str, t, pi)
     char const *str;
     struct partime *t;
     int *pi;
{
  int i = *pi;
  char const *pat;
  unsigned char c;

  if (i < 0)
    return 0;

  /* Remove initial noise.  */
  while (! ISALNUM (c = *str) && c != '-' && c != '+')
    {
      if (! c)
	{
	  undefine (t);
	  *pi = -1;
	  return str;
	}
      str++;
    }

  /* Try a pattern until one succeeds.  */
  while ((pat = patterns[i++]) != 0)
    {
      char const *s = str;
      undefine (t);
      do
	{
	  if (! (c = *pat++))
	    {
	      *pi = i;
	      return s;
	    }
	}
      while ((s = parse_pattern_letter (s, c, t)) != 0);
    }

  return 0;
}

/* Parse an initial prefix of S of length DIGITS; it must be a number.
   Store the parsed number into *RES.
   Return the first character after the prefix, or 0 if it wasn't parsed.  */
static char const *
parse_fixed (s, digits, res)
     char const *s;
     int digits, *res;
{
  int n = 0;
  char const *lim = s + digits;
  while (s < lim)
    {
      unsigned d = *s++ - '0';
      if (9 < d)
	return 0;
      n = 10 * n + d;
    }
  *res = n;
  return s;
}

/* Parse an initial prefix of S of length DIGITS;
   it must be a number in the range LO through HI.
   Store the parsed number into *RES.
   Return the first character after the prefix, or 0 if it wasn't parsed.  */
static char const *
parse_ranged (s, digits, lo, hi, res)
     char const *s;
     int digits, lo, hi, *res;
{
  s = parse_fixed (s, digits, res);
  return s && lo <= *res && *res <= hi ? s : 0;
}

/* Parse an initial prefix of S of length DIGITS;
   it must be a number in the range LO through HI
   and it may be followed by a fraction to be computed using RESOLUTION.
   Store the parsed number into *RES; store the fraction times RESOLUTION,
   rounded to the nearest integer, into *FRES.
   Return the first character after the prefix, or 0 if it wasn't parsed.  */
static char const *
parse_decimal (s, digits, lo, hi, resolution, res, fres)
     char const *s;
     int digits, lo, hi, resolution, *res, *fres;
{
  s = parse_fixed (s, digits, res);
  if (s && lo <= *res && *res <= hi)
    {
      int f = 0;
      if ((s[0] == ',' || s[0] == '.') && ISDIGIT (s[1]))
	{
	  char const *s1 = ++s;
	  int num10 = 0, denom10 = 10, product;
	  while (ISDIGIT (*++s))
	    {
	      int d = denom10 * 10;
	      if (d / 10  !=  denom10)
		return 0; /* overflow */
	      denom10 = d;
	    }
	  s = parse_fixed (s1, (int) (s - s1), &num10);
	  product = num10 * resolution;
	  f = (product + (denom10 >> 1)) / denom10;
	  f -= f & (product % denom10  ==  denom10 >> 1); /* round to even */
	  if (f < 0  ||  product/resolution != num10)
	    return 0; /* overflow */
	}
      *fres = f;
      return s;
    }
  return 0;
}

/* Parse an initial prefix of S; it must denote a time zone.
   Set *ZONE to the number of seconds east of GMT,
   or to TM_LOCAL_ZONE if it is the local time zone.
   Return the first character after the prefix, or 0 if it wasn't parsed.  */
char *
parzone (s, zone)
     char const *s;
     long *zone;
{
  char sign;
  int hh, mm, ss;
  int minutesEastOfUTC;
  long offset, z;

  /* The formats are LT, n, n DST, nDST, no, o
     where n is a time zone name
     and o is a time zone offset of the form [-+]hh[:mm[:ss]].  */
  switch (*s)
    {
    case '-':
    case '+':
      z = 0;
      break;

    default:
      minutesEastOfUTC = lookup (s, zone_names);
      if (minutesEastOfUTC == -1)
	return 0;

      /* Don't bother to check rest of spelling.  */
      while (ISALPHA ((unsigned char) *s))
	s++;

      /* Don't modify LT.  */
      if (minutesEastOfUTC == 1)
	{
	  *zone = TM_LOCAL_ZONE;
	  return (char *) s;
	}

      z = minutesEastOfUTC * 60L;

      /* Look for trailing " DST".  */
      if ((s[-1] == 'T' || s[-1] == 't')
	  && (s[-2] == 'S' || s[-2] == 's')
	  && (s[-3] == 'D' || s[-3] == 'd'))
	goto trailing_dst;
      while (ISSPACE ((unsigned char) *s))
	s++;
      if ((s[0] == 'D' || s[0] == 'd')
	  && (s[1] == 'S' || s[1] == 's')
	  && (s[2] == 'T' || s[2] == 't'))
	{
	  s += 3;
	trailing_dst:
	  *zone = z + 60*60;
	  return (char *) s;
	}

      switch (*s)
	{
	case '-':
	case '+':
	  break;

	default:
	  *zone = z;
	  return (char *) s;
	}

      break;
    }

  sign = *s++;

  if (! (s = parse_ranged (s, 2, 0, 23, &hh)))
    return 0;
  mm = ss = 0;
  if (*s == ':')
    s++;
  if (ISDIGIT (*s))
    {
      if (! (s = parse_ranged (s, 2, 0, 59, &mm)))
	return 0;
      if (*s == ':' && s[-3] == ':' && ISDIGIT (s[1])
	  && ! (s = parse_ranged (s + 1, 2, 0, 59, &ss)))
	return 0;
    }
  if (ISDIGIT (*s))
    return 0;
  offset = (hh * 60 + mm) * 60L + ss;
  *zone = z + (sign == '-' ? -offset : offset);
  /* ?? Are fractions allowed here?  If so, they're not implemented.  */
  return (char *) s;
}

/* Parse an initial prefix of S, matching the pattern whose code is C.
   Set *T accordingly.
   Return the first character after the prefix, or 0 if it wasn't parsed.  */
static char const *
parse_pattern_letter (s, c, t)
     char const *s;
     int c;
     struct partime *t;
{
  switch (c)
    {
    case '$': /* The next character must be a non-digit.  */
      if (ISDIGIT (*s))
	return 0;
      break;

    case '-':
    case '/':
    case ':':
      /* These characters stand for themselves.  */
      if (*s++ != c)
	return 0;
      break;

    case '4': /* 4-digit year */
      s = parse_fixed (s, 4, &t->tm.tm_year);
      break;

    case '=': /* optional '-' */
      s += *s == '-';
      break;

    case 'A': /* AM or PM */
      /* This matches the regular expression [AaPp][Mm]?.
         It must not be followed by a letter or digit;
         otherwise it would match prefixes of strings like "PST".  */
      switch (*s++)
	{
	case 'A':
	case 'a':
	  if (t->tm.tm_hour == 12)
	    t->tm.tm_hour = 0;
	  break;

	case 'P':
	case 'p':
	  if (t->tm.tm_hour != 12)
	    t->tm.tm_hour += 12;
	  break;

	default:
	  return 0;
	}
      switch (*s)
	{
	case 'M':
	case 'm':
	  s++;
	  break;
	}
      if (ISALNUM ((unsigned char) *s))
	return 0;
      break;

    case 'D': /* day of month [01-31] */
      s = parse_ranged (s, 2, 1, 31, &t->tm.tm_mday);
      break;

    case 'd': /* day of year [001-366] */
      s = parse_ranged (s, 3, 1, 366, &t->tm.tm_yday);
      t->tm.tm_yday--;
      break;

    case 'E': /* extended day of month [1-9, 01-31] */
      s = parse_ranged (s, (ISDIGIT (s[0]) && ISDIGIT (s[1])) + 1, 1, 31,
			&t->tm.tm_mday);
      break;

    case 'h': /* hour [00-23 followed by optional fraction] */
      {
	int frac;
	s = parse_decimal (s, 2, 0, 23, 60 * 60, &t->tm.tm_hour, &frac);
	t->tm.tm_min = frac / 60;
	t->tm.tm_sec = frac % 60;
      }
      break;

    case 'm': /* minute [00-59 followed by optional fraction] */
      s = parse_decimal (s, 2, 0, 59, 60, &t->tm.tm_min, &t->tm.tm_sec);
      break;

    case 'n': /* month name [e.g. "Jan"] */
      if (! TM_DEFINED (t->tm.tm_mon = lookup (s, month_names)))
	return 0;
      /* Don't bother to check rest of spelling.  */
      while (ISALPHA ((unsigned char) *s))
	s++;
      break;

    case 'N': /* month [01-12] */
      s = parse_ranged (s, 2, 1, 12, &t->tm.tm_mon);
      t->tm.tm_mon--;
      break;

    case 'r': /* year % 10 (remainder in origin-0 decade) [0-9] */
      s = parse_fixed (s, 1, &t->tm.tm_year);
      t->ymodulus = 10;
      break;

    case_R:
    case 'R': /* year % 100 (remainder in origin-0 century) [00-99] */
      s = parse_fixed (s, 2, &t->tm.tm_year);
      t->ymodulus = 100;
      break;

    case 's': /* second [00-60 followed by optional fraction] */
      {
	int frac;
	s = parse_decimal (s, 2, 0, 60, 1, &t->tm.tm_sec, &frac);
	t->tm.tm_sec += frac;
      }
      break;

    case 'T': /* 'T' or 't' */
      switch (*s++)
	{
	case 'T':
	case 't':
	  break;
	default:
	  return 0;
	}
      break;

    case 't': /* traditional hour [1-9 or 01-12] */
      s = parse_ranged (s, (ISDIGIT (s[0]) && ISDIGIT (s[1])) + 1, 1, 12,
			&t->tm.tm_hour);
      break;

    case 'w': /* 'W' or 'w' only (stands for current week) */
      switch (*s++)
	{
	case 'W':
	case 'w':
	  break;
	default:
	  return 0;
	}
      break;

    case 'W': /* 'W' or 'w', followed by a week of year [00-53] */
      switch (*s++)
	{
	case 'W':
	case 'w':
	  break;
	default:
	  return 0;
	}
      s = parse_ranged (s, 2, 0, 53, &t->yweek);
      break;

    case 'X': /* weekday (1=Mon ... 7=Sun) [1-7] */
      s = parse_ranged (s, 1, 1, 7, &t->tm.tm_wday);
      t->tm.tm_wday--;
      break;

    case 'x': /* weekday name [e.g. "Sun"] */
      if (! TM_DEFINED (t->tm.tm_wday = lookup (s, weekday_names)))
	return 0;
      /* Don't bother to check rest of spelling.  */
      while (ISALPHA ((unsigned char) *s))
	s++;
      break;

    case 'y': /* either R or Y */
      if (ISDIGIT (s[0]) && ISDIGIT (s[1]) && ! ISDIGIT (s[2]))
	goto case_R;
      /* fall into */
    case 'Y': /* year in full [4 or more digits] */
      {
	int len = 0;
	while (ISDIGIT (s[len]))
	  len++;
	if (len < 4)
	  return 0;
	s = parse_fixed (s, len, &t->tm.tm_year);
      }
      break;

    case 'Z': /* time zone */
      s = parzone (s, &t->zone);
      break;

    case '_': /* possibly empty sequence of non-alphanumerics */
      while (! ISALNUM ((unsigned char) *s) && *s)
	s++;
      break;

    default: /* bad pattern */
      return 0;
    }

  return s;
}

/* If there is no conflict, merge into *T the additional information in *U
   and return 0.  Otherwise do nothing and return -1.  */
static int
merge_partime (t, u)
     struct partime *t;
     struct partime const *u;
{
# define conflict(a,b) ((a) != (b)  &&  TM_DEFINED (a)  &&  TM_DEFINED (b))
  if (conflict (t->tm.tm_sec, u->tm.tm_sec)
      || conflict (t->tm.tm_min, u->tm.tm_min)
      || conflict (t->tm.tm_hour, u->tm.tm_hour)
      || conflict (t->tm.tm_mday, u->tm.tm_mday)
      || conflict (t->tm.tm_mon, u->tm.tm_mon)
      || conflict (t->tm.tm_year, u->tm.tm_year)
      || conflict (t->tm.tm_wday, u->tm.tm_yday)
      || conflict (t->ymodulus, u->ymodulus)
      || conflict (t->yweek, u->yweek)
      || (t->zone != u->zone
	  && t->zone != TM_UNDEFINED_ZONE
	  && u->zone != TM_UNDEFINED_ZONE))
    return -1;
# undef conflict
# define merge_(a,b) if (TM_DEFINED (b)) (a) = (b);
  merge_ (t->tm.tm_sec, u->tm.tm_sec)
  merge_ (t->tm.tm_min, u->tm.tm_min)
  merge_ (t->tm.tm_hour, u->tm.tm_hour)
  merge_ (t->tm.tm_mday, u->tm.tm_mday)
  merge_ (t->tm.tm_mon, u->tm.tm_mon)
  merge_ (t->tm.tm_year, u->tm.tm_year)
  merge_ (t->tm.tm_wday, u->tm.tm_yday)
  merge_ (t->ymodulus, u->ymodulus)
  merge_ (t->yweek, u->yweek)
# undef merge_
  if (u->zone != TM_UNDEFINED_ZONE)
    t->zone = u->zone;
  return 0;
}

/* Parse a date/time prefix of S, putting the parsed result into *T.
   Return the first character after the prefix.
   The prefix may contain no useful information;
   in that case, *T will contain only undefined values.  */
char *
partime (s, t)
     char const *s;
     struct partime *t;
{
  struct partime p;

  undefine (t);

  while (*s)
    {
      int i = 0;
      char const *s1;

      do
	{
	  if (! (s1 = parse_prefix (s, &p, &i)))
	    return (char *) s;
	}
      while (merge_partime (t, &p) != 0);

      s = s1;
    }

  return (char *) s;
}
