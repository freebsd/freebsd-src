/*
 * man.c
 *
 * Copyright (c) 1990, 1991, John W. Eaton.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with the man
 * distribution.
 *
 * John W. Eaton
 * jwe@che.utexas.edu
 * Department of Chemical Engineering
 * The University of Texas at Austin
 * Austin, Texas  78712
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#define MAN_MAIN

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#ifdef __FreeBSD__
#include <locale.h>
#include <langinfo.h>
#endif
#include <stdio.h>
#include <string.h>
#include <signal.h>
#if HAVE_LIBZ > 0
#include <zlib.h>
#endif
#include "config.h"
#include "gripes.h"
#include "version.h"

#ifdef POSIX
#include <unistd.h>
#else
#ifndef R_OK
#define R_OK 4
#endif
#endif

#ifdef SECURE_MAN_UID
extern uid_t getuid ();
extern int setuid ();
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
extern char *malloc ();
extern char *getenv ();
extern void free ();
extern int system ();
extern int strcmp ();
extern int strncmp ();
extern int exit ();
extern int fflush ();
extern int printf ();
extern int fprintf ();
extern FILE *fopen ();
extern int fclose ();
extern char *sprintf ();
#endif

extern char **glob_filename ();
extern int is_newer ();
extern int is_directory ();
extern int do_system_command ();

char *prognam;
static char *pager;
static char *machine;
static char *manp;
static char *manpathlist[MAXDIRS];
static char *shortsec;
static char *longsec;
static char *colon_sep_section_list;
static char **section_list;
static char *roff_directive;
static int apropos;
static int whatis;
static int findall;
static int print_where;

#ifdef __FreeBSD__
static char *locale, *locale_opts, *locale_nroff, *locale_codeset;
static char locale_terr[3], locale_lang[3];
static int use_original;
struct ltable {
	char *lcode;
	char *nroff;
};
static struct ltable ltable[] = {
	{"KOI8-R", "koi8-r"},
	{"ISO8859-1", "latin1"},
	{"ISO8859-15", "latin1"},
	{NULL}
};
#endif

static int troff = 0;

int debug;

#ifdef HAS_TROFF
#ifdef __FreeBSD__
static char args[] = "M:P:S:adfhkm:op:tw?";
#else
static char args[] = "M:P:S:adfhkm:p:tw?";
#endif
#else
#ifdef __FreeBSD__
static char args[] = "M:P:S:adfhkm:op:w?";
#else
static char args[] = "M:P:S:adfhkm:p:w?";
#endif
#endif

#ifdef SETREUID
uid_t ruid;
uid_t euid;
gid_t rgid;
gid_t egid;
#endif

int
main (argc, argv)
     int argc;
     char **argv;
{
  int status = 0;
  char *nextarg;
  char *tmp;
  extern char *mkprogname ();
  char *is_section ();
  char **get_section_list ();
  void man_getopt ();
  void do_apropos ();
  void do_whatis ();
  int man ();

  prognam = mkprogname (argv[0]);
  longsec = NULL;

  unsetenv("IFS");
#ifdef __FreeBSD__
  (void) setlocale(LC_ALL, "");
#endif
  man_getopt (argc, argv);

  if (optind == argc)
    gripe_no_name ((char *)NULL);

  section_list = get_section_list ();

  if (optind == argc - 1)
    {
      tmp = is_section (argv[optind], manp);

      if (tmp != NULL)
	gripe_no_name (tmp);
    }

#ifdef SETREUID
  ruid = getuid();
  rgid = getgid();
  euid = geteuid();
  egid = getegid();
  setreuid(-1, ruid);
  setregid(-1, rgid);
#endif

  while (optind < argc)
    {
      nextarg = argv[optind++];

      /*
       * See if this argument is a valid section name.  If not,
       * is_section returns NULL.
       */
      tmp = is_section (nextarg, manp);

      if (tmp != NULL)
	{
	  shortsec = tmp;

	  if (debug)
	    fprintf (stderr, "\nsection: %s\n", shortsec);

	  continue;
	}

      if (apropos) {
	do_apropos (nextarg);
	status = (status ? 0 : 1); /* reverts status, see below */
      }
      else if (whatis) {
	do_whatis (nextarg);
	status = (status ? 0 : 1); /* reverts status, see below */
      }
      else
	{
	  status = man (nextarg);

	  if (status == 0)
	    gripe_not_found (nextarg, longsec);
	}
    }
  return (status==0);         /* status==1 --> exit(0),
                                 status==0 --> exit(1) */
}

void
usage ()
{
  static char usage_string[1024] = "%s, version %s\n\n";

#ifdef HAS_TROFF
#ifdef __FreeBSD__
  static char s1[] =
    "usage: %s [-adfhkotw] [section] [-M path] [-P pager] [-S list]\n\
           [-m machine] [-p string] name ...\n\n";
#else
  static char s1[] =
    "usage: %s [-adfhktw] [section] [-M path] [-P pager] [-S list]\n\
           [-m machine] [-p string] name ...\n\n";
#endif
#else
#ifdef __FreeBSD__
  static char s1[] =
    "usage: %s [-adfhkow] [section] [-M path] [-P pager] [-S list]\n\
           [-m machine] [-p string] name ...\n\n";
#else
  static char s1[] =
    "usage: %s [-adfhkw] [section] [-M path] [-P pager] [-S list]\n\
           [-m machine] [-p string] name ...\n\n";
#endif
#endif

static char s2[] = "  a : find all matching entries\n\
  d : print gobs of debugging information\n\
  f : same as whatis(1)\n\
  h : print this help message\n\
  k : same as apropos(1)\n";

#ifdef __FreeBSD__
  static char s3[] = "  o : use original, non-localized manpages\n";
#endif

#ifdef HAS_TROFF
  static char s4[] = "  t : use troff to format pages for printing\n";
#endif

  static char s5[] = "  w : print location of man page(s) that would be displayed\n\n\
  M path    : set search path for manual pages to `path'\n\
  P pager   : use program `pager' to display pages\n\
  S list    : colon separated section list\n\
  m machine : search for alternate architecture man pages\n";

  static char s6[] = "  p string : string tells which preprocessors to run\n\
               e - [n]eqn(1)   p - pic(1)    t - tbl(1)\n\
               g - grap(1)     r - refer(1)  v - vgrind(1)\n";

  strcat (usage_string, s1);
  strcat (usage_string, s2);
#ifdef __FreeBSD__
  strcat (usage_string, s3);
#endif

#ifdef HAS_TROFF
  strcat (usage_string, s4);
#endif

  strcat (usage_string, s5);

  strcat (usage_string, s6);

  fprintf (stderr, usage_string, prognam, version, prognam);
  exit(1);
}

char **
add_dir_to_mpath_list (mp, p)
     char **mp;
     char *p;
{
  int status;

  status = is_directory (p);

  if (status < 0 && debug)
    {
      fprintf (stderr, "Warning: couldn't stat file %s!\n", p);
    }
  else if (status == 0 && debug)
    {
      fprintf (stderr, "Warning: %s isn't a directory!\n", p);
    }
  else if (status == 1)
    {
      if (debug)
	fprintf (stderr, "adding %s to manpathlist\n", p);

      *mp++ = strdup (p);
    }
  return mp;
}

/*
 * Get options from the command line and user environment.
 */
void
man_getopt (argc, argv)
     register int argc;
     register char **argv;
{
  register int c;
  register char *p;
  register char *end;
  register char **mp;
  extern void downcase ();
  extern char *manpath ();

  while ((c = getopt (argc, argv, args)) != EOF)
    {
      switch (c)
	{
	case 'M':
	  manp = strdup (optarg);
	  break;
	case 'P':
	  pager = strdup (optarg);
	  if (setenv("PAGER", pager, 1) != 0)
		  (void)fprintf(stderr, "setenv PAGER=%s\n", pager);
	  break;
	case 'S':
	  colon_sep_section_list = strdup (optarg);
	  break;
	case 'a':
	  findall++;
	  break;
	case 'd':
	  debug++;
	  break;
	case 'f':
	  if (troff)
	    gripe_incompatible ("-f and -t");
	  if (apropos)
	    gripe_incompatible ("-f and -k");
	  if (print_where)
	    gripe_incompatible ("-f and -w");
	  whatis++;
	  break;
	case 'k':
	  if (troff)
	    gripe_incompatible ("-k and -t");
	  if (whatis)
	    gripe_incompatible ("-k and -f");
	  if (print_where)
	    gripe_incompatible ("-k and -w");
	  apropos++;
	  break;
	case 'm':
	  machine = optarg;
	  break;
#ifdef __FreeBSD__
	case 'o':
	  use_original++;
	  break;
#endif
	case 'p':
	  roff_directive = strdup (optarg);
	  break;
#ifdef HAS_TROFF
	case 't':
	  if (apropos)
	    gripe_incompatible ("-t and -k");
	  if (whatis)
	    gripe_incompatible ("-t and -f");
	  if (print_where)
	    gripe_incompatible ("-t and -w");
	  troff++;
	  break;
#endif
	case 'w':
	  if (apropos)
	    gripe_incompatible ("-w and -k");
	  if (whatis)
	    gripe_incompatible ("-w and -f");
	  if (troff)
	    gripe_incompatible ("-w and -t");
	  print_where++;
	  break;
	case 'h':
	case '?':
	default:
	  usage();
	  break;
	}
    }

#ifdef __FreeBSD__
  /* "" intentionally used to catch error */
  if ((locale = setlocale(LC_CTYPE, "")) != NULL)
	locale_codeset = nl_langinfo(CODESET);
  if (!use_original && locale != NULL && *locale_codeset != '\0' &&
      strcmp(locale_codeset, "US-ASCII") != 0
     ) {
	char *tmp, *short_locale;
	struct ltable *pltable;

	*locale_lang = '\0';
	*locale_terr = '\0';

	if ((short_locale = strdup(locale)) == NULL) {
		perror ("ctype locale strdup");
		exit (1);
	}
	if ((tmp = strchr(short_locale, '.')) != NULL)
		*tmp = '\0';

	if (strlen(short_locale) == 2)
		strcpy(locale_lang, short_locale);
	else if ((tmp = strchr(short_locale, '_')) == NULL ||
		 tmp != short_locale + 2 ||
		 strlen(tmp + 1) != 2
		) {
		errno = EINVAL;
		perror ("ctype locale format");
		locale = NULL;
	} else {
		strncpy(locale_terr, short_locale + 3, 2);
		locale_terr[2] = '\0';
		strncpy(locale_lang, short_locale, 2);
		locale_lang[2] = '\0';
	}

	free(short_locale);

	if (locale != NULL) {
		for (pltable = ltable; pltable->lcode != NULL; pltable++) {
			if (strcmp(pltable->lcode, locale_codeset) == 0) {
				locale_nroff = pltable->nroff;
				break;
			}
		}
	}
  } else {
	if (locale == NULL) {
		errno = EINVAL;
		perror ("ctype locale");
	} else {
		locale = NULL;
		if (*locale_codeset == '\0') {
			errno = EINVAL;
			perror ("ctype codeset");
		}
	}
  }
#endif /* __FreeBSD__ */

  if (pager == NULL || *pager == '\0')
    if ((pager = getenv ("PAGER")) == NULL || *pager == '\0')
      pager = strdup (PAGER);

  if (debug)
    fprintf (stderr, "\nusing %s as pager\n", pager);

  if (machine == NULL && (machine = getenv ("MACHINE")) == NULL)
    machine = MACHINE;

  if (debug)
    fprintf (stderr, "\nusing %s architecture\n", machine);

  if (manp == NULL)
    {
      if ((manp = manpath (0)) == NULL)
	gripe_manpath ();

      if (debug)
	fprintf (stderr,
		 "\nsearch path for pages determined by manpath is\n%s\n\n",
		 manp);
    }

  /*
   * Expand the manpath into a list for easier handling.
   */
  mp = manpathlist;
  for (p = manp; ; p = end+1)
    {
      if (mp == manpathlist + MAXDIRS - 1) {
	fprintf (stderr, "Warning: too many directories in manpath, truncated!\n");
	break;
      }
      if ((end = strchr (p, ':')) != NULL)
	*end = '\0';

      mp = add_dir_to_mpath_list (mp, p);
      if (end == NULL)
	break;

      *end = ':';
    }
  *mp = NULL;
}

/*
 * Check to see if the argument is a valid section number.  If the
 * first character of name is a numeral, or the name matches one of
 * the sections listed in section_list, we'll assume that it's a section.
 * The list of sections in config.h simply allows us to specify oddly
 * named directories like .../man3f.  Yuk.
 */
char *
is_section (name, path)
     char *name;
     char *path;
{
  register char **vs;
  char *temp, *end, *loc;
  char **plist;
  int x;

  for (vs = section_list; *vs != NULL; vs++)
    if ((strcmp (*vs, name) == 0)
	|| (isdigit ((unsigned char)name[0]) && strlen(name) == 1))
      return (longsec = strdup (name));

  plist = manpathlist;
  if (isdigit ((unsigned char)name[0]))
    {
      while (*plist != NULL)
	{
	  asprintf(&temp, "%s/man%c/*", *plist, name[0]);
	  plist++;

	  x = 0;
	  vs = glob_filename (temp);
	  if ((int)vs == -1)
	    {
	      free (temp);
	      return NULL;
	    }
	  for ( ; *vs != NULL; vs++)
	    {
	      end = strrchr (*vs, '/');
	      if ((loc = strstr (end, name)) != NULL && loc - end > 2
		  && *(loc-1) == '.'
		  && (*(loc+strlen(name)) == '\0' || *(loc+strlen(name)) == '.'))
		{
		  x = 1;
		  break;
		}
	    }
	  free (temp);
	  if (x == 1)
	    {
	      asprintf(&temp, "%c", name[0]);
	      longsec = strdup (name);
	      return (temp);
	    }
	}
    }
  return NULL;
}

/*
 * Handle the apropos option.  Cheat by using another program.
 */
void
do_apropos (name)
     register char *name;
{
  register int len;
  register char *command;

  len = strlen (APROPOS) + strlen (name) + 4;

  if ((command = (char *) malloc(len)) == NULL)
    gripe_alloc (len, "command");

  sprintf (command, "%s \"%s\"", APROPOS, name);

  (void) do_system_command (command);

  free (command);
}

/*
 * Handle the whatis option.  Cheat by using another program.
 */
void
do_whatis (name)
     register char *name;
{
  register int len;
  register char *command;

  len = strlen (WHATIS) + strlen (name) + 4;

  if ((command = (char *) malloc(len)) == NULL)
    gripe_alloc (len, "command");

  sprintf (command, "%s \"%s\"", WHATIS, name);

  (void) do_system_command (command);

  free (command);
}

/*
 * Change a name of the form ...man/man1/name.1 to ...man/cat1/name.1
 * or a name of the form ...man/cat1/name.1 to ...man/man1/name.1
 */
char *
convert_name (name, to_cat)
     register char *name;
     register int to_cat;
{
  register char *to_name;
  register char *t1;
  register char *t2 = NULL;

#ifdef DO_COMPRESS
  if (to_cat)
    {
      int olen = strlen(name);
      int cextlen = strlen(COMPRESS_EXT);
      int len = olen + cextlen;

      to_name = malloc (len+1);
      if (to_name == NULL)
	gripe_alloc (len+1, "to_name");
      strcpy (to_name, name);
      olen -= cextlen;
      /* Avoid tacking it on twice */
      if (olen >= 1 && strcmp(name + olen, COMPRESS_EXT) != 0)
      	strcat (to_name, COMPRESS_EXT);
    }
  else
    to_name = strdup (name);
#else
  to_name = strdup (name);
#endif

  t1 = strrchr (to_name, '/');
  if (t1 != NULL)
    {
      *t1 = '\0';
      t2 = strrchr (to_name, '/');
      *t1 = '/';

      /* Skip architecture part (if present). */
      if (t2 != NULL && (t1 - t2 < 5 || *(t2 + 1) != 'm' || *(t2 + 3) != 'n'))
	{
	  t1 = t2;
	  *t1 = '\0';
	  t2 = strrchr (to_name, '/');
	  *t1 = '/';
	}
    }

  if (t2 == NULL)
    gripe_converting_name (name, to_cat);

  if (to_cat)
    {
      *(++t2) = 'c';
      *(t2+2) = 't';
    }
  else
    {
      *(++t2) = 'm';
      *(t2+2) = 'n';
    }

  if (debug)
    fprintf (stderr, "to_name in convert_name () is: %s\n", to_name);

  return to_name;
}

/*
 * Try to find the man page corresponding to the given name.  The
 * reason we do this with globbing is because some systems have man
 * page directories named man3 which contain files with names like
 * XtPopup.3Xt.  Rather than requiring that this program know about
 * all those possible names, we simply try to match things like
 * .../man[sect]/name[sect]*.  This is *much* easier.
 *
 * Note that globbing is only done when the section is unspecified.
 */
char **
glob_for_file (path, section, longsec, name, cat)
     char *path;
     char *section;
     char *longsec;
     char *name;
     int cat;
{
  char pathname[FILENAME_MAX];
  char **gf;

  if (longsec == NULL)
    longsec = section;

  if (cat)
    snprintf (pathname, sizeof(pathname), "%s/cat%s/%s.%s*", path, section,
       name, longsec);
  else
    snprintf (pathname, sizeof(pathname), "%s/man%s/%s.%s*", path, section,
       name, longsec);

  if (debug)
    fprintf (stderr, "globbing %s\n", pathname);

  gf = glob_filename (pathname);

  if ((gf == (char **) -1 || *gf == NULL) && isdigit ((unsigned char)*section)
      && strlen (longsec) == 1)
    {
      if (cat)
	snprintf (pathname, sizeof(pathname), "%s/cat%s/%s.%c*", path, section, name, *section);
      else
	snprintf (pathname, sizeof(pathname), "%s/man%s/%s.%c*", path, section, name, *section);

      gf = glob_filename (pathname);
    }
  if ((gf == (char **) -1 || *gf == NULL) && isdigit ((unsigned char)*section)
      && strlen (longsec) == 1)
    {
      if (cat)
	snprintf (pathname, sizeof(pathname), "%s/cat%s/%s.0*", path, section, name);
      else
	snprintf (pathname, sizeof(pathname), "%s/man%s/%s.0*", path, section, name);
      if (debug)
	fprintf (stderr, "globbing %s\n", pathname);
      gf = glob_filename (pathname);
    }
  return gf;
}

/*
 * Return an un-globbed name in the same form as if we were doing
 * globbing.
 */
char **
make_name (path, section, longsec, name, cat)
     char *path;
     char *section;
     char *longsec;
     char *name;
     int cat;
{
  register int i = 0;
  static char *names[3];
  char buf[FILENAME_MAX];

  if (cat)
    snprintf (buf, sizeof(buf), "%s/cat%s/%s.%s", path, section, name, longsec);
  else
    snprintf (buf, sizeof(buf), "%s/man%s/%s.%s", path, section, name, longsec);

  if (access (buf, R_OK) == 0)
    names[i++] = strdup (buf);

  /*
   * If we're given a section that looks like `3f', we may want to try
   * file names like .../man3/foo.3f as well.  This seems a bit
   * kludgey to me, but what the hey...
   */
  if (section[1] != '\0')
    {
      if (cat)
	snprintf (buf, sizeof(buf), "%s/cat%c/%s.%s", path, section[0], name, section);
      else
	snprintf (buf, sizeof(buf), "%s/man%c/%s.%s", path, section[0], name, section);

      if (access (buf, R_OK) == 0)
	names[i++] = strdup (buf);
    }

  names[i] = NULL;

  return &names[0];
}

char *
get_expander (file)
     char *file;
{
  char *end = file + (strlen (file) - 1);

  while (end > file && end[-1] != '.')
    --end;
  if (end == file)
    return NULL;
#ifdef FCAT
  if (*end == 'F')
    return FCAT;
#endif	/* FCAT */
#ifdef YCAT
  if (*end == 'Y')
    return YCAT;
#endif	/* YCAT */
#ifdef ZCAT
  if (*end == 'Z' || !strcmp(end, "gz"))
    return ZCAT;
#endif	/* ZCAT */
  return NULL;
}

/*
 * Simply display the preformatted page.
 */
int
display_cat_file (file)
     register char *file;
{
  register int found;
  char command[FILENAME_MAX];

  found = 0;

  if (access (file, R_OK) == 0)
    {
      char *expander = get_expander (file);

      if (expander != NULL)
	snprintf (command, sizeof(command), "%s %s | %s", expander, file, pager);
      else
	snprintf (command, sizeof(command), "%s %s", pager, file);

      found = do_system_command (command);
    }
  return found;
}

/*
 * Try to find the ultimate source file.  If the first line of the
 * current file is not of the form
 *
 *      .so man3/printf.3s
 *
 * the input file name is returned.
 */
char *
ultimate_source (name, path)
     char *name;
     char *path;
{
  static  char buf[BUFSIZ];
  static  char ult[FILENAME_MAX];

  FILE *fp;
  char *beg;
  char *end;

  strncpy (ult, name, sizeof(ult)-1);
  ult[sizeof(ult)-1] = '\0';
  strncpy (buf, name, sizeof(buf)-1);
  ult[sizeof(buf)-1] = '\0';

 next:

  if ((fp = fopen (ult, "r")) == NULL)
    return ult;

  end = fgets (buf, BUFSIZ, fp);
  fclose(fp);

  if (!end || strlen (buf) < 5)
    return ult;

  beg = buf;
  if (*beg++ == '.' && *beg++ == 's' && *beg++ == 'o')
    {
      while ((*beg == ' ' || *beg == '\t') && *beg != '\0')
	beg++;

      end = beg;
      while (*end != ' ' && *end != '\t' && *end != '\n' && *end != '\0')
	end++;

      *end = '\0';

      snprintf(ult, sizeof(ult), "%s/%s", path, beg);
      snprintf(buf, sizeof(buf), "%s", ult);

      goto next;
    }

  if (debug)
    fprintf (stderr, "found ultimate source file %s\n", ult);

  return ult;
}

void
add_directive (first, d, file, buf, bufsize)
     int *first;
     char *d;
     char *file;
     char *buf;
     int bufsize;
{
  if (strcmp (d, "") != 0)
    {
      if (*first)
	{
	  *first = 0;
	  snprintf(buf, bufsize, "%s %s", d, file);
	}
      else
	{
	  strncat (buf, " | ", bufsize-strlen(buf)-1);
	  strncat (buf, d, bufsize-strlen(buf)-1);
	}
    }
}

int
parse_roff_directive (cp, file, buf, bufsize)
  char *cp;
  char *file;
  char *buf;
  int bufsize;
{
  char c;
  char *exp;
  int first = 1;
  int preproc_found = 0;
  int use_col = 0;

  if ((exp = get_expander(file)) != NULL)
	add_directive (&first, exp, file, buf, bufsize);

  while ((c = *cp++) != '\0')
    {
      switch (c)
	{
	case 'e':

	  if (debug)
	    fprintf (stderr, "found eqn(1) directive\n");

	  preproc_found++;
	  if (troff)
	    add_directive (&first, EQN, file, buf, bufsize);
	  else {
#ifdef __FreeBSD__
	    char lbuf[FILENAME_MAX];

	    snprintf(lbuf, sizeof(lbuf), "%s -T%s", NEQN,
		     locale_opts == NULL ? "ascii" : locale_opts);
	    add_directive (&first, lbuf, file, buf, bufsize);
#else
	    add_directive (&first, NEQN, file, buf, bufsize);
#endif
	  }

	  break;

	case 'g':

	  if (debug)
	    fprintf (stderr, "found grap(1) directive\n");

	  preproc_found++;
	  add_directive (&first, GRAP, file, buf, bufsize);

	  break;

	case 'p':

	  if (debug)
	    fprintf (stderr, "found pic(1) directive\n");

	  preproc_found++;
	  add_directive (&first, PIC, file, buf, bufsize);

	  break;

	case 't':

	  if (debug)
	    fprintf (stderr, "found tbl(1) directive\n");

	  preproc_found++;
	  use_col++;
	  add_directive (&first, TBL, file, buf, bufsize);
	  break;

	case 'v':

	  if (debug)
	    fprintf (stderr, "found vgrind(1) directive\n");

	  add_directive (&first, VGRIND, file, buf, bufsize);
	  break;

	case 'r':

	  if (debug)
	    fprintf (stderr, "found refer(1) directive\n");

	  add_directive (&first, REFER, file, buf, bufsize);
	  break;

	case ' ':
	case '\t':
	case '\n':

	  goto done;

	default:

	  return -1;
	}
    }

 done:

#ifdef HAS_TROFF
  if (troff)
    add_directive (&first, TROFF, file, buf, bufsize);
  else
#endif
    {
#ifdef __FreeBSD__
      char lbuf[FILENAME_MAX];

      snprintf(lbuf, sizeof(lbuf), "%s -T%s", NROFF,
	       locale_opts == NULL ? "ascii" : locale_opts);
	    add_directive (&first, lbuf, file, buf, bufsize);
#else
      add_directive (&first, NROFF " -Tascii", file, buf, bufsize);
#endif
    }
  if (use_col && !troff)
      add_directive (&first, COL, file, buf, bufsize);

  if (preproc_found)
    return 0;
  else
    return 1;
}

char *
make_roff_command (file)
     char *file;
{
#if HAVE_LIBZ > 0
  gzFile fp;
#else
  FILE *fp;
#endif
  char line [BUFSIZ];
  static char buf [BUFSIZ];
  int status;
  char *cp;

  if (roff_directive != NULL)
    {
      if (debug)
	fprintf (stderr, "parsing directive from command line\n");

      status = parse_roff_directive (roff_directive, file, buf, sizeof(buf));

      if (status == 0)
	return buf;

      if (status == -1)
	gripe_roff_command_from_command_line (file);
    }

#if HAVE_LIBZ > 0
  if ((fp = gzopen (file, "r")) != NULL)
#else
  if ((fp = fopen (file, "r")) != NULL)
#endif
    {
      cp = line;
#if HAVE_LIBZ > 0
      gzgets (fp, line, BUFSIZ);
      gzclose(fp);
#else
      fgets (line, BUFSIZ, fp);
      fclose(fp);
#endif
      if (*cp++ == '\'' && *cp++ == '\\' && *cp++ == '"' && *cp++ == ' ')
	{
	  if (debug)
	    fprintf (stderr, "parsing directive from file\n");

	  status = parse_roff_directive (cp, file, buf, sizeof(buf));

	  if (status == 0)
	    return buf;

	  if (status == -1)
	    gripe_roff_command_from_file (file);
	}
    }
  else
    {
      /*
       * Is there really any point in continuing to look for
       * preprocessor options if we can't even read the man page source?
       */
      gripe_reading_man_file (file);
      return NULL;
    }

  if ((cp = getenv ("MANROFFSEQ")) != NULL)
    {
      if (debug)
	fprintf (stderr, "parsing directive from environment\n");

      status = parse_roff_directive (cp, file, buf, sizeof(buf));

      if (status == 0)
	return buf;

      if (status == -1)
	gripe_roff_command_from_env ();
    }

  if (debug)
    fprintf (stderr, "using default preprocessor sequence\n");

  status = parse_roff_directive ("t", file, buf, sizeof(buf));
  if (status >= 0)
    return buf;
  else		/* can't happen */
    return NULL;
}

sig_t ohup, oint, oquit, oterm;
static char temp[FILENAME_MAX];

void cleantmp()
{
	unlink(temp);
	exit(1);
}

void
set_sigs()
{
  ohup = signal(SIGHUP, cleantmp);
  oint = signal(SIGINT, cleantmp);
  oquit = signal(SIGQUIT, cleantmp);
  oterm = signal(SIGTERM, cleantmp);
}

void
restore_sigs()
{
  signal(SIGHUP, ohup);
  signal(SIGINT, oint);
  signal(SIGQUIT, oquit);
  signal(SIGTERM, oterm);
}

/*
 * Try to format the man page and create a new formatted file.  Return
 * 1 for success and 0 for failure.
 */
int
make_cat_file (path, man_file, cat_file, manid)
     register char *path;
     register char *man_file;
     register char *cat_file;
{
  int s, f;
  FILE *fp, *pp;
  char *roff_command;
  char command[FILENAME_MAX];

  roff_command = make_roff_command (man_file);
  if (roff_command == NULL)
      return 0;

  snprintf(temp, sizeof(temp), "%s.tmpXXXXXX", cat_file);
  if ((f = mkstemp(temp)) >= 0 && (fp = fdopen(f, "w")) != NULL)
    {
      set_sigs();

      if (fchmod (f, CATMODE) < 0) {
	perror("fchmod");
	unlink(temp);
	restore_sigs();
	fclose(fp);
	return 0;
      } else if (debug)
	fprintf (stderr, "mode of %s is now %o\n", temp, CATMODE);

#ifdef DO_COMPRESS
      snprintf (command, sizeof(command), "(cd %s ; %s | %s)", path,
		roff_command, COMPRESSOR);
#else
      snprintf (command, sizeof(command), "(cd %s ; %s)", path,
		roff_command);
#endif
      fprintf (stderr, "Formatting page, please wait...");
      fflush(stderr);

      if (debug)
	fprintf (stderr, "\ntrying command: %s\n", command);
      else {

#ifdef SETREUID
	if (manid) {
	  setreuid(-1, ruid);
	  setregid(-1, rgid);
	}
#endif
	if ((pp = popen(command, "r")) == NULL) {
	  s = errno;
	  fprintf(stderr, "Failed.\n");
	  errno = s;
	  perror("popen");
#ifdef SETREUID
	  if (manid) {
	    setreuid(-1, euid);
	    setregid(-1, egid);
	  }
#endif
	  unlink(temp);
	  restore_sigs();
	  fclose(fp);
	  return 0;
	}
#ifdef SETREUID
	if (manid) {
	  setreuid(-1, euid);
	  setregid(-1, egid);
	}
#endif

	f = 0;
	while ((s = getc(pp)) != EOF) {
	  putc(s, fp); f++;
	}

	if (!f || ((s = pclose(pp)) == -1)) {
	  s = errno;
	  fprintf(stderr, "Failed.\n");
	  errno = s;
	  perror("pclose");
	  unlink(temp);
	  restore_sigs();
	  fclose(fp);
	  return 0;
	}

	if (s != 0) {
	  fprintf(stderr, "Failed.\n");
	  gripe_system_command(s);
	  unlink(temp);
	  restore_sigs();
	  fclose(fp);
	  return 0;
	}
      }

      if (debug)
	unlink(temp);
      else if (rename(temp, cat_file) == -1) {
	s = errno;
	fprintf(stderr,
		 "\nHmm!  Can't seem to rename %s to %s, check permissions on man dir!\n",
		 temp, cat_file);
	errno = s;
	perror("rename");
	unlink(temp);
	restore_sigs();
	fclose(fp);
	return 0;
      }
      restore_sigs();

      if (fclose(fp)) {
	s = errno;
	if (!debug)
	  unlink(cat_file);
	fprintf(stderr, "Failed.\n");
	errno = s;
	perror("fclose");
	return 0;
      }

      if (debug) {
	fprintf(stderr, "No output, debug mode.\n");
	return 0;
      }

      fprintf(stderr, "Done.\n");

      return 1;
    }
  else
    {
      if (f >= 0) {
	s = errno;
	unlink(temp);
	errno = s;
      }
      if (debug) {
	s = errno;
	fprintf (stderr, "Couldn't open %s for writing.\n", temp);
	errno = s;
      }
      if (f >= 0) {
	perror("fdopen");
	close(f);
      }

      return 0;
    }
}

/*
 * Try to format the man page source and save it, then display it.  If
 * that's not possible, try to format the man page source and display
 * it directly.
 *
 * Note that we've already been handed the name of the ultimate source
 * file at this point.
 */
int
format_and_display (path, man_file, cat_file)
     register char *path;
     register char *man_file;
     register char *cat_file;
{
  int status;
  register int found;
  char *roff_command;
  char command[FILENAME_MAX];

  found = 0;

  if (access (man_file, R_OK) != 0)
    return 0;

  if (troff)
    {
      roff_command = make_roff_command (man_file);
      if (roff_command == NULL)
	return 0;
      else
	snprintf (command, sizeof(command), "(cd %s ; %s)", path, roff_command);

      found = do_system_command (command);
    }
  else
    {
      status = is_newer (man_file, cat_file);
      if (debug)
	fprintf (stderr, "status from is_newer() = %d\n", status);

      if (status == 1 || status == -2)
	{
	  /*
	   * Cat file is out of date.  Try to format and save it.
	   */
	  if (print_where)
	    {
	      printf ("%s\n", man_file);
	      found++;
	    }
	  else
	    {

#ifdef SETREUID
	      setreuid(-1, euid);
	      setregid(-1, egid);
	      found = make_cat_file (path, man_file, cat_file, 1);
#else
	      found = make_cat_file (path, man_file, cat_file, 0);
#endif
#ifdef SETREUID
	      setreuid(-1, ruid);
	      setregid(-1, rgid);

	      if (!found)
	        {
		  /* Try again as real user - see note below.
		     By running with
		       effective group (user) ID == real group (user) ID
		     except for the call above, I believe the problems
		     of reading private man pages is avoided.  */
		  found = make_cat_file (path, man_file, cat_file, 0);
	        }
#endif
#ifdef SECURE_MAN_UID
	      if (!found)
		{
		  /*
		   * Try again as real user.  Note that for private
		   * man pages, we won't even get this far unless the
		   * effective user can read the real user's man page
		   * source.  Also, if we are trying to find all the
		   * man pages, this will probably make it impossible
		   * to make cat files in the system directories if
		   * the real user's man directories are searched
		   * first, because there's no way to undo this (is
		   * there?).  Yikes, am I missing something obvious?
		   */
		  setuid (getuid ());

		  found = make_cat_file (path, man_file, cat_file, 0);
		}
#endif
	      if (found)
		{
		  /*
		   * Creating the cat file worked.  Now just display it.
		   */
		  (void) display_cat_file (cat_file);
		}
	      else
		{
		  /*
		   * Couldn't create cat file.  Just format it and
		   * display it through the pager.
		   */
		  roff_command = make_roff_command (man_file);
		  if (roff_command == NULL)
		    return 0;
		  else
		    snprintf (command, sizeof(command), "(cd %s ; %s | %s)", path,
			     roff_command, pager);

		  found = do_system_command (command);
		}
	    }
	}
      else if (access (cat_file, R_OK) == 0)
	{
	  /*
	   * Formatting not necessary.  Cat file is newer than source
	   * file, or source file is not present but cat file is.
	   */
	  if (print_where)
	    {
	      printf ("%s (source: %s)\n", cat_file, man_file);
	      found++;
	    }
	  else
	    {
	      found = display_cat_file (cat_file);
	    }
	}
    }
  return found;
}

/*
 * See if the preformatted man page or the source exists in the given
 * section.
 */
int
try_section (path, section, longsec, name, glob)
     char *path;
     char *section;
     char *longsec;
     char *name;
     int glob;
{
  register int found = 0;
  register int to_cat;
  register int cat;
  register char **names;
  register char **np;
  static int arch_search;
  char buf[FILENAME_MAX];

  if (!arch_search)
    {
      snprintf(buf, sizeof(buf), "%s/man%s/%s", path, section, machine);
      if (is_directory (buf) == 1)
	{
	  snprintf(buf, sizeof(buf), "%s/%s", machine, name);
	  arch_search++;
	  found = try_section (path, section, longsec, buf, glob);
	  arch_search--;
	  if (found && !findall)   /* only do this architecture... */
	    return found;
	}
    }

  if (debug)
    {
      if (glob)
	fprintf (stderr, "trying section %s with globbing\n", section);
      else
	fprintf (stderr, "trying section %s without globbing\n", section);
    }

#ifndef NROFF_MISSING
  /*
   * Look for man page source files.
   */
  cat = 0;
  if (glob)
    names = glob_for_file (path, section, longsec, name, cat);
  else
    names = make_name (path, section, longsec, name, cat);

  if (names == (char **) -1 || *names == NULL)
    /*
     * No files match.  See if there's a preformatted page around that
     * we can display.
     */
#endif /* NROFF_MISSING */
    {
      if (!troff)
	{
	  cat = 1;
	  if (glob)
	    names = glob_for_file (path, section, longsec, name, cat);
	  else
	    names = make_name (path, section, longsec, name, cat);

	  if (names != (char **) -1 && *names != NULL)
	    {
	      for (np = names; *np != NULL; np++)
		{
		  if (print_where)
		    {
		      printf ("%s\n", *np);
		      found++;
		    }
		  else
		    {
		      found += display_cat_file (*np);
		    }
		}
	    }
	}
    }
#ifndef NROFF_MISSING
  else
    {
      for (np = names; *np != NULL; np++)
	{
	  register char *cat_file = NULL;
	  register char *man_file;

	  man_file = ultimate_source (*np, path);

	  if (!troff)
	    {
	      to_cat = 1;

	      cat_file = convert_name (man_file, to_cat);

	      if (debug)
		fprintf (stderr, "will try to write %s if needed\n", cat_file);
	    }

	  found += format_and_display (path, man_file, cat_file);
	}
    }
#endif /* NROFF_MISSING */
  return found;
}

/*
 * Search for manual pages.
 *
 * If preformatted manual pages are supported, look for the formatted
 * file first, then the man page source file.  If they both exist and
 * the man page source file is newer, or only the source file exists,
 * try to reformat it and write the results in the cat directory.  If
 * it is not possible to write the cat file, simply format and display
 * the man file.
 *
 * If preformatted pages are not supported, or the troff option is
 * being used, only look for the man page source file.
 *
 */
int
man (name)
     char *name;
{
  register int found;
  register int glob;
  register char **mp;
  register char **sp;
#ifdef __FreeBSD__
  int l_found;
  char buf[FILENAME_MAX];
#endif

  found = 0;

  fflush (stdout);
  if (shortsec != NULL)
    {
      for (mp = manpathlist; *mp != NULL; mp++)
	{
	  if (debug)
	    fprintf (stderr, "\nsearching in %s\n", *mp);

	  glob = 1;

#ifdef __FreeBSD__
	  l_found = 0;
	  if (locale != NULL) {
	    locale_opts = locale_nroff;
	    if (*locale_lang != '\0' && *locale_terr != '\0') {
	      snprintf(buf, sizeof(buf), "%s/%s_%s.%s", *mp,
		       locale_lang, locale_terr, locale_codeset);
	      if (is_directory (buf) == 1)
		l_found = try_section (buf, shortsec, longsec, name, glob);
	    }
	    if (!l_found) {
	      if (*locale_lang != '\0') {
		snprintf(buf, sizeof(buf), "%s/%s.%s", *mp,
			 locale_lang, locale_codeset);
		if (is_directory (buf) == 1)
		  l_found = try_section (buf, shortsec, longsec, name, glob);
	      }
	      if (!l_found && strcmp(locale_lang, "en") != 0) {
		snprintf(buf, sizeof(buf), "%s/en.%s", *mp,
			 locale_codeset);
		if (is_directory (buf) == 1)
		  l_found = try_section (buf, shortsec, longsec, name, glob);
	      }
	    }
	    locale_opts = NULL;
	  }
	  if (!l_found) {
#endif
	  found += try_section (*mp, shortsec, longsec, name, glob);
#ifdef __FreeBSD__
	  } else
	    found += l_found;
#endif

	  if (found && !findall)   /* i.e. only do this section... */
	    return found;
	}
    }
  else
    {
      for (sp = section_list; *sp != NULL; sp++)
	{
	  for (mp = manpathlist; *mp != NULL; mp++)
	    {
	      if (debug)
		fprintf (stderr, "\nsearching in %s\n", *mp);

	      glob = 1;

#ifdef __FreeBSD__
	      l_found = 0;
	      if (locale != NULL) {
		locale_opts = locale_nroff;
		if (*locale_lang != '\0' && *locale_terr != '\0') {
		  snprintf(buf, sizeof(buf), "%s/%s_%s.%s", *mp,
			   locale_lang, locale_terr, locale_codeset);
		  if (is_directory (buf) == 1)
		    l_found = try_section (buf, *sp, longsec, name, glob);
		}
		if (!l_found) {
		  if (*locale_lang != '\0') {
		    snprintf(buf, sizeof(buf), "%s/%s.%s", *mp,
			     locale_lang, locale_codeset);
		    if (is_directory (buf) == 1)
		      l_found = try_section (buf, *sp, longsec, name, glob);
		  }
		  if (!l_found && strcmp(locale_lang, "en") != 0) {
		    snprintf(buf, sizeof(buf), "%s/en.%s", *mp,
			     locale_codeset);
		    if (is_directory (buf) == 1)
		      l_found = try_section (buf, *sp, longsec, name, glob);
		  }
		}
		locale_opts = NULL;
	      }
	      if (!l_found) {
#endif
	      found += try_section (*mp, *sp, longsec, name, glob);
#ifdef __FreeBSD__
	      } else
		found += l_found;
#endif

	      if (found && !findall)   /* i.e. only do this section... */
		return found;
	    }
	}
    }
  return found;
}

char **
get_section_list ()
{
  int i;
  char *p;
  char *end;
#define TMP_SECTION_LIST_SIZE 100
  static char *tmp_section_list[TMP_SECTION_LIST_SIZE];

  if (colon_sep_section_list == NULL)
    {
      if ((p = getenv ("MANSECT")) == NULL)
	{
	  return std_sections;
	}
      else
	{
	  colon_sep_section_list = strdup (p);
	}
    }

  i = 0;
  for (p = colon_sep_section_list; i < TMP_SECTION_LIST_SIZE ; p = end+1) 
    {
      if ((end = strchr (p, ':')) != NULL)
	*end = '\0';

      tmp_section_list[i++] = strdup (p);

      if (end == NULL)
	break;
    }

  tmp_section_list [i] = NULL;
  return tmp_section_list;
}
