/*-
 * Copyright (c) 1999 Brian Somers <brian@Awfulhak.org>
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
 *	$Id: exec.c,v 1.4 1999/06/01 19:08:57 brian Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <termios.h>
#include <unistd.h>

#include "layer.h"
#include "defs.h"
#include "mbuf.h"
#include "log.h"
#include "sync.h"
#include "timer.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "fsm.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "async.h"
#include "slcompress.h"
#include "iplist.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "physical.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "chat.h"
#include "command.h"
#include "bundle.h"
#include "prompt.h"
#include "auth.h"
#include "chap.h"
#include "cbcp.h"
#include "datalink.h"
#include "exec.h"

static struct device execdevice = {
  EXEC_DEVICE,
  "exec",
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

struct device *
exec_iov2device(int type, struct physical *p, struct iovec *iov,
                int *niov, int maxiov)
{
  if (type == EXEC_DEVICE) {
    free(iov[(*niov)++].iov_base);
    physical_SetupStack(p, execdevice.name, PHYSICAL_FORCE_ASYNC);
    return &execdevice;
  }

  return NULL;
}

struct device *
exec_Create(struct physical *p)
{
  if (p->fd < 0 && *p->name.full == '!') {
    int fids[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fids) < 0)
      log_Printf(LogPHASE, "Unable to create pipe for line exec: %s\n",
	         strerror(errno));
    else {
      int stat, argc;
      pid_t pid;
      char *argv[MAXARGS];

      stat = fcntl(fids[0], F_GETFL, 0);
      if (stat > 0) {
        stat |= O_NONBLOCK;
        fcntl(fids[0], F_SETFL, stat);
      }
      switch ((pid = fork())) {
        case -1:
          log_Printf(LogPHASE, "Unable to create pipe for line exec: %s\n",
	             strerror(errno));
          break;

        case  0:
          close(fids[0]);
          timer_TermService();
          setuid(geteuid());

          switch (fork()) {
            case 0:
              break;

            case -1:
              log_Printf(LogPHASE, "Unable to fork to drop parent: %s\n",
	                 strerror(errno));
            default:
              _exit(127);
          }

          fids[1] = fcntl(fids[1], F_DUPFD, 3);
          dup2(fids[1], STDIN_FILENO);
          dup2(fids[1], STDOUT_FILENO);
          dup2(fids[1], STDERR_FILENO);

          log_Printf(LogDEBUG, "Exec'ing ``%s''\n", p->name.base);
          argc = MakeArgs(p->name.base, argv, VECSIZE(argv));
          command_Expand(argv, argc, (char const *const *)argv,
                         p->dl->bundle, 0);
          execvp(*argv, argv);
          fprintf(stderr, "execvp failed: %s: %s\r\n", *argv, strerror(errno));
          _exit(127);
          break;

        default:
          close(fids[1]);
          p->fd = fids[0];
          waitpid(pid, &stat, 0);
          log_Printf(LogDEBUG, "Using descriptor %d for child\n", p->fd);
          physical_SetupStack(p, execdevice.name, PHYSICAL_FORCE_ASYNC);
          return &execdevice;
      }
    }
  }

  return NULL;
}
