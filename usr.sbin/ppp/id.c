/*
 * $Id: defs.c,v 1.1 1997/10/26 01:02:30 brian Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "mbuf.h"
#include "log.h"
#include "main.h"
#ifdef __OpenBSD__
#include <util.h>
#else
#include <libutil.h>
#endif
#include "id.h"

static int uid;
static int gid;
static int euid;
static int egid;

void
ID0init()
{
  uid = getuid();
  gid = getgid();
  euid = geteuid();
  egid = getegid();
}

static void
ID0setuser()
{
  if (setreuid(euid, uid) == -1) {
    LogPrintf(LogERROR, "ID0setuser: Unable to setreuid!\n");
    Cleanup(EX_NOPERM);
  }
}

uid_t
ID0realuid()
{
  return uid;
}

static void
ID0set0()
{
  if (setreuid(uid, euid) == -1) {
    LogPrintf(LogERROR, "ID0set0: Unable to setreuid!\n");
    Cleanup(EX_NOPERM);
  }
}

int
ID0ioctl(int fd, unsigned long req, void *arg)
{
  int ret;

  ID0set0();
  ret = ioctl(fd, req, arg);
  LogPrintf(LogID0, "%d = ioctl(%d, %d, %p)\n", ret, fd, req, arg);
  ID0setuser();
  return ret;
}

int
ID0unlink(const char *name)
{
  int ret;

  ID0set0();
  ret = unlink(name);
  LogPrintf(LogID0, "%d = unlink(\"%s\")\n", ret, name);
  ID0setuser();
  return ret;
}

int
ID0socket(int domain, int type, int protocol)
{
  int ret;

  ID0set0();
  ret = socket(domain, type, protocol);
  LogPrintf(LogID0, "%d = socket(%d, %d, %d)\n", ret, domain, type, protocol);
  ID0setuser();
  return ret;
}

FILE *
ID0fopen(const char *path, const char *mode)
{
  FILE *ret;

  ID0set0();
  ret = fopen(path, mode);
  LogPrintf(LogID0, "%p = fopen(\"%s\", \"%s\")\n", ret, path, mode);
  ID0setuser();
  return ret;
}

int
ID0open(const char *path, int flags)
{
  int ret;

  ID0set0();
  ret = open(path, flags);
  LogPrintf(LogID0, "%d = open(\"%s\", %d)\n", ret, path, flags);
  ID0setuser();
  return ret;
}

int
ID0uu_lock(const char *ttyname)
{
  int ret;

  ID0set0();
  ret = uu_lock(ttyname);
  LogPrintf(LogID0, "%d = uu_lock(\"%s\")\n", ret, ttyname);
  ID0setuser();
  return ret;
}

int
ID0uu_unlock(const char *ttyname)
{
  int ret;

  ID0set0();
  ret = uu_unlock(ttyname);
  LogPrintf(LogID0, "%d = uu_unlock(\"%s\")\n", ret, ttyname);
  ID0setuser();
  return ret;
}
