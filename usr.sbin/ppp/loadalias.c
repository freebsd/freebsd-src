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
 *	$Id$
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <alias.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "systems.h"
#include "id.h"
#include "loadalias.h"
#include "defs.h"
#include "vars.h"

#define _PATH_ALIAS "/usr/lib/libalias.so." ## __libalias_version

#define off(item) ((int)&(((struct aliasHandlers *)0)->item))
#define entry(a) { off(a), "_" #a }

static struct {
  int offset;
  const char *name;
} map[] = {
  entry(PacketAliasGetFragment),
  entry(PacketAliasInit),
  entry(PacketAliasIn),
  entry(PacketAliasOut),
  entry(PacketAliasRedirectAddr),
  entry(PacketAliasRedirectPort),
  entry(PacketAliasSaveFragment),
  entry(PacketAliasSetAddress),
  entry(PacketAliasSetMode),
  entry(PacketAliasFragmentIn),
  { 0, 0 }
};

static void *dl;

int 
loadAliasHandlers(struct aliasHandlers * h)
{
  const char *path;
  const char *env;
  int i;

  path = _PATH_ALIAS;
  env = getenv("_PATH_ALIAS");
  if (env)
    if (ID0realuid() == 0)
      path = env;
    else
      LogPrintf(LogALERT, "Ignoring environment _PATH_ALIAS value (%s)\n", env);

  dl = dlopen(path, RTLD_LAZY);
  if (dl == (void *) 0) {
    LogPrintf(LogWARN, "_PATH_ALIAS (%s): Invalid lib: %s\n",
	      path, dlerror());
    return -1;
  }
  for (i = 0; map[i].name; i++) {
    *(void **) ((char *) h + map[i].offset) = dlsym(dl, map[i].name);
    if (*(void **) ((char *) h + map[i].offset) == (void *) 0) {
      LogPrintf(LogWARN, "_PATH_ALIAS (%s): %s: %s\n", path,
		map[i].name, dlerror());
      (void) dlclose(dl);
      dl = (void *) 0;
      return -1;
    }
  }

  VarPacketAliasInit();

  return 0;
}

void 
unloadAliasHandlers()
{
  if (dl) {
    dlclose(dl);
    dl = (void *) 0;
  }
}
