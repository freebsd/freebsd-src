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
 *	$Id:$
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>
#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/ioctl.h>
#include <util.h>
#else
#include <libutil.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "layer.h"
#include "defs.h"
#include "mbuf.h"
#include "log.h"
#include "id.h"
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
#include "tty.h"

#define	Online(p)	((p)->mbits & TIOCM_CD)

static int
tty_Lock(struct physical *p, int tunno)
{
  int res;
  FILE *lockfile;
  char fn[MAXPATHLEN];

  if (*p->name.full != '/')
    return 0;

  if (p->type != PHYS_DIRECT &&
      (res = ID0uu_lock(p->name.base)) != UU_LOCK_OK) {
    if (res == UU_LOCK_INUSE)
      log_Printf(LogPHASE, "%s: %s is in use\n", p->link.name, p->name.full);
    else
      log_Printf(LogPHASE, "%s: %s is in use: uu_lock: %s\n",
                 p->link.name, p->name.full, uu_lockerr(res));
    return (-1);
  }

  snprintf(fn, sizeof fn, "%s%s.if", _PATH_VARRUN, p->name.base);
  lockfile = ID0fopen(fn, "w");
  if (lockfile != NULL) {
    fprintf(lockfile, "%s%d\n", TUN_NAME, tunno);
    fclose(lockfile);
  }
#ifndef RELEASE_CRUNCH
  else
    log_Printf(LogALERT, "%s: Can't create %s: %s\n",
               p->link.name, fn, strerror(errno));
#endif

  return 0;
}

static void
tty_Unlock(struct physical *p)
{
  char fn[MAXPATHLEN];

  if (*p->name.full != '/')
    return;

  snprintf(fn, sizeof fn, "%s%s.if", _PATH_VARRUN, p->name.base);
#ifndef RELEASE_CRUNCH
  if (ID0unlink(fn) == -1)
    log_Printf(LogALERT, "%s: Can't remove %s: %s\n",
               p->link.name, fn, strerror(errno));
#else
  ID0unlink(fn);
#endif

  if (p->type != PHYS_DIRECT && ID0uu_unlock(p->name.base) == -1)
    log_Printf(LogALERT, "%s: Can't uu_unlock %s\n", p->link.name, fn);
}

static void
tty_SetupDevice(struct physical *p)
{
  struct termios rstio;
  int oldflag;

  tcgetattr(p->fd, &rstio);
  p->ios = rstio;

  log_Printf(LogDEBUG, "%s: tty_SetupDevice: physical (get): fd = %d,"
             " iflag = %lx, oflag = %lx, cflag = %lx\n", p->link.name, p->fd,
             (u_long)rstio.c_iflag, (u_long)rstio.c_oflag,
             (u_long)rstio.c_cflag);

  cfmakeraw(&rstio);
  if (p->cfg.rts_cts)
    rstio.c_cflag |= CLOCAL | CCTS_OFLOW | CRTS_IFLOW;
  else {
    rstio.c_cflag |= CLOCAL;
    rstio.c_iflag |= IXOFF;
  }
  rstio.c_iflag |= IXON;
  if (p->type != PHYS_DEDICATED)
    rstio.c_cflag |= HUPCL;

  if (p->type != PHYS_DIRECT) {
      /* Change tty speed when we're not in -direct mode */
      rstio.c_cflag &= ~(CSIZE | PARODD | PARENB);
      rstio.c_cflag |= p->cfg.parity;
      if (cfsetspeed(&rstio, IntToSpeed(p->cfg.speed)) == -1)
	log_Printf(LogWARN, "%s: %s: Unable to set speed to %d\n",
		  p->link.name, p->name.full, p->cfg.speed);
  }
  tcsetattr(p->fd, TCSADRAIN, &rstio);
  log_Printf(LogDEBUG, "%s: physical (put): iflag = %lx, oflag = %lx, "
            "cflag = %lx\n", p->link.name, (u_long)rstio.c_iflag,
            (u_long)rstio.c_oflag, (u_long)rstio.c_cflag);

  if (ioctl(p->fd, TIOCMGET, &p->mbits) == -1) {
    if (p->type != PHYS_DIRECT) {
      log_Printf(LogWARN, "%s: Open: Cannot get physical status: %s\n",
                 p->link.name, strerror(errno));
      physical_Close(p);
      return;
    } else
      p->mbits = TIOCM_CD;
  }
  log_Printf(LogDEBUG, "%s: Open: physical control = %o\n",
             p->link.name, p->mbits);

  oldflag = fcntl(p->fd, F_GETFL, 0);
  if (oldflag < 0) {
    log_Printf(LogWARN, "%s: Open: Cannot get physical flags: %s\n",
               p->link.name, strerror(errno));
    physical_Close(p);
    return;
  } else
    fcntl(p->fd, F_SETFL, oldflag & ~O_NONBLOCK);

  physical_SetupStack(p, 0);
}

static int
tty_Open(struct physical *p)
{
  if (*p->name.full == '/') {
    p->mbits = 0;
    if (tty_Lock(p, p->dl->bundle->unit) != -1) {
      p->fd = ID0open(p->name.full, O_RDWR | O_NONBLOCK);
      if (p->fd < 0) {
        log_Printf(LogPHASE, "%s: Open(\"%s\"): %s\n",
                   p->link.name, p->name.full, strerror(errno));
        tty_Unlock(p);
      } else if (!isatty(p->fd)) {
        log_Printf(LogPHASE, "%s: Open(\"%s\"): Not a tty\n",
                   p->link.name, p->name.full);
        close(p->fd);
        p->fd = -1;
        tty_Unlock(p);
      } else {
        log_Printf(LogDEBUG, "%s: Opened %s\n", p->link.name, p->name.full);
        tty_SetupDevice(p);
      }
    }
  }

  return p->fd >= 0;
}

int
tty_OpenStdin(struct physical *p)
{
  if (isatty(STDIN_FILENO)) {
    p->mbits = 0;
    log_Printf(LogDEBUG, "%s: tty_Open: stdin is a tty\n", p->link.name);
    physical_SetDevice(p, ttyname(STDIN_FILENO));
    if (tty_Lock(p, p->dl->bundle->unit) == -1)
      close(STDIN_FILENO);
    else {
      p->fd = STDIN_FILENO;
      tty_SetupDevice(p);
    }
  }

  return p->fd >= 0;
}

/*
 * tty_Timeout() watches the DCD signal and mentions it if it's status
 * changes.
 */
static void
tty_Timeout(void *data)
{
  struct physical *p = data;
  int ombits, change;

  timer_Stop(&p->Timer);
  p->Timer.load = SECTICKS;		/* Once a second please */
  timer_Start(&p->Timer);
  ombits = p->mbits;

  if (p->fd >= 0) {
    if (ioctl(p->fd, TIOCMGET, &p->mbits) < 0) {
      log_Printf(LogPHASE, "%s: ioctl error (%s)!\n", p->link.name,
                 strerror(errno));
      datalink_Down(p->dl, CLOSE_NORMAL);
      timer_Stop(&p->Timer);
      return;
    }
  } else
    p->mbits = 0;

  if (ombits == -1) {
    /* First time looking for carrier */
    if (Online(p))
      log_Printf(LogDEBUG, "%s: %s: CD detected\n", p->link.name, p->name.full);
    else if (p->cfg.cd.required) {
      log_Printf(LogPHASE, "%s: %s: Required CD not detected\n",
                 p->link.name, p->name.full);
      datalink_Down(p->dl, CLOSE_NORMAL);
    } else {
      log_Printf(LogPHASE, "%s: %s doesn't support CD\n",
                 p->link.name, p->name.full);
      timer_Stop(&p->Timer);
      p->mbits = TIOCM_CD;
    }
  } else {
    change = ombits ^ p->mbits;
    if (change & TIOCM_CD) {
      if (p->mbits & TIOCM_CD)
        log_Printf(LogDEBUG, "%s: offline -> online\n", p->link.name);
      else {
        log_Printf(LogDEBUG, "%s: online -> offline\n", p->link.name);
        log_Printf(LogPHASE, "%s: Carrier lost\n", p->link.name);
        datalink_Down(p->dl, CLOSE_NORMAL);
        timer_Stop(&p->Timer);
      }
    } else
      log_Printf(LogDEBUG, "%s: Still %sline\n", p->link.name,
                 Online(p) ? "on" : "off");
  }
}

static void
tty_StartTimer(struct physical *p)
{
  timer_Stop(&p->Timer);
  p->Timer.load = SECTICKS * p->cfg.cd.delay;
  p->Timer.func = tty_Timeout;
  p->Timer.name = "tty CD";
  p->Timer.arg = p;
  log_Printf(LogDEBUG, "%s: Using tty_Timeout [%p]\n",
             p->link.name, tty_Timeout);
  p->mbits = -1;		/* So we know it's the first time */
  timer_Start(&p->Timer);
}

static int
tty_Raw(struct physical *p)
{
  struct termios rstio;
  int oldflag;

  if (physical_IsSync(p))
    return 1;

  log_Printf(LogDEBUG, "%s: Entering physical_Raw\n", p->link.name);

  if (p->type != PHYS_DIRECT && p->fd >= 0 && !Online(p))
    log_Printf(LogDEBUG, "%s: Raw: descriptor = %d, mbits = %x\n",
              p->link.name, p->fd, p->mbits);

  tcgetattr(p->fd, &rstio);
  cfmakeraw(&rstio);
  if (p->cfg.rts_cts)
    rstio.c_cflag |= CLOCAL | CCTS_OFLOW | CRTS_IFLOW;
  else
    rstio.c_cflag |= CLOCAL;

  if (p->type != PHYS_DEDICATED)
    rstio.c_cflag |= HUPCL;

  tcsetattr(p->fd, TCSANOW, &rstio);

  oldflag = fcntl(p->fd, F_GETFL, 0);
  if (oldflag < 0)
    return 0;
  fcntl(p->fd, F_SETFL, oldflag | O_NONBLOCK);

  if (ioctl(p->fd, TIOCMGET, &p->mbits) == 0)
    tty_StartTimer(p);

  return 1;
}

static void
tty_Offline(struct physical *p)
{
  if (p->fd >= 0) {
    timer_Stop(&p->Timer);
    p->mbits &= ~TIOCM_DTR;
    if (Online(p)) {
      struct termios tio;

      tcgetattr(p->fd, &tio);
      if (cfsetspeed(&tio, B0) == -1)
        log_Printf(LogWARN, "%s: Unable to set physical to speed 0\n",
                   p->link.name);
      else
        tcsetattr(p->fd, TCSANOW, &tio);
    }
  }
}

static void
tty_Cooked(struct physical *p)
{
  int oldflag;

  tcflush(p->fd, TCIOFLUSH);
  if (!physical_IsSync(p)) {
    tcsetattr(p->fd, TCSAFLUSH, &p->ios);
    oldflag = fcntl(p->fd, F_GETFL, 0);
    if (oldflag == 0)
      fcntl(p->fd, F_SETFL, oldflag & ~O_NONBLOCK);
  }
}

static void
tty_Close(struct physical *p)
{
  tty_Unlock(p);
}

static void
tty_Restored(struct physical *p)
{
  if (p->Timer.state != TIMER_STOPPED) {
    p->Timer.state = TIMER_STOPPED;	/* Special - see physical2iov() */
    tty_StartTimer(p);
  }
}

static int
tty_Speed(struct physical *p)
{
  struct termios rstio;

  if (tcgetattr(p->fd, &rstio) == -1)
    return 0;

  return SpeedToInt(cfgetispeed(&rstio));
}

static const char *
tty_OpenInfo(struct physical *p)
{
  static char buf[13];

  if (Online(p))
    strcpy(buf, "with");
  else
    strcpy(buf, "no");
  strcat(buf, " carrier");
  return buf;
}

const struct device ttydevice = {
  TTY_DEVICE,
  "tty",
  tty_Open,
  tty_Raw,
  tty_Offline,
  tty_Cooked,
  tty_Close,
  tty_Restored,
  tty_Speed,
  tty_OpenInfo
};
