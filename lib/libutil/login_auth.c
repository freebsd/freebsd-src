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
 *	$Id: login_auth.c,v 1.1 1997/01/04 16:49:59 davidn Exp $
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <login_cap.h>
#include <stdarg.h>
#include <paths.h>
#include <sys/wait.h>

extern char *fgetline(FILE *, int*);

#ifdef RLIM_LONG
# define STRTOV strtol
#else
# define STRTOV strtoq
#endif

#define AUTHMAXLINES  1024
#define AUTHMAXARGS   16

struct auth_info {
  int reject;
  int auths;
  int env_count;
  char **env;
  int file_count;
  char **files;
};

static struct auth_info auth_info;

/*
 * free_auth_info()
 * Go through the auth_info structure, and free() anything of interest.
 * This includes the string arrays, and any individual element.
 * All part of being environmentally conscious ;).
 */

static void
free_auth_info(void)
{
  int i;
  char *ptr;

  auth_info.reject = 0;
  auth_info.auths = 0;
  if (auth_info.env) {
    for (i = 0; i < auth_info.env_count; i++) {
      if (auth_info.env[i])
	free(auth_info.env[i]);
    }
    free(auth_info.env);
    auth_info.env = NULL;
  }
  if (auth_info.files) {
    for (i = 0; i < auth_info.file_count; i++) {
      if (auth_info.files[i])
	free(auth_info.files[i]);
    }
    free(auth_info.files);
    auth_info.files = NULL;
  }
}


/*
 * collect_info()
 * Read from <fd>, a list of authorization commands.
 * These commands are:
 *	reject
 *	authorize [root|secure]
 *	setenv <name>[ <value>]
 *	remove <file>
 * A single reject means the entire thing is bad;
 * multiple authorize statements can be present (it would be
 * silly, but that's what the spec says).
 * The commands are collected, and are accted upon by:
 *	auth_scan()	-- check for authorization or rejection
 *	auth_rmfiles()	-- remove the specified files
 *	auth_env()	-- set the specified environment variables
 * We only get up to AUTHMAXLINES lines of input from the program.
 */
#define STRSIZEOF(x)  (sizeof(x)-1)
static void
collect_info(int fd)
{
  char *line;
  FILE *fp;
  char *ptr;
  int len;
  int line_count = 0;

  fp = fdopen(fd, "r");

  while ((line = fgetline(fp, &len)) != NULL) {
    if (++line_count > AUTHMAXLINES)
      break;
    if (strncasecmp(line, BI_REJECT, STRSIZEOF(BI_REJECT)) == 0) {
      auth_info.reject = 1;
    } else if (strncasecmp(line, BI_AUTH, STRSIZEOF(BI_AUTH)) == 0) {
      ptr = line + STRSIZEOF(BI_AUTH);
      ptr += strspn(ptr, " \t");
      if (!*ptr)
	auth_info.auths |= AUTH_OKAY;
      else if (strncasecmp(ptr, BI_ROOTOKAY, STRSIZEOF(BI_ROOTOKAY)) == 0)
	auth_info.auths |= AUTH_ROOTOKAY;
      else if (strncasecmp(ptr, BI_SECURE, STRSIZEOF(BI_SECURE)) == 0)
	auth_info.auths |= AUTH_SECURE;
    } else if (strncasecmp(line, BI_SETENV, STRSIZEOF(BI_SETENV)) == 0) {
      ptr = line + STRSIZEOF(BI_SETENV);
      ptr += strspn(ptr, " \t");
      if (*ptr) {
	char **tmp = realloc(auth_info.env, sizeof(char*) * (auth_info.env_count + 1));
	if (tmp != NULL) {
	  auth_info.env = tmp;
	  if ((auth_info.env[auth_info.env_count] = strdup(ptr)) != NULL)
	    auth_info.env_count++;
	}
      }
    } else if (strncasecmp(line, BI_REMOVE, STRSIZEOF(BI_REMOVE)) == 0) {
      ptr = line + STRSIZEOF(BI_REMOVE);
      ptr += strspn(ptr, " \t");
      if (*ptr) {
	char **tmp = realloc(auth_info.files, sizeof(char*) * (auth_info.file_count + 1));
	if (tmp != NULL) {
	  auth_info.files = tmp;
	  if ((auth_info.files[auth_info.file_count] = strdup(ptr)) != NULL)
	    auth_info.file_count++;
	}
      }
    }
  }
  fclose(fp);
}

      
/*
 * authenticate()
 * Starts an auth_script() for the given <user>, with a class <class>,
 * style <style>, and service <service>.  <style> is necessary,
 * as are <user> and <class>, but <service> is optional -- it defaults
 * to "login".
 * Since auth_script() expects an execl'able program name, authenticate()
 * also concatenates <style> to _PATH_AUTHPROG.
 * Lastly, calls auth_scan(AUTH_NONE) to see if there are any "reject" statements,
 * or lack of "auth" statements.
 * Returns -1 on error, 0 on rejection, and >0 on success.
 * (See AUTH_* for the return values.)
 *
 */
int
authenticate(const char * name, const char * class, const char * style, const char *service)
{
  int retval;

  if (style == NULL || *style == '\0')
    retval = -1;
  else {
    char buf[sizeof(_PATH_AUTHPROG) + 64];

    if (service == NULL || *service == '\0')
      service = LOGIN_DEFSERVICE;

    free_auth_info();

    if (snprintf(buf, sizeof buf, _PATH_AUTHPROG "%s", style) >= sizeof buf)
      retval = -1;
    else {
      retval = auth_script(buf, style, "-s", service, name, class, NULL);
      if (retval >= 0)
	retval = auth_scan(AUTH_NONE);
    }
  }
  return retval;
}


/*
 * auth_script()
 * Runs an authentication program with specified arguments.
 * It sets up file descriptor 3 for the program to write to;
 * it stashes the output somewhere.  The output of the program
 * consists of statements:
 *	reject
 *	authorize [root|secure]
 *	setenv <name> [<value>]
 *	remove <file>
 *
 * Terribly exciting, isn't it?  There is no limit specified in
 * BSDi's API for how much output can be present, but we should
 * keep it fairly small, I think.
 * No more than AUTHMAXLINES lines.
 */

int
auth_script(const char * path, ...)
{
  va_list ap;
  int pid, status;
  int argc = 0;
  int p[2];	/* pipes */
  char *argv[AUTHMAXARGS+1];

  va_start(ap, path);
  while (argc < AUTHMAXARGS && (argv[argc++] = va_arg(ap, char*)) != NULL)
    ;
  argv[argc] = NULL;
  va_end(ap);

  fflush(NULL);

  if (pipe(p) >= 0) {
    if ((pid = fork()) == -1) {
      close(p[0]);
      close(p[1]);
    } else if (pid == 0) {    /* Child */
      close(p[0]);
      dup2(p[1], 3);
      if (setenv("PATH", _PATH_DEFPATH, 1)==0 && setenv("SHELL", _PATH_BSHELL, 1)==0)
	execv(path, argv);
      _exit(1);
    } else {
      close(p[1]);
      collect_info(p[0]);
      if (waitpid(pid, &status, 0) != -1 && WIFEXITED(status) && !WEXITSTATUS(status))
	return 0;
    }
  }
  return -1;
}


/*
 * auth_env()
 * Processes the stored "setenv" lines from the stored authentication
 * output.
 */

int
auth_env(void)
{
  int i;

  for (i = 0; i < auth_info.env_count; i++) {
    char *nam = auth_info.env[i];
    char *ptr = nam + strcspn(nam, " \t=");
    if (*ptr) {
      *ptr++ = '\0';
      ptr += strspn(ptr, " \t");
    }
    setenv(nam, ptr, 1);
  }
}


/*
 * auth_scan()
 * Goes through the output of the auth_script/authenticate, and
 * checks for a failure or authentication.
 * <ok> is a default authentication value -- if there are no
 * rejection or authentication statements, then it is returned
 * unmodified.
 * AUTH_NONE is returned if there were any reject statements
 * from the authentication program (invoked by auth_script()), and
 * AUTH, AUTH_ROOTOKAY, and/or AUTH_SECURE are returned if the
 * appropriate directives were found.  Note that AUTH* are
 * *bitmasks*!
 */

int
auth_scan(int ok)
{
  if (auth_info.reject)
    return 0;
  return ok | auth_info.auths;
}


/*
 * auth_rmfiles()
 * Removes any files that the authentication program said needed to be
 * removed, said files having come from a previous execution of
 * auth_script().
 */

int
auth_rmfiles(void)
{
  int i = auth_info.file_count;
  while (i-- > 0) {
    unlink(auth_info.files[i]);
    free(auth_info.files[i]);
    auth_info.files[i] = NULL;
  }
}


/*
 * auth_checknologin()
 * Checks for the existance of a nologin file in the login_cap
 * capability <lc>.  If there isn't one specified, then it checks
 * to see if this class should just ignore nologin files.  Lastly,
 * it tries to print out the default nologin file, and, if such
 * exists, it exits.
 */

void
auth_checknologin(login_cap_t *lc)
{
  char *file;

  /* Do we ignore a nologin file? */
  if (login_getcapbool(lc, "ignorenologin", 0))
    return;

  /* Note that <file> will be "" if there is no nologin capability */
  if ((file = login_getcapstr(lc, "nologin", "", NULL)) == NULL)
    exit(1);

  /*
   * *file is true IFF there was a "nologin" capability
   * Note that auth_cat() returns 1 only if the specified
   * file exists, and is readable.  E.g., /.nologin exists.
   */
  if ((*file && auth_cat(file)) || auth_cat(_PATH_NOLOGIN))
    exit(1);
}


/*
 * auth_cat()
 * Checks for the readability of <file>; if it can be opened for
 * reading, it prints it out to stdout, and then exits.  Otherwise,
 * it returns 0 (meaning no nologin file).
 */
int
auth_cat(const char *file)
{
  int fd, count;
  char buf[BUFSIZ];

  if ((fd = open(file, O_RDONLY)) < 0)
    return 0;
  while ((count = read(fd, buf, sizeof(buf))) > 0)
    write(fileno(stdout), buf, count);
  close(fd);
  return 1;
}
