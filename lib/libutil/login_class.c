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
 * High-level routines relating to use of the user capabilities database
 *
 *	$Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <pwd.h>
#include <syslog.h>
#include <login_cap.h>
#include <paths.h>


#undef	UNKNOWN
#define	UNKNOWN	"su"


static struct login_res {
  const char * what;
  rlim_t (*who)(login_cap_t *, const char *, rlim_t, rlim_t);
  int why;
} resources[] = {
  { "cputime",	    login_getcaptime, RLIMIT_CPU      },
  { "filesize",     login_getcapsize, RLIMIT_FSIZE    },
  { "datasize",     login_getcapsize, RLIMIT_DATA     },
  { "stacksize",    login_getcapsize, RLIMIT_STACK    },
  { "coredumpsize", login_getcapsize, RLIMIT_CORE     },
  { "memoryuse",    login_getcapsize, RLIMIT_RSS      },
  { "memorylocked", login_getcapsize, RLIMIT_MEMLOCK  },
  { "maxproc",	    login_getcapnum,  RLIMIT_NPROC    },
  { "openfiles",    login_getcapnum,  RLIMIT_NOFILE   },
  { NULL,	    0,		      0 	      }
};



void
setclassresources(login_cap_t *lc)
{
  struct login_res *lr = resources;

  while (lr->what != NULL) {
    struct rlimit rlim,
		  newlim;
    char	  cur[40],
		  max[40];
    rlim_t	  rcur,
		  rmax;

    sprintf(cur, "%s-cur", lr->what);
    sprintf(max, "%s-max", lr->what);

    /*
     * The login.conf file can have <limit>, <limit>-max, and
     * <limit>-cur entries.
     * What we do is get the current current- and maximum- limits.
     * Then, we try to get an entry for <limit> from the capability,
     * using the current and max limits we just got as the
     * default/error values.
     * *Then*, we try looking for <limit>-cur and <limit>-max,
     * again using the appropriate values as the default/error
     * conditions.
     */

    getrlimit(lr->why, &rlim);
    rcur = rlim.rlim_cur;
    rmax = rlim.rlim_max;

    rcur = (*lr->who)(lc, lr->what, rcur, rcur);
    rmax = (*lr->who)(lc, lr->what, rmax, rmax);
    newlim.rlim_cur = (*lr->who)(lc, cur, rcur, rcur);
    newlim.rlim_max = (*lr->who)(lc, max, rmax, rmax);
    
    if (setrlimit(lr->why, &newlim) == -1)
      syslog(LOG_WARNING, "set class '%s' resource limit %s: %m", lc->lc_class, lr->what);

    ++lr;
  }
}

static struct login_vars {
  const char * tag;
  const char * var;
  const char * def;
} pathvars[] = {
  { "path",	"PATH",	      NULL    },
  { "manpath",	"MANPATH",    NULL    },
  { NULL,	NULL,	      NULL    }
}, envars[] = {
  { "lang",	"LANG",	      NULL    },
  { "charset",	"MM_CHARSET", NULL    },
  { "timezone", "TZ",	      NULL    },
  { "term",	"TERM",       UNKNOWN },
  { NULL,	NULL,	      NULL    }
};

static char *
substvar(char * var, const struct passwd * pwd, int hlen, int pch, int nlen)
{
  char * np = NULL;

  if (var != NULL) {
    int tildes = 0;
    int dollas = 0;
    char * p;

    if (pwd != NULL) {
      /*
       * Count the number of ~'s in var to substitute
       */
      p = var;
      while ((p = strchr(p, '~')) != NULL) {
	++tildes;
	++p;
      }

      /*
       * Count the number of $'s in var to substitute
       */
      p = var;
      while ((p = strchr(p, '$')) != NULL) {
        ++dollas;
        ++p;
      }
    }

    np = malloc(strlen(var) + (dollas * nlen) - dollas + (tildes * (pch+hlen)) - tildes + 1);

    if (np != NULL) {
      p = strcpy(np, var);

      if (pwd != NULL) {
	/*
	 * This loop does user username and homedir substitutions
	 * for unescaped $ (username) and ~ (homedir)
	 */
	while (*(p += strcspn(p, "~$")) != '\0') {
	  int l = strlen(p);

	  if (p > var && *(p-1) == '\\')  /* Escaped: */
	    memmove(p - 1, p, l + 1);	    /* Slide-out the backslash */
	  else if (*p == '~') {
	    memmove(p + 1, p + hlen + pch, l);  /* Subst homedir */
	    memmove(p, pwd->pw_dir, hlen);
	    if (pch)
      	      p[hlen] = '/';
	    p += hlen + pch;
	  }
	  else /* if (*p == '$') */ {
	    memmove(p + 1, p + nlen, l);	/* Subst username */
	    memmove(p, pwd->pw_name, nlen);
	    p += nlen;
	  }
	}
      }
    }
  }
  return np;
}


void
setclassenvironment(login_cap_t *lc, const struct passwd * pwd, int paths)
{
  struct login_vars * vars = paths ? pathvars : envars;
  int hlen = pwd ? strlen(pwd->pw_dir) : 0;
  int nlen = pwd ? strlen(pwd->pw_name) : 0;
  char pch = 0;

  if (hlen && pwd->pw_dir[hlen-1] != '/')
    ++pch;

  while (vars->tag != NULL) {
    char * var = paths ? login_getpath(lc, vars->tag, NULL)
		       : login_getcapstr(lc, vars->tag, NULL, NULL);

    char * np  = substvar(var, pwd, hlen, pch, nlen);

    if (np != NULL) {
      setenv(vars->var, np, 1);
      free(np);
    } else if (vars->def != NULL) {
      setenv(vars->var, vars->def, 0);
    }
    ++vars;
  }

  /*
   * If we're not processing paths, then see if there is a setenv list by
   * which the admin and/or user may set an arbitrary set of env vars.
   */
  if (!paths) {
    char ** set_env = login_getcaplist(lc, "setenv", ",");
    if (set_env != NULL) {
      while (*set_env != NULL) {
	char *p = strchr(*set_env, '=');
	if (p != NULL) {
	  char * np = substvar(set_env[1], pwd, hlen, pch, nlen);
	  if (np != NULL) {
	    setenv(set_env[0], np, 1);
	    free(np);
	  }
	}
	++set_env;
      }
    }
  }

}


/*
 * setclasscontext()
 *
 * For the login class <class>, set various class context values
 * (limits, mainly) to the values for that class.  Which values are
 * set are controlled by <flags> -- see <login_class.h> for the
 * possible values.
 *
 * setclasscontext() can only set resources, priority, and umask.
 */

int
setclasscontext(const char *classname, unsigned int flags)
{
  int rc;
  login_cap_t * lc = login_getclassbyname(classname, NULL);
  flags &= (LOGIN_SETRESOURCES| LOGIN_SETPRIORITY|LOGIN_SETUMASK);
  rc = setusercontext(lc, NULL, 0, flags);
  login_close(lc);
  return rc;
}


/*
 * setusercontext()
 *
 * Given a login class <lc> and a user in <pwd>, with a uid <uid>,
 * set the context as in setclasscontext().  <flags> controls which
 * values are set.
 *
 * The difference between setclasscontext() and setusercontext() is
 * that the former sets things up for an already-existing process,
 * while the latter sets things up from a root context.  Such as might
 * be called from login(1).
 *
 */

int
setusercontext(login_cap_t *lc, const struct passwd *pwd, uid_t uid, unsigned int flags)
{
  int i;
  login_cap_t * llc = NULL;

  if (lc == NULL)
  {
    if (pwd == NULL || (lc = login_getclass(pwd)) == NULL) {
      return -1;
    }
    llc = lc; /* free this when we're done */
  }

  if (flags & LOGIN_SETPATH)
    pathvars[0].def = uid ? _PATH_DEFPATH : _PATH_STDPATH;

  /*
   * Set the process priority
   */
  if (flags & LOGIN_SETPRIORITY) {
    int pri = (int)login_getcapnum(lc, "priority", LOGIN_DEFPRI, LOGIN_DEFPRI);
    pri = (pri < PRIO_MIN) ? PRIO_MIN : (pri > PRIO_MAX) ? PRIO_MAX : pri;
    if (setpriority(PRIO_PROCESS, 0, pri) != 0)
      syslog(LOG_WARNING, "setpriority '%s': %m", pwd->pw_name);
  }

  /*
   * Set resources
   */
  if (flags & LOGIN_SETRESOURCES)
    setclassresources(lc);

  /*
   * Set the sessions login
   */
  if ((flags & LOGIN_SETLOGIN) && setlogin(pwd->pw_name) != 0) {
    syslog(LOG_ERR, "setlogin '%s': %m", pwd->pw_name);
    login_close(llc);
    return -1;	/* You can't be too paranoid */
  }

  /*
   * Setup the user's group permissions
   */
  if (flags & LOGIN_SETGROUP) {
    /* XXX is it really a good idea to let errors here go? */
    if (setgid(pwd->pw_gid) != 0)
      syslog(LOG_WARNING, "setgid %ld: %m", (long)pwd->pw_gid);
    if (initgroups(pwd->pw_name, pwd->pw_gid) == -1)
      syslog(LOG_WARNING, "initgroups '%s': %m", pwd->pw_name);
  }

  /*
   * This needs to be done after all of the above.
   */
  if ((flags & LOGIN_SETUSER) && setuid(uid) != 0) {
    syslog(LOG_ERR, "setuid %ld: %m", uid);
    login_close(llc);
    return -1;	/* Paranoia again */
  }

  /*
   * FreeBSD extension: here we (might) loop and do this twice.
   * First, for the class we have been given, and next for
   * any user overrides in ~/.login.conf the user's home directory.
   */
  if (flags & LOGIN_SETUMASK)
    umask(LOGIN_DEFUMASK);    /* Set default umask up front */

  i = 0;
  while (i < 2 && lc != NULL) {

    if (flags & LOGIN_SETUMASK) {
      rlim_t tmp = login_getcapnum(lc, "umask", RLIM_INFINITY, RLIM_INFINITY);
      if (tmp != RLIM_INFINITY)
	umask((mode_t)tmp);
    }

    if (flags & LOGIN_SETPATH)
      setclassenvironment(lc, pwd, 1);

    if (flags & LOGIN_SETENV)
      setclassenvironment(lc, pwd, 0);

    if (i++ == 0) /* Play it again, Sam */
      lc = (pwd == NULL) ? NULL : login_getuserclass(pwd);
  }

  login_close(lc);  /* User's private 'me' class */
  login_close(llc); /* Class we opened */

  return 0;
}

