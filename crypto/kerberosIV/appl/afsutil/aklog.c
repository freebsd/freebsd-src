/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if defined(HAVE_SYS_IOCTL_H) && SunOS != 40
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <err.h>
#include <krb.h>
#include <kafs.h>

#include <roken.h>

RCSID("$Id: aklog.c,v 1.24.2.1 2000/06/23 02:31:15 assar Exp $");

static int debug = 0;

static void
DEBUG(const char *, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
;

static void
DEBUG(const char *fmt, ...)
{
  va_list ap;
  if (debug) {
    va_start(ap, fmt);
    vwarnx(fmt, ap);
    va_end(ap);
  }
}

static char *
expand_1 (const char *cell, const char *filename)
{
  FILE *f;
  static char buf[128];
  char *p;

  f = fopen(filename, "r");
  if(f == NULL)
    return NULL;
  while(fgets(buf, sizeof(buf), f) != NULL) {
    if(buf[0] == '>') {
      for(p=buf; *p && !isspace(*p) && *p != '#'; p++)
	  ;
      *p = '\0';
      if(strstr(buf, cell)){
	fclose(f);
	return buf + 1;
      }
    }
    buf[0] = 0;
  }
  fclose(f);
  return NULL;
}

static const char *
expand_cell_name(const char *cell)
{
  char *ret;

  ret = expand_1(cell, _PATH_CELLSERVDB);
  if (ret != NULL)
    return ret;
  ret = expand_1(cell, _PATH_ARLA_CELLSERVDB);
  if (ret != NULL)
    return ret;
  return cell;
}

static int
createuser (const char *cell)
{
  char cellbuf[64];
  char name[ANAME_SZ];
  char instance[INST_SZ];
  char realm[REALM_SZ];
  char cmd[1024];

  if (cell == NULL) {
    FILE *f;
    int len;

    f = fopen (_PATH_THISCELL, "r");
    if (f == NULL)
      f = fopen (_PATH_ARLA_THISCELL, "r");
    if (f == NULL)
      err (1, "open(%s, %s)", _PATH_THISCELL, _PATH_ARLA_THISCELL);
    if (fgets (cellbuf, sizeof(cellbuf), f) == NULL)
      err (1, "read cellname from %s %s", _PATH_THISCELL, _PATH_ARLA_THISCELL);
    fclose (f);
    len = strlen(cellbuf);
    if (cellbuf[len-1] == '\n')
      cellbuf[len-1] = '\0';
    cell = cellbuf;
  }

  if(krb_get_default_principal(name, instance, realm))
    errx (1, "Could not even figure out who you are");

  snprintf (cmd, sizeof(cmd),
	    "pts createuser %s%s%s@%s -cell %s",
	    name, *instance ? "." : "", instance, strlwr(realm),
	    cell);
  DEBUG("Executing %s", cmd);
  return system(cmd);
}

int
main(int argc, char **argv)
{
  int i;
  int do_aklog = -1;
  int do_createuser = -1;
  const char *cell = NULL;
  char *realm = NULL;
  char cellbuf[64];
  
  set_progname (argv[0]);

  if(!k_hasafs())
    exit(1);

  for(i = 1; i < argc; i++){
    if(!strncmp(argv[i], "-createuser", 11)){
      do_createuser = do_aklog = 1;

    }else if(!strncmp(argv[i], "-c", 2) && i + 1 < argc){
      cell = expand_cell_name(argv[++i]);
      do_aklog = 1;

    }else if(!strncmp(argv[i], "-k", 2) && i + 1 < argc){
      realm = argv[++i];

    }else if(!strncmp(argv[i], "-p", 2) && i + 1 < argc){
      if(k_afs_cell_of_file(argv[++i], cellbuf, sizeof(cellbuf)))
	errx (1, "No cell found for file \"%s\".", argv[i]);
      else
	cell = cellbuf;
      do_aklog = 1;

    }else if(!strncmp(argv[i], "-unlog", 6)){
      exit(k_unlog());

    }else if(!strncmp(argv[i], "-hosts", 6)){
      warnx ("Argument -hosts is not implemented.");

    }else if(!strncmp(argv[i], "-zsubs", 6)){
      warnx("Argument -zsubs is not implemented.");

    }else if(!strncmp(argv[i], "-noprdb", 6)){
      warnx("Argument -noprdb is not implemented.");

    }else if(!strncmp(argv[i], "-d", 6)){
      debug = 1;

    }else{
      if(!strcmp(argv[i], ".") ||
	 !strcmp(argv[i], "..") ||
	 strchr(argv[i], '/')){
	DEBUG("I guess that \"%s\" is a filename.", argv[i]);
	if(k_afs_cell_of_file(argv[i], cellbuf, sizeof(cellbuf)))
	  errx (1, "No cell found for file \"%s\".", argv[i]);
	else {
	  cell = cellbuf;
	  DEBUG("The file \"%s\" lives in cell \"%s\".", argv[i], cell);
	}
      }else{
	cell = expand_cell_name(argv[i]);
	DEBUG("I guess that %s is cell %s.", argv[i], cell);
      }
      do_aklog = 1;
    }
    if(do_aklog == 1){
      do_aklog = 0;
      if(krb_afslog(cell, realm))
	errx (1, "Failed getting tokens for cell %s in realm %s.", 
	      cell?cell:"(local cell)", realm?realm:"(local realm)");
    }
    if(do_createuser == 1) {
      do_createuser = 0;
      if(createuser(cell))
	errx (1, "Failed creating user in cell %s", cell?cell:"(local cell)");
    }
  }
  if(do_aklog == -1 && do_createuser == -1 && krb_afslog(0, realm))
    errx (1, "Failed getting tokens for cell %s in realm %s.", 
	  cell?cell:"(local cell)", realm?realm:"(local realm)");
  return 0;
}
