/*-
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: loadalias.c,v 1.17 1998/06/07 03:54:41 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "id.h"
#include "loadalias.h"

#if __FreeBSD__ >= 3
#ifdef __ELF__
#define _PATH_ALIAS_PREFIX "libalias.so.2"	/* dlopen() searches */
#else
#define _PATH_ALIAS_PREFIX "libalias.so.2.5"	/* dlopen() searches */
#endif
#else
#define _PATH_ALIAS_PREFIX "/usr/lib/libalias.so.2."
#endif

#define off(item) ((int)&(((struct aliasHandlers *)0)->item))
#define entry(a) { off(a), "_PacketAlias" #a }

#ifndef RTLD_NOW
#define RTLD_NOW 1		/* really RTLD_LAZY */
#endif

static struct {
  int offset;
  const char *name;
} map[] = {
  entry(GetFragment),
  entry(Init),
  entry(In),
  entry(Out),
  entry(RedirectAddr),
  entry(RedirectPort),
  entry(SaveFragment),
  entry(SetAddress),
  entry(SetMode),
  entry(FragmentIn),
  { 0, 0 }
};

struct aliasHandlers PacketAlias;

int 
alias_Load()
{
  const char *path;
  const char *env;
  int i;

  if (PacketAlias.dl)
    return 0;

  path = _PATH_ALIAS_PREFIX;
  env = getenv("_PATH_ALIAS_PREFIX");
  if (env) {
    if (ID0realuid() == 0)
      path = env;
    else
      log_Printf(LogALERT, "Ignoring environment _PATH_ALIAS_PREFIX"
                " value (%s)\n", env);
  }

  PacketAlias.dl = dlopen(path, RTLD_NOW);
  if (PacketAlias.dl == (void *) 0) {
    /* Look for _PATH_ALIAS_PREFIX with any number appended */
    int plen;

    plen = strlen(path);
    if (plen && plen < MAXPATHLEN - 1 && path[plen-1] == '.') {
      DIR *d;
      char p[MAXPATHLEN], *fix, *file;
      const char *dir;

      strcpy(p, path);
      if ((file = strrchr(p, '/')) != NULL) {
        fix = file;
        *file++ = '\0';
        dir = p;
      } else {
        fix = NULL;
        file = p;
        dir = ".";
      }
      if ((d = opendir(dir))) {
        struct dirent *entry;
        int flen;
        char *end;
        long maxver, ver;

        if (fix)
          *fix = '/';
        maxver = -1;
        flen = strlen(file);
        while ((entry = readdir(d)))
          if (entry->d_namlen > flen && !strncmp(entry->d_name, file, flen)) {
            ver = strtol(entry->d_name + flen, &end, 10);
            strcpy(p + plen, entry->d_name + flen);
            if (ver >= 0 && *end == '\0' && ver > maxver &&
                access(p, R_OK) == 0)
              maxver = ver;
          }
        closedir(d);

        if (maxver > -1) {
          sprintf(p + plen, "%ld", maxver);
          PacketAlias.dl = dlopen(p, RTLD_NOW);
        }
      }
    }
    if (PacketAlias.dl == (void *) 0) {
      log_Printf(LogWARN, "_PATH_ALIAS_PREFIX (%s*): Invalid lib: %s\n",
	        path, dlerror());
      return -1;
    }
  }
  for (i = 0; map[i].name; i++) {
    *(void **)((char *)&PacketAlias + map[i].offset) =
      dlsym(PacketAlias.dl, map[i].name);
    if (*(void **)((char *)&PacketAlias + map[i].offset) == (void *)0) {
      log_Printf(LogWARN, "_PATH_ALIAS (%s*): %s: %s\n", path,
		map[i].name, dlerror());
      alias_Unload();
      return -1;
    }
  }

  (*PacketAlias.Init)();

  return 0;
}

void 
alias_Unload()
{
  if (PacketAlias.dl) {
    dlclose(PacketAlias.dl);
    memset(&PacketAlias, '\0', sizeof PacketAlias);
  }
}
