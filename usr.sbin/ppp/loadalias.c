/*
 * $Id: $
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
#include "systems.h"
#include "mbuf.h"
#include "log.h"
#include "loadalias.h"
#include "vars.h"

#define _PATH_ALIAS "/usr/lib/libalias.so." ## __libalias_version

#define off(item) ((int)&(((struct aliasHandlers *)0)->item))
#define entry(a) { off(a), "_" #a }

static struct {
  int offset;
  char *name;
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
  char *path;
  char *env;
  int i;

  path = _PATH_ALIAS;
  env = getenv("_PATH_ALIAS");
  if (env)
    if (OrigUid() == 0)
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
