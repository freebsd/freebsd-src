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
 *	$Id: server.c,v 1.15 1997/12/24 09:29:14 brian Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "loadalias.h"
#include "defs.h"
#include "vars.h"
#include "server.h"
#include "id.h"

int server = -1;

static struct sockaddr_un ifsun;
static char *rm;

int
ServerLocalOpen(const char *name, mode_t mask)
{
  int s;

  if (VarLocalAuth == LOCAL_DENY) {
    LogPrintf(LogERROR, "Local: Can't open socket %s: No password "
	      "in ppp.secret\n", name);
    return 1;
  }

  if (mode & MODE_INTER) {
    LogPrintf(LogERROR, "Local: Can't open socket in interactive mode\n");
    return 1;
  }

  memset(&ifsun, '\0', sizeof ifsun);
  ifsun.sun_len = strlen(name);
  if (ifsun.sun_len > sizeof ifsun.sun_path - 1) {
    LogPrintf(LogERROR, "Local: %s: Path too long\n", name);
    return 2;
  }
  ifsun.sun_family = AF_LOCAL;
  strcpy(ifsun.sun_path, name);

  s = ID0socket(PF_LOCAL, SOCK_STREAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "Local: socket: %s\n", strerror(errno));
    return 3;
  }
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &s, sizeof s);
  if (mask != (mode_t)-1)
    mask = umask(mask);
  if (bind(s, (struct sockaddr *)&ifsun, sizeof ifsun) < 0) {
    if (mask != (mode_t)-1)
      umask(mask);
    LogPrintf(LogERROR, "Local: bind: %s\n", strerror(errno));
    if (errno == EADDRINUSE && VarTerm)
      fprintf(VarTerm, "Wait for a while, then try again.\n");
    close(s);
    return 4;
  }
  if (mask != (mode_t)-1)
    umask(mask);
  if (listen(s, 5) != 0) {
    LogPrintf(LogERROR, "Local: Unable to listen to socket - OS overload?\n");
    close(s);
    ID0unlink(name);
    return 5;
  }
  ServerClose();
  server = s;
  rm = ifsun.sun_path;
  LogPrintf(LogPHASE, "Listening at local socket %s.\n", name);
  return 0;
}

int
ServerTcpOpen(int port)
{
  struct sockaddr_in ifsin;
  int s;

  if (VarLocalAuth == LOCAL_DENY) {
    LogPrintf(LogERROR, "Tcp: Can't open socket %d: No password "
	      "in ppp.secret\n", port);
    return 6;
  }

  if (mode & MODE_INTER) {
    LogPrintf(LogERROR, "Tcp: Can't open socket in interactive mode\n");
    return 6;
  }

  s = ID0socket(PF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "Tcp: socket: %s\n", strerror(errno));
    return 7;
  }
  memset(&ifsin, '\0', sizeof ifsin);
  ifsin.sin_family = AF_INET;
  ifsin.sin_addr.s_addr = INADDR_ANY;
  ifsin.sin_port = htons(port);
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &s, sizeof s);
  if (bind(s, (struct sockaddr *)&ifsin, sizeof ifsin) < 0) {
    LogPrintf(LogERROR, "Tcp: bind: %s\n", strerror(errno));
    if (errno == EADDRINUSE && VarTerm)
      fprintf(VarTerm, "Wait for a while, then try again.\n");
    close(s);
    return 8;
  }
  if (listen(s, 5) != 0) {
    LogPrintf(LogERROR, "Tcp: Unable to listen to socket - OS overload?\n");
    close(s);
    return 9;
  }
  ServerClose();
  server = s;
  LogPrintf(LogPHASE, "Listening at port %d.\n", port);
  return 0;
}

void
ServerClose()
{
  if (server >= 0) {
    close(server);
    if (rm) {
      ID0unlink(rm);
      rm = 0;
    }
  }
  server = -1;
}
