/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RCSID("$Id: pagsh.c,v 1.6 2002/08/23 17:54:20 assar Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <time.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#ifdef KRB5
#include <krb5.h>
#endif
#ifdef KRB4
#include <krb.h>
#endif
#include <kafs.h>

#include <err.h>
#include <roken.h>
#include <getarg.h>

static int help_flag;
static int version_flag;
static int c_flag;

struct getargs getargs[] = {
    { NULL,	'c', arg_flag, &c_flag },
    { "version", 0,  arg_flag, &version_flag },
    { "help",	'h', arg_flag, &help_flag },
};

static int num_args = sizeof(getargs) / sizeof(getargs[0]);

static void
usage(int ecode)
{
    arg_printusage(getargs, num_args, NULL, "command [args...]");
    exit(ecode);
}

/*
 * Run command with a new ticket file / credentials cache / token
 */

int
main(int argc, char **argv)
{
  int f;
  char tf[1024];
  char *p;

  char *path;
  char **args;
  int i;
  int optind = 0;

  set_progname(argv[0]);
  if(getarg(getargs, num_args, argc, argv, &optind))
      usage(1);
  if(help_flag)
      usage(0);
  if(version_flag) {
      print_version(NULL);
      exit(0);
  }

  argc -= optind;
  argv += optind;

#ifdef KRB5
  snprintf (tf, sizeof(tf), "%sXXXXXX", KRB5_DEFAULT_CCROOT);
  f = mkstemp (tf + 5);
  close (f);
  unlink (tf + 5);
  esetenv("KRB5CCNAME", tf, 1);
#endif

#ifdef KRB4
  snprintf (tf, sizeof(tf), "%s_XXXXXX", TKT_ROOT);
  f = mkstemp (tf);
  close (f);
  unlink (tf);
  esetenv("KRBTKFILE", tf, 1);
#endif

  i = 0;

  args = (char **) malloc((argc + 10)*sizeof(char *));
  if (args == NULL)
      errx (1, "Out of memory allocating %lu bytes",
	    (unsigned long)((argc + 10)*sizeof(char *)));
  
  if(*argv == NULL) {
    path = getenv("SHELL");
    if(path == NULL){
      struct passwd *pw = k_getpwuid(geteuid());
      path = strdup(pw->pw_shell);
    }
  } else {
    path = strdup(*argv++);
  }
  if (path == NULL)
      errx (1, "Out of memory copying path");
  
  p=strrchr(path, '/');
  if(p)
    args[i] = strdup(p+1);
  else
    args[i] = strdup(path);

  if (args[i++] == NULL)
      errx (1, "Out of memory copying arguments");
  
  while(*argv)
    args[i++] = *argv++;

  args[i++] = NULL;

  if(k_hasafs())
    k_setpag();

  unsetenv("PAGPID");
  execvp(path, args);
  if (errno == ENOENT) {
      char **sh_args = malloc ((i + 2) * sizeof(char *));
      int j;

      if (sh_args == NULL)
	  errx (1, "Out of memory copying sh arguments");
      for (j = 1; j < i; ++j)
	  sh_args[j + 2] = args[j];
      sh_args[0] = "sh";
      sh_args[1] = "-c";
      sh_args[2] = path;
      execv ("/bin/sh", sh_args);
  }
  err (1, "execvp");
}
