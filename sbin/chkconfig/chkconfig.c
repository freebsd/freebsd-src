/*
 * Copyright 1993, Garrett A. Wollman.
 * Copyright 1993, University of Vermont and State Agricultural College.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

const char chkconfig_c_rcsid[] =
  "$Id: chkconfig.c,v 1.5 1994/04/17 09:22:15 alm Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include "paths.h"

static int testvalue(const char *);
static int printflags(const char *);
static int setvalue(const char *, int);
static int printvalues(void);
static void usage(void);
static void die(const char *);
static int is_on(const char *, size_t);

const char *whoami;
static const char *configdir = _PATH_CONFIG;
static int sortbystate = 0;
static int doflags = 0;
static int force = 0;
static int verbose = 0;

int main(int argc, char **argv) {
  int opt;

  whoami = argv[0];

  while((opt = getopt(argc, argv, "osfd:v")) != EOF) {
    switch(opt) {
    case 'o':
      doflags = 1;
      break;
    case 's':
      sortbystate = 1;
      break;
    case 'f':
      force = 1;
      break;
    case 'd':
      configdir = optarg;
      break;
    case 'v':
      verbose = 1;
      break;
    case '?':
    default:
      usage();
      return -1;
    }
  }

  if(sortbystate && optind != argc) {
    fprintf(stderr, "%s: -s: too many arguments\n", whoami);
    usage();
    return -1;
  }

  if(force && optind == argc) {
    fprintf(stderr, "%s: -f: too few arguments\n", whoami);
    usage();
    return -1;
  }

  switch(argc - optind) {
  case 0:
    return printvalues();

  case 1:
    return doflags ? printflags(argv[optind]) : testvalue(argv[optind]);

  case 2:
    return setvalue(argv[optind], is_on(argv[optind + 1],
	strlen(argv[optind + 1])));
  
  default:
    usage();
    return -1;
  }
}

static int is_on(const char *str, size_t len) {
  if(!str || len < 2) return 0;
  return (   ((str[0] == 'o') || (str[0] == 'O'))
	  && ((str[1] == 'n') || (str[1] == 'N'))
	  && ((len == 2) || (str[2] == '\n')));
}

static void chat(const char *str, int state) {
  if(verbose)
    printf("`%s' is %s.\n", str, state ? "ON" : "OFF");
}

static const char *confname(const char *str, const char *str2) {
  int len = strlen(configdir) + strlen(str) + strlen(str2) + 2;
  char *rv = malloc(len);

  if(!rv) {
    errno = ENOMEM;
    die("confname: malloc");
  }

  strcpy(rv, configdir);
  strcat(rv, "/");
  strcat(rv, str);
  strcat(rv, str2);
  return rv;
}

static int testvalue(const char *str) {
  FILE *fp;
  char *line;
  const char *fname;
  size_t len = 0;
  int rv = 1;			/* NB: shell's convention is opposite C's */

  fname = confname(str, "");
  fp = fopen(fname, "r");
  if(fp) {
    do {
      line = fgetln(fp, &len);
    } while(line && line[0] == '#');
    rv = !is_on(line, len);	/* shell's convention is opposite C's */
    fclose(fp);
  }

  free((void *)fname);		/* cast away `const' to avoid warning */
  chat(str, !rv);		/* tell the user about it if verbose */
  return rv;
}

static char *getflags(const char *str) {
  FILE *fp;
  char *line;
  const char *fname;
  char *rv = strdup("");
  size_t len = 0;

  if(!rv) {
    errno = ENOMEM;
    die("getflags: strdup");
  }

  fname = confname(str, ".flags");
  fp = fopen(fname, "r");
  if(fp) {
    do {
      line = fgetln(fp, &len);
    } while(line && line[0] == '#');

    if(line) {
      free(rv);
      if(line[len - 1] == '\n') --len;
      if((rv = (char *) malloc(len + 1)) == NULL) {
	errno = ENOMEM;
	die("getflags: malloc");
      }
      bcopy(line, rv, len);
      rv[len] = '\0';
    }

    fclose(fp);
  }
  free((void *)fname);

  return rv;
}


static int printflags(const char *str) {
  int rv = 0;
  char *flags;

  flags = getflags(str);
  if(flags[0]) printf("%s\n", flags);
  free(flags);
  return 0;
}

static int setvalue(const char *str, int state) {
  FILE *fp;
  const char *fname;

  fname = confname(str, "");
  errno = 0;
  fp = fopen(fname, "r");
  if(!fp && !force) {
    /*
     * Yes, I know this is bogus, but SGI must have had some sort of
     * reason...
     */
    if(errno == ENOENT) {
      fprintf(stderr, 
	      "%s: configuration file does not exist;"
	      " use `-f' flag to create it.\n", whoami);
      return -1;
    } else {
      die(fname);
    }
  }

  if(fp)
    fclose(fp);

  errno = 0;
  fp = fopen(fname, "w");
  if(!fp)
    die(fname);

  fprintf(fp, "%s\n", state ? "on" : "off");

  fclose(fp);

  chat(str, state);
  return 0;
}

/*
 * From here down is the code for listing the value of every option.
 */
struct q {
  struct q *next;
  int state;
  char *name;
  char *flags;
};

static struct q *onhead;
static struct q *offhead;	/* only used for sortbystate == 1 */

static void insert(const char *fname) {
  struct q *q;
  struct q **headp;
  int state;

  q = malloc(sizeof *q);
  if(!q) {
    errno = ENOMEM;
    die("insert: malloc");
  }

  q->name = strdup(fname);
  q->state = state = !testvalue(fname);
  q->flags = getflags(fname);

  if(state || !sortbystate)
    headp = &onhead;
  else
    headp = &offhead;

  while(*headp && strcmp(q->name, (**headp).name) > 0) {
    headp = &(**headp).next;
  }

  q->next = *headp;
  *headp = q;
}

static int printvalues(void) {
  struct q *temp;
  DIR *dir;
  struct dirent *entry;
  struct stat stab;
  int doneheader = 0;

  verbose = 0;			/* we're already verbose enough */

  errno = 0;
  dir = opendir(configdir);
  if(!dir)
    die(configdir);

  while((entry = readdir(dir))) {
    if(stat(entry->d_name, &stab) < 0) {
      die(entry->d_name);
    }

    if(S_ISREG(stab.st_mode) && !strchr(entry->d_name, '.')) {
      insert(entry->d_name);
    }
  }

  closedir(dir);

  /*
   * Now we're done reading the file names, so we can print them out.
   * Thanks to insert(), everything is already in ASCII order.
   */
#define FORMAT "%15s %-5s %s\n"

  if(sortbystate) {
    printf("%15s %s %s\n", 
	   "Option", "State", "Flags");
    printf("%15s %s %s\n",
	   "===============", "=====", "====================");
    doneheader = 1;

    while((temp = offhead)) {
      printf(FORMAT, temp->name, "off", temp->flags);
      free(temp->name);
      free(temp->flags);
      offhead = temp->next;
      free(temp);
    }
  }

  if(!doneheader) {
    printf("%15s %s %s\n", "Option", "State", "Flags");
    printf("%15s %s %s\n",
	   "===============", "=====", "====================");
  }

  while((temp = onhead)) {
    printf(FORMAT, temp->name, temp->state ? "on" : "off", temp->flags);
    free(temp->name);
    free(temp->flags);
    onhead = temp->next;
    free(temp);
  }

  return 0;
}


static void die(const char *why) {
  fprintf(stderr, "%s: %s: %s\n", whoami, why, strerror(errno));
  exit(-1);
}

static void usage(void) {
  fprintf(stderr, "%s: usage:\n"
	  "%s [ -d configdir ] [ -s ]\n"
	  "%s [ -d configdir ] option\n"
	  "%s [ -d configdir ] [ -f ] option [ on | off ]\n",
	  whoami, whoami, whoami, whoami);
}
