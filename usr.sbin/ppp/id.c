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
 *	$Id: id.c,v 1.5 1997/12/27 19:23:12 brian Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include "command.h"
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
static int euid;

void
ID0init()
{
  uid = getuid();
  euid = geteuid();
}

static void
ID0setuser(void)
{
  if (seteuid(uid) == -1) {
    LogPrintf(LogERROR, "ID0setuser: Unable to seteuid!\n");
    Cleanup(EX_NOPERM);
  }
}

uid_t
ID0realuid()
{
  return uid;
}

static void
ID0set0(void)
{
  if (seteuid(euid) == -1) {
    LogPrintf(LogERROR, "ID0set0: Unable to seteuid!\n");
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
ID0write(int fd, const void *data, size_t len)
{
  int ret;

  ID0set0();
  ret = write(fd, data, len);
  LogPrintf(LogID0, "%d = write(%d, data, %d)\n", ret, fd, len);
  ID0setuser();
  return ret;
}

int
ID0uu_lock(const char *basettyname)
{
  int ret;

  ID0set0();
  ret = uu_lock(basettyname);
  LogPrintf(LogID0, "%d = uu_lock(\"%s\")\n", ret, basettyname);
  ID0setuser();
  return ret;
}

int
ID0uu_unlock(const char *basettyname)
{
  int ret;

  ID0set0();
  ret = uu_unlock(basettyname);
  LogPrintf(LogID0, "%d = uu_unlock(\"%s\")\n", ret, basettyname);
  ID0setuser();
  return ret;
}
