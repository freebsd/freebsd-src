/* entgen.c -

   Implement entgen() which generates a list of filenames from a struct fpi.

   Written by James Clark (jjc@jclark.com).
*/

#include "config.h"

#ifdef HAVE_ACCESS

#ifdef HAVE_UNISTD_H
#include <unistd.h>		/* For R_OK. */
#endif /* HAVE_UNISTD_H */

#ifndef R_OK
#define R_OK 4
#endif /* not R_OK */

#endif /* HAVE_ACCESS */

#include "sgmlaux.h"

/* Environment variable that contains path. */
#ifndef PATH_ENV_VAR
#define PATH_ENV_VAR "SGML_PATH"
#endif
/* Default search path.  See field() for interpretation of %*. */
#ifndef DEFAULT_PATH
#define DEFAULT_PATH "/usr/local/lib/sgml/%O/%C/%T:%N.%X:%N.%D"
#endif

#ifndef PATH_FILE_SEP
#define PATH_FILE_SEP ':'
#endif

#ifndef SYSID_FILE_SEP
#define SYSID_FILE_SEP ':'
#endif

/* This says: change space to underscore, slash to percent. */

#ifndef MIN_DAT_SUBS_FROM
#define MIN_DAT_SUBS_FROM " /"
#endif
#ifndef MIN_DAT_SUBS_TO
#define MIN_DAT_SUBS_TO "_%"
#endif

static int field P((struct fpi *, int, char *));
static int mindatcpy P((char *, char *, int, int));
static int testopen P((char *));
static UNIV sysidgen P((char *));

static char *path = 0;

/* Non-zero if searching should be performed when a system identifier
is specified. */
static int sysidsrch = 0;

#define EMPTY_VERSION "default"

static char *classes[] = {
     "capacity",
     "charset",
     "notation",
     "syntax",
     "document",
     "dtd",
     "elements",
     "entities",
     "lpd",
     "nonsgml",
     "shortref",
     "subdoc",
     "text"
     };

/* This is mainly for compatibility with arcsgml. */

static char *genext[] = {
     "nsd",  /* Non-SGML data entity. */
     "gml",  /* GML document or text entity. */
     "spe",  /* System parameter entity. */
     "dtd",  /* Document type definition. */
     "lpd",  /* Link process definition. */
     "pns",  /* Public non-SGML data entity. */
     "pge",  /* Public general entity. */
     "ppe",  /* Public parameter entity. */
     "pdt",  /* Public document type definition. */
     "plp",  /* Public link process definition. */
     "vns",  /* Display version non-SGML data entity. */
     "vge",  /* Display version general entity. */
     "vpe",  /* Display version parameter entity. */
     "vdt",  /* Display version document type definition.*/
     "vlp",  /* Display version link process definition.*/
};

static char *ext[] = {
     "sgml",			/* SGML subdocument */
     "data",			/* Data */
     "text",			/* General text */
     "parm",			/* Parameter entity */
     "dtd",			/* Document type definition */
     "lpd",			/* Link process definition */
};

/* Like memcpy, but substitute, fold to lower case (if fold is
non-zero) and null terminate.  This is used both for minimum data and
for names. If p is NULL, do nothing. Return len. */

static int mindatcpy(p, q, len, fold)
char *p, *q;
int len;
int fold;
{
     static char subsfrom[] = MIN_DAT_SUBS_FROM;
     static char substo[] = MIN_DAT_SUBS_TO;
     int n;

     if (!p)
	  return len;
     for (n = len; --n >= 0; q++) {
	  char *r = strchr(subsfrom, *q);
	  if (!r) {
	       if (fold && ISASCII(*q) && isupper((UNCH)*q))
		    *p++ = tolower((UNCH)*q);
	       else
		    *p++ = *q;
	  }
	  else {
	       int i = r - subsfrom;
	       if (i < sizeof(substo) - 1)
		    *p++ = substo[i];
	  }
     }
     *p = '\0';
     return len;
}


/* Return length of field.  Copy into buf if non-NULL. */

static int field(f, c, buf)
struct fpi *f;
int c;
char *buf;
{
     int n;

     switch (c) {
     case '%':
	  if (buf) {
	       buf[0] = '%';
	       buf[1] = '\0';
	  }
	  return 1;
     case 'N':			/* the entity, document or dcn name */
	  return mindatcpy(buf, (char *)f->fpinm, ustrlen(f->fpinm),
		    (f->fpistore != 1 && f->fpistore != 2 && f->fpistore != 3
		     ? NAMECASE
		     : ENTCASE));
     case 'D':			/* dcn name */
	  if (f->fpistore != 1) /* not a external data entity */
	       return -1;
	  if (f->fpinedcn == 0) /* it's a SUBDOC */
	       return -1;
	  return mindatcpy(buf, (char *)f->fpinedcn, ustrlen(f->fpinedcn),
   	                   NAMECASE);
     case 'X':
	  /* This is for compatibility with arcsgml */
	  if (f->fpistore < 1 || f->fpistore > 5)
	       return -1;
	  n = (f->fpipubis != 0)*(f->fpiversw > 0 ? 2 : 1)*5+f->fpistore - 1;
	  if (buf)
	       strcpy(buf, genext[n]);
	  return strlen(genext[n]);
     case 'Y':			/* tYpe */
	  n = f->fpistore;
	  if (n < 1 || n > 5)
	       return -1;
	  if (n == 1 && f->fpinedcn == 0) /* it's a SUBDOC */
	       n = 0;
	  if (buf)
	       strcpy(buf, ext[n]);
	  return strlen(ext[n]);
     case 'P':			/* public identifier */
	  if (!f->fpipubis)
	       return -1;
	  return mindatcpy(buf, (char *)f->fpipubis, ustrlen(f->fpipubis), 0);
     case 'S':			/* system identifier */
	  if (!f->fpisysis)
	       return -1;
	  else {
	       UNCH *p;
	       n = 0;
	       for (p = f->fpisysis; *p; p++)
		    if (*p != RSCHAR) {
			 if (buf)
			      buf[n] = *p == RECHAR ? '\n' : *p;
			 n++;
		    }
	       return n;
	  }
     }
     /* Other fields need a formal public identifier. */
     /* return -1 if the formal public identifier was invalid or missing. */
     if (f->fpiversw < 0 || !f->fpipubis)
	  return -1;

     switch (c) {
     case 'A':			/* Is it available? */
	  return f->fpitt == '+' ? 0 : -1;
     case 'I':			/* Is it ISO? */
	  return f->fpiot == '!' ? 0 : -1;
     case 'R':			/* Is it registered? */
	  return f->fpiot == '+' ? 0 : -1;
     case 'U':			/* Is it unregistered? */
	  return f->fpiot == '-' ? 0 : -1;
     case 'L':			/* public text language */
	  if (f->fpic == FPICHARS)
	       return -1;
	  /* it's entered in all upper case letters */
	  return mindatcpy(buf, (char *)f->fpipubis + f->fpil, f->fpill, 1);
     case 'O':			/* owner identifier */
	  return mindatcpy(buf, (char *)f->fpipubis + f->fpio, f->fpiol, 0);
     case 'C':			/* public text class */
	  n = f->fpic - 1;
	  if (n < 0 || n >= sizeof(classes)/sizeof(classes[0]))
	       return -1;
	  if (buf)
	       strcpy(buf, classes[n]);
	  return strlen(classes[n]);
     case 'T':			/* text description */
	  return mindatcpy(buf, (char *)f->fpipubis + f->fpit, f->fpitl, 0);
     case 'V':
	  if (f->fpic < FPICMINV)	/* class doesn't have version */
	       return -1;
	  if (f->fpiversw > 0)         	/* no version */
	       return -1;
	  if (f->fpivl == 0) {		/* empty version: */
				        /* use device-independent version*/
	       if (buf)
		    strcpy(buf, EMPTY_VERSION);
	       return strlen(EMPTY_VERSION);
	  }
	  return mindatcpy(buf, (char *)f->fpipubis + f->fpiv, f->fpivl, 0);
     case 'E':	              /* public text designating (escape) sequence */
	  if (f->fpic != FPICHARS)
	       return -1;
	  return mindatcpy(buf, (char *)f->fpipubis + f->fpil, f->fpill, 0);
     default:
	  break;
     }
     return -1;
}

static int testopen(pathname)
char *pathname;
{
#ifdef HAVE_ACCESS
     return access(pathname, R_OK) >= 0;
#else /* not HAVE_ACCESS */
     FILE *fp;
     fp = fopen(pathname, "r");
     if (!fp)
	  return 0;
     fclose(fp);
     return 1;
#endif /* not HAVE_ACCESS */
}

/* Return a pointer to an dynamically-allocated buffer that contains
   the names of the files containing this entity, with each filename
   terminated by a '\0', and with the list of filenames terminated by
   another '\0'. */

UNIV entgen(f)
struct fpi *f;
{
     char *file;

     assert(f->fpistore != 6);	/* Musn't call entgen for a notation. */
     if (!path) {
	  char *p;
	  char c;
	  path = getenv(PATH_ENV_VAR);
	  if (!path)
	       path = DEFAULT_PATH;
	  p = path;

	  /* Only search for system identifiers if path uses %S. */
	  while ((c = *p++) != '\0')
	       if (c == '%') {
		    if (*p == 'S') {
			 sysidsrch = 1;
			 break;
		    }
		    if (*p != '\0' && *p != PATH_FILE_SEP)
			 p++;
	       }
     }
     if (f->fpisysis
	 && (!sysidsrch
	     || strchr((char *)f->fpisysis, SYSID_FILE_SEP)
	     || strcmp((char *)f->fpisysis, STDINNAME) == 0))
	  return sysidgen((char *)f->fpisysis);

     file = path;

     for (;;) {
	  char *p;
	  int len = 0;
	  char *fileend = strchr(file, PATH_FILE_SEP);
	  if (!fileend)
	       fileend = strchr(file, '\0');

	  /* Check that all substitutions are non-null, and calculate
	     the resulting total length of the filename. */
	  for (p = file; p < fileend; p++)
	       if (*p == '%') {
		    int n;
		    /* Set len to -1 if a substitution is invalid. */
		    if (++p >= fileend) {
			 len = -1;
			 break;
		    }
		    n = field(f, *p, (char *)0);
		    if (n < 0) {
			 len = -1;
			 break;
		    }
		    len += n;
	       }
	       else
		    len++;

	  if (len > 0) {
	       /* We've got a valid non-empty filename. */
	       char *s;
	       char *buf;

	       s = buf = (char *)rmalloc(len + 2);
	       for (p = file; p < fileend; p++)
		    if (*p == '%')
			 s += field(f, *++p, s);
		    else
			 *s++ = *p;
	       *s++ = '\0';
	       if (testopen(buf)) {
		    /* Terminate the array of filenames. */
		    *s++ = '\0';
		    return buf;
	       }
	       free((UNIV)buf);
	  }
	  if (*fileend == '\0')
	       break;
	  file = ++fileend;
     }
     return 0;
}

/* Handle a system identifier without searching. */

static
UNIV sysidgen(s)
char *s;
{
     char *buf, *p;

     buf = (char *)rmalloc(strlen(s) + 2);

     for (p = buf; *s; s++) {
	  if (*s == SYSID_FILE_SEP) {
	       if (p > buf && p[-1] != '\0')
		    *p++ = '\0';
	  }
	  else if (*s == RECHAR)
	       *p++ = '\n';
	  else if (*s != RSCHAR)
	       *p++ = *s;
     }
     /* Terminate this filename. */
     if (p > buf && p[-1] != '\0')
	  *p++ = '\0';
     if (p == buf) {
	  /* No filenames. */
	  frem((UNIV)buf);
	  return 0;
     }
     /* Terminate the list. */
     *p++ = '\0';
     return buf;
}

/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
End:
*/
