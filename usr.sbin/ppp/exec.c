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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/socket.h>
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
#include "timer.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "fsm.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "async.h"
#include "descriptor.h"
#include "physical.h"
#include "mp.h"
#include "chat.h"
#include "command.h"
#include "auth.h"
#include "chap.h"
#include "cbcp.h"
#include "datalink.h"
#include "id.h"
#include "exec.h"

static struct device execdevice = {
  EXEC_DEVICE,
  "exec",
  { CD_NOTREQUIRED, 0 },
  NULL,
  NULL,
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
                int *niov, int maxiov, int *auxfd, int *nauxfd)
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

    p->fd--;	/* We own the device but maybe can't use it - change fd */

    if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fids) < 0)
      log_Printf(LogPHASE, "Unable to create pipe for line exec: %s\n",
	         strerror(errno));
    else {
      static int child_status;
      int stat, argc, i, ret, wret;
      pid_t pid, realpid;
      char *argv[MAXARGS];

      stat = fcntl(fids[0], F_GETFL, 0);
      if (stat > 0) {
        stat |= O_NONBLOCK;
        fcntl(fids[0], F_SETFL, stat);
      }
      realpid = getpid();
      switch ((pid = fork())) {
        case -1:
          log_Printf(LogPHASE, "Unable to create pipe for line exec: %s\n",
	             strerror(errno));
          close(fids[1]);
          break;

        case  0:
          close(fids[0]);
          timer_TermService();
          setuid(ID0realuid());

          child_status = 0;
          switch (vfork()) {
            case 0:
              break;

            case -1:
              ret = errno;
              log_Printf(LogPHASE, "Unable to fork to drop parent: %s\n",
	                 strerror(errno));
              _exit(ret);
              break;

            default:
              _exit(child_status);	/* The error from exec() ! */
          }

          log_Printf(LogDEBUG, "Exec'ing ``%s''\n", p->name.base);

          if ((argc = MakeArgs(p->name.base, argv, VECSIZE(argv),
                               PARSE_REDUCE|PARSE_NOHASH)) < 0) {
            log_Printf(LogWARN, "Syntax error in exec command\n");
            _exit(ESRCH);
          }

          command_Expand(argv, argc, (char const *const *)argv,
                         p->dl->bundle, 0, realpid);

          dup2(fids[1], STDIN_FILENO);
          dup2(fids[1], STDOUT_FILENO);
          dup2(fids[1], STDERR_FILENO);
          for (i = getdtablesize(); i > STDERR_FILENO; i--)
            fcntl(i, F_SETFD, 1);

          execvp(*argv, argv);
          child_status = errno;		/* Only works for vfork() */
          printf("execvp failed: %s: %s\r\n", *argv, strerror(child_status));
          _exit(child_status);
          break;

        default:
          close(fids[1]);
          while ((wret = waitpid(pid, &stat, 0)) == -1 && errno == EINTR)
            ;
          if (wret == -1) {
            log_Printf(LogWARN, "Waiting for child process: %s\n",
                       strerror(errno));
            close(fids[0]);
            break;
          } else if (WIFSIGNALED(stat)) {
            log_Printf(LogWARN, "Child process received sig %d !\n",
                       WTERMSIG(stat));
            close(fids[0]);
            break;
          } else if (WIFSTOPPED(stat)) {
            log_Printf(LogWARN, "Child process received stop sig %d !\n",
                       WSTOPSIG(stat));
            /* I guess that's ok.... */
          } else if ((ret = WEXITSTATUS(stat))) {
            log_Printf(LogWARN, "Cannot exec \"%s\": %s\n", p->name.base,
                       strerror(ret));
            close(fids[0]);
            break;
          }
          p->fd = fids[0];
          log_Printf(LogDEBUG, "Using descriptor %d for child\n", p->fd);
          physical_SetupStack(p, execdevice.name, PHYSICAL_FORCE_ASYNC);
          if (p->cfg.cd.necessity != CD_DEFAULT)
            log_Printf(LogWARN, "Carrier settings ignored\n");
          return &execdevice;
      }
      close(fids[0]);
    }
  }

  return NULL;
}
