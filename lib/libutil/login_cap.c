/*-
 * Copyright (c) 1996 by
 * Sean Eric Fagan <sef@kithrup.com>
 * David Nugent <davidn@blaze.net.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. This work was done expressly for inclusion into FreeBSD.  Other use
 *    is permitted provided this notation is included.
 * 4. Absolutely no warranty of function or purpose is made by the authors.
 * 5. Modifications may be freely made to this file providing the above
 *    conditions are met.
 *
 * Low-level routines relating to the user capabilities database
 *
 *	$FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <pwd.h>
#include <login_cap.h>

#ifdef RLIM_LONG
# define STRTOV strtol
#else
# define STRTOV strtoq
#endif

static int lc_object_count = 0;

static size_t internal_stringsz = 0;
static char * internal_string = NULL;
static size_t internal_arraysz = 0;
static char ** internal_array = NULL;

static char *
allocstr(char * str)
{
  char * p;
  size_t sz = strlen(str) + 1;	/* realloc() only if necessary */
  if (sz <= internal_stringsz)
    p = strcpy(internal_string, str);
  else if ((p = realloc(internal_string, sz)) != NULL) {
    internal_stringsz = sz;
    internal_string = strcpy(p, str);
  }
  return p;
}

static char **
allocarray(size_t sz)
{
  char ** p;
  if (sz <= internal_arraysz)
    p = internal_array;
  else if ((p = realloc(internal_array, sz * sizeof(char*))) != NULL) {
    internal_arraysz = sz;
    internal_array = p;
  }
  return p;
}


/*
 * arrayize()
 * Turn a simple string <str> seperated by any of
 * the set of <chars> into an array.  The last element
 * of the array will be NULL, as is proper.
 * Free using freearraystr()
 */

static char **
arrayize(char *str, const char *chars, int *size)
{
  int i;
  char *ptr;
  char **res = NULL;

  for (i = 0, ptr = str; *ptr; i++) {
    int count = strcspn(ptr, chars);
    ptr += count;
    if (*ptr)
      ++ptr;
  }

  if ((ptr = allocstr(str)) == NULL) {
    res = NULL;
    i = 0;
  } else if ((res = allocarray(++i)) == NULL) {
    free(str);
    i = 0;
  } else {
    for (i = 0; *ptr; i++) {
      int count = strcspn(ptr, chars);
      res[i] = ptr;
      ptr += count;
      if (*ptr)
	*ptr++ = '\0';
    }
    res[i] = 0;
  }
  if (size)
    *size = i;
  return res;
}

static void
freearraystr(char ** array)
{
  /*
   * the array[0] should be free'd, and then array.
   */
  if (array) {
    free(array[0]);
    free(array);
  }
}


/*
 * login_close()
 * Frees up all resources relating to a login class
 *
 */

void
login_close(login_cap_t * lc)
{
  if (lc) {
    free(lc->lc_style);
    free(lc->lc_class);
    free(lc);
    if (--lc_object_count == 0) {
      free(internal_string);
      free(internal_array);
      internal_array = NULL;
      internal_string = NULL;
      cgetclose();
    }
  }
}


/*
 * login_getclassbyname() get the login class by its name.
 * If the name given is NULL or empty, the default class
 * LOGIN_DEFCLASS (ie. "default") is fetched. If the
 * 'dir' argument contains a non-NULL non-empty string,
 * then the file _FILE_LOGIN_CONF is picked up from that
 * directory instead of the system login database.
 * Return a filled-out login_cap_t structure, including
 * class name, and the capability record buffer.
 */

login_cap_t *
login_getclassbyname(char const * name, char const * dir)
{
  login_cap_t *lc = malloc(sizeof(login_cap_t));
  
  if (lc != NULL) {
    int   i = 0;
    char  userpath[MAXPATHLEN];
    static char *login_dbarray[] = { NULL, NULL, NULL };

    if (dir && snprintf(userpath, MAXPATHLEN, "%s/%s", dir, _FILE_LOGIN_CONF) < MAXPATHLEN)
      login_dbarray[i++] = userpath;
    else
      login_dbarray[i++]   = _PATH_LOGIN_CONF;
    login_dbarray[i  ]   = NULL;

    lc->lc_cap = lc->lc_class = lc->lc_style = NULL;

    if ((name == NULL || cgetent(&lc->lc_cap, login_dbarray, (char*)name) != 0) &&
	cgetent(&lc->lc_cap, login_dbarray, (char*)(name = LOGIN_DEFCLASS)) != 0) {
	free(lc);
	lc = NULL;
    } else {
      ++lc_object_count;
      lc->lc_class = strdup(name);
    }
  }

  return lc;
}



/*
 * login_getclass()
 * Get the login class for a given password entry from
 * the system (only) login class database.
 * If the password entry's class field is not set, or
 * the class specified does not exist, then use the
 * default of LOGIN_DEFCLASS (ie. "default").
 * Return a filled-out login_cap_t structure, including
 * class name, and the capability record buffer.
 */

login_cap_t *
login_getclass(const struct passwd *pwd)
{
  const char * class = NULL;
  if (pwd != NULL) {
    if ((class = pwd->pw_class) == NULL || *class == '\0')
      class = (pwd->pw_uid == 0) ? "root" : NULL;
  }
  return login_getclassbyname(class, 0);
}


/*
 * login_getuserclass()
 * Get the login class for a given password entry, allowing user
 * overrides via ~/.login_conf.
 * ### WAS: If the password entry's class field is not set,
 * #######  or the class specified does not exist, then use
 * If an entry with the recordid "me" does not exist, then use
 * the default of LOGIN_DEFCLASS (ie. "default").
 * Return a filled-out login_cap_t structure, including
 * class name, and the capability record buffer.
 */

login_cap_t *
login_getuserclass(const struct passwd *pwd)
{
  const char * class = "me"; /* (pwd == NULL) ? NULL : pwd->pw_class; */
  const char * home  = (pwd == NULL) ? NULL : pwd->pw_dir;
  return login_getclassbyname(class, home);
}



/*
 * login_getcapstr()
 * Given a login_cap entry, and a capability name, return the
 * value defined for that capability, a defualt if not found, or
 * an error string on error.
 */

char *
login_getcapstr(login_cap_t *lc, const char *cap, char *def, char *error)
{
  char *res;
  int ret;

  if (lc == NULL || cap == NULL || lc->lc_cap == NULL || *cap == '\0')
    return def;

  if ((ret = cgetstr(lc->lc_cap, (char *)cap, &res)) == -1) {
    return def;
  } else if (ret >= 0)
    return res;
  else
    return error;
}


/*
 * login_getcaplist()
 * Given a login_cap entry, and a capability name, return the
 * value defined for that capability split into an array of
 * strings.
 */

char **
login_getcaplist(login_cap_t *lc, const char * cap, const char * chars)
{
  char * lstring;

  if (chars == NULL)
    chars = ". \t";
  if ((lstring = login_getcapstr(lc, (char*)cap, NULL, NULL)) != NULL)
    return arrayize(lstring, chars, NULL);
  return NULL;
}


/*
 * login_getpath()
 * From the login_cap_t <lc>, get the capability <cap> which is
 * formatted as either a space or comma delimited list of paths
 * and append them all into a string and separate by semicolons.
 * If there is an error of any kind, return <error>.
 */

char *
login_getpath(login_cap_t *lc, const char *cap, char * error)
{
  char *str = login_getcapstr(lc, (char*)cap, NULL, NULL);

  if (str == NULL)
    str = error;
  else {
    char *ptr = str;

    while (*ptr) {
      int count = strcspn(ptr, ", \t");
      ptr += count;
      if (*ptr)
	*ptr++ = ':';
    }
  }
  return str;
}


/*
 * login_getcaptime()
 * From the login_cap_t <lc>, get the capability <cap>, which is
 * formatted as a time (e.g., "<cap>=10h3m2s").  If <cap> is not
 * present in <lc>, return <def>; if there is an error of some kind,
 * return <error>.
 */

rlim_t
login_getcaptime(login_cap_t *lc, const char *cap, rlim_t def, rlim_t error)
{
  char *res, *ep;
  int ret;
  rlim_t tot = 0, tim;

  errno = 0;
  if (lc == NULL || lc->lc_cap == NULL)
    return def;

  /*
   * Look for <cap> in lc_cap.
   * If it's not there (-1), return <def>.
   * If there's an error, return <error>.
   */

  if ((ret = cgetstr(lc->lc_cap, (char *)cap, &res)) == -1)
    return def;
  else if (ret < 0)
    return error;

  /*
   * "inf" and "infinity" are two special cases for this.
   */
  if (!strcasecmp(res, "infinity") || !strcasecmp(res, "inf"))
    return RLIM_INFINITY;

  /*
   * Now go through the string, turning something like 1h2m3s into
   * an integral value.  Whee.
   */

  errno = 0;
  while (*res) {
    tim = STRTOV(res, &ep, 0);
    if ((ep == NULL) || (ep == res) || errno) {
      return error;
    }
    /* Look for suffixes */
    switch (*ep++) {
    case 0:
      ep--; break;	/* end of string */
    case 's': case 'S':	/* seconds */
      break;
    case 'm': case 'M':	/* minutes */
      tim *= 60L;
      break;
    case 'h': case 'H':	/* hours */
      tim *= (60L * 60L);
      break;
    case 'd': case 'D':	/* days */
      tim *= (60L * 60L * 24L);
      break;
    case 'w': case 'W':	/* weeks */
      tim *= (60L * 60L * 24L * 7L);
    case 'y': case 'Y':	/* Years */
      /* I refuse to take leap years into account here.  Sue me. */
      tim *= (60L * 60L * 24L * 365L);
    default:
      return error;
    }
    res = ep;
    tot += tim;
  }
  return tot;
}


/*
 * login_getcapnum()
 * From the login_cap_t <lc>, extract the numerical value <cap>.
 * If it is not present, return <def> for a default, and return
 * <error> if there is an error.
 * Like login_getcaptime(), only it only converts to a number, not
 * to a time; "infinity" and "inf" are 'special.'
 */

rlim_t
login_getcapnum(login_cap_t *lc, const char *cap, rlim_t def, rlim_t error)
{
  char *ep, *res;
  int ret;
  rlim_t val;

  if (lc == NULL || lc->lc_cap == NULL)
    return def;

  /*
   * For BSDI compatibility, try for the tag=<val> first
   */
  if ((ret = cgetstr(lc->lc_cap, (char *)cap, &res)) == -1) {
    long lval;
    /*
     * String capability not present, so try for tag#<val> as numeric
     */
    if ((ret = cgetnum(lc->lc_cap, (char *)cap, &lval)) == -1)
      return def; /* Not there, so return default */
    else if (ret < 0)
      return error;
    return (rlim_t)lval;
  }
  else if (ret < 0)
    return error;

  if (!strcasecmp(res, "infinity") || !strcasecmp(res, "inf"))
    return RLIM_INFINITY;

  errno = 0;
  val = STRTOV(res, &ep, 0);
  if ((ep == NULL) || (ep == res) || errno)
    return error;
  return val;
}


/*
 * login_getcapsize()
 * From the login_cap_t <lc>, extract the capability <cap>, which is
 * formatted as a size (e.g., "<cap>=10M"); it can also be "infinity".
 * If not present, return <def>, or <error> if there is an error of
 * some sort.
 */

rlim_t
login_getcapsize(login_cap_t *lc, const char *cap, rlim_t def, rlim_t error) {
  char *ep, *res;
  int ret;
  rlim_t val;
  rlim_t mult;

  if (lc == NULL || lc->lc_cap == NULL)
    return def;

  if ((ret = cgetstr(lc->lc_cap, (char *)cap, &res)) == -1)
    return def;
  else if (ret < 0)
    return error;

  errno = 0;
  val = STRTOV(res, &ep, 0);
  if ((res == NULL) || (res == ep) || errno)
    return error;
  switch (*ep) {
  case 0:	/* end of string */
    mult = 1; break;
  case 'b': case 'B':	/* 512-byte blocks */
    mult = 512; break;
  case 'k': case 'K':	/* 1024-byte Kilobytes */
    mult = 1024; break;
  case 'm': case 'M':	/* 1024-k kbytes */
    mult = 1024 * 1024; break;
  case 'g': case 'G':	/* 1Gbyte */
    mult = 1024 * 1024 * 1024; break;
#ifndef RLIM_LONG
  case 't': case 'T':	/* 1TBte */
    mult = 1024LL * 1024LL * 1024LL * 1024LL; break;
#endif
  default:
    return error;
  }
  return val * mult;
}


/*
 * login_getcapbool()
 * From the login_cap_t <lc>, check for the existance of the capability
 * of <cap>.  Return <def> if <lc>->lc_cap is NULL, otherwise return
 * the whether or not <cap> exists there.
 */

int
login_getcapbool(login_cap_t *lc, const char *cap, int def)
{
  if (lc == NULL || lc->lc_cap == NULL)
    return def;
  return (cgetcap(lc->lc_cap, (char *)cap, ':') != NULL);
}


/*
 * login_getstyle()
 * Given a login_cap entry <lc>, and optionally a type of auth <auth>,
 * and optionally a style <style>, find the style that best suits these
 * rules:
 *	1.  If <auth> is non-null, look for an "auth-<auth>=" string
 *	in the capability; if not present, default to "auth=".
 *	2.  If there is no auth list found from (1), default to
 *	"passwd" as an authorization list.
 *	3.  If <style> is non-null, look for <style> in the list of
 *	authorization methods found from (2); if <style> is NULL, default
 *	to LOGIN_DEFSTYLE ("passwd").
 *	4.  If the chosen style is found in the chosen list of authorization
 *	methods, return that; otherwise, return NULL.
 * E.g.:
 *     login_getstyle(lc, NULL, "ftp");
 *     login_getstyle(lc, "login", NULL);
 *     login_getstyle(lc, "skey", "network");
 */

char *
login_getstyle(login_cap_t *lc, char *style, const char *auth)
{
  int  i;
  char **authtypes = NULL;
  char *auths= NULL;
  char realauth[64];

  static char *defauthtypes[] = { LOGIN_DEFSTYLE, NULL };

  if (auth != NULL && *auth != '\0' &&
      snprintf(realauth, sizeof realauth, "auth-%s", auth) < sizeof realauth)
    authtypes = login_getcaplist(lc, realauth, NULL);

  if (authtypes == NULL)
    authtypes = login_getcaplist(lc, "auth", NULL);

  if (authtypes == NULL)
    authtypes = defauthtypes;

  /*
   * We have at least one authtype now; auths is a comma-seperated
   * (or space-separated) list of authentication types.  We have to
   * convert from this to an array of char*'s; authtypes then gets this.
   */
  i = 0;
  if (style != NULL && *style != '\0') {
    while (authtypes[i] != NULL && strcmp(style, authtypes[i]) != 0)
      i++;
  }
  lc->lc_style = NULL;
  if (authtypes[i] != NULL && (auths = strdup(authtypes[i])) != NULL)
    lc->lc_style = auths;

  return lc->lc_style;
}


