#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
#include "systems.h"
#include "mbuf.h"
#include "log.h"
#include "loadalias.h"
#include "vars.h"

#define _PATH_ALIAS "/usr/lib/libalias.so.2.1"

#define off(item) ((int)&(((struct aliasHandlers *)0)->item))
#define entry(a) { off(a), "_" #a }

static struct {
    int offset;
    char *name;
} map[] = {
    entry(GetNextFragmentPtr),
    entry(GetNextFragmentPtr),
    entry(InitPacketAlias),
    entry(PacketAliasIn),
    entry(PacketAliasOut),
    entry(PacketAliasRedirectAddr),
    entry(PacketAliasRedirectPort),
    entry(SaveFragmentPtr),
    entry(SetPacketAliasAddress),
    entry(SetPacketAliasMode),
    entry(FragmentAliasIn),
    { 0, 0 }
};

static void *dl;

int loadAliasHandlers(struct aliasHandlers *h)
{
    char *path;
    char *env;
    char *err;
    int i;

    path = _PATH_ALIAS;
    env = getenv("_PATH_ALIAS");
    if (env)
        if (OrigUid() == 0)
            path = env;
        else {
            logprintf("Ignoring environment _PATH_ALIAS value (%s)\n", env);
            printf("Ignoring environment _PATH_ALIAS value (%s)\n", env);
        }

    dl = dlopen(path, RTLD_LAZY);
    if (dl == (void *)0) {
        err = dlerror();
        logprintf("_PATH_ALIAS (%s): Invalid lib: %s\n", path, err);
        printf("_PATH_ALIAS (%s): Invalid lib: %s\n", path, err);
        return -1;
    }

    for (i = 0; map[i].name; i++) {
        *(void **)((char *)h + map[i].offset) = dlsym(dl, map[i].name);
        if (*(void **)((char *)h + map[i].offset) == (void *)0) {
            err = dlerror();
            logprintf("_PATH_ALIAS (%s): %s: %s\n", path, map[i].name, err);
            printf("_PATH_ALIAS (%s): %s: %s\n", path, map[i].name, err);
            (void)dlclose(dl);
            dl = (void *)0;
            return -1;
        }
    }

    VarInitPacketAlias();

    return 0;
}

void unloadAliasHandlers()
{
    if (dl) {
        dlclose(dl);
        dl = (void *)0;
    }
}
