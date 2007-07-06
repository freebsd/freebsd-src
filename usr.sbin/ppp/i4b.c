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

#include <sys/un.h>
#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/ioctl.h>
#endif
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#ifdef __NetBSD__
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_rbch_ioctl.h>
#else
#include <i4b/i4b_ioctl.h>
#include <i4b/i4b_rbch_ioctl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
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
#include "auth.h"
#include "chap.h"
#include "cbcp.h"
#include "datalink.h"
#include "main.h"
#include "i4b.h"

#define	Online(dev)	((dev)->mbits & TIOCM_CD)

struct i4bdevice {
  struct device dev;		/* What struct physical knows about */
  struct pppTimer Timer;	/* CD checks */
  int mbits;			/* Current DCD status */
  int carrier_seconds;		/* seconds before CD is *required* */
};

#define device2i4b(d) ((d)->type == I4B_DEVICE ? (struct i4bdevice *)d : NULL)

unsigned
i4b_DeviceSize(void)
{
  return sizeof(struct i4bdevice);
}

/*
 * i4b_Timeout() watches the DCD signal and mentions it if it's status
 * changes.
 */
static void
i4b_Timeout(void *data)
{
  struct physical *p = data;
  struct i4bdevice *dev = device2i4b(p->handler);
  int ombits, change;

  timer_Stop(&dev->Timer);
  dev->Timer.load = SECTICKS;		/* Once a second please */
  timer_Start(&dev->Timer);
  ombits = dev->mbits;

  if (p->fd >= 0) {
    if (ioctl(p->fd, TIOCMGET, &dev->mbits) < 0) {
      log_Printf(LogPHASE, "%s: ioctl error (%s)!\n", p->link.name,
                 strerror(errno));
      datalink_Down(p->dl, CLOSE_NORMAL);
      timer_Stop(&dev->Timer);
      return;
    }
  } else
    dev->mbits = 0;

  if (ombits == -1) {
    /* First time looking for carrier */
    if (Online(dev))
      log_Printf(LogPHASE, "%s: %s: CD detected\n", p->link.name, p->name.full);
    else if (++dev->carrier_seconds >= dev->dev.cd.delay) {
      log_Printf(LogPHASE, "%s: %s: No carrier"
                 " (increase ``set cd'' from %d ?)\n",
                 p->link.name, p->name.full, dev->dev.cd.delay);
      timer_Stop(&dev->Timer);
      /* i4b_AwaitCarrier() will notice */
    } else {
      /* Keep waiting */
      log_Printf(LogDEBUG, "%s: %s: Still no carrier (%d/%d)\n",
                 p->link.name, p->name.full, dev->carrier_seconds,
                 dev->dev.cd.delay);
      dev->mbits = -1;
    }
  } else {
    change = ombits ^ dev->mbits;
    if (change & TIOCM_CD) {
      if (dev->mbits & TIOCM_CD)
        log_Printf(LogDEBUG, "%s: offline -> online\n", p->link.name);
      else {
        log_Printf(LogDEBUG, "%s: online -> offline\n", p->link.name);
        log_Printf(LogPHASE, "%s: Carrier lost\n", p->link.name);
        datalink_Down(p->dl, CLOSE_NORMAL);
        timer_Stop(&dev->Timer);
      }
    } else
      log_Printf(LogDEBUG, "%s: Still %sline\n", p->link.name,
                 Online(dev) ? "on" : "off");
  }
}

static void
i4b_StartTimer(struct physical *p)
{
  struct i4bdevice *dev = device2i4b(p->handler);

  timer_Stop(&dev->Timer);
  dev->Timer.load = SECTICKS;
  dev->Timer.func = i4b_Timeout;
  dev->Timer.name = "i4b CD";
  dev->Timer.arg = p;
  log_Printf(LogDEBUG, "%s: Using i4b_Timeout [%p]\n",
             p->link.name, i4b_Timeout);
  timer_Start(&dev->Timer);
}

static int
i4b_AwaitCarrier(struct physical *p)
{
  struct i4bdevice *dev = device2i4b(p->handler);

  if (dev->mbits == -1) {
    if (dev->Timer.state == TIMER_STOPPED) {
      dev->carrier_seconds = 0;
      i4b_StartTimer(p);
    }
    return CARRIER_PENDING;			/* Not yet ! */
  }

  return Online(dev) ? CARRIER_OK : CARRIER_LOST;
}

static int
i4b_Raw(struct physical *p)
{
  int oldflag;

  log_Printf(LogDEBUG, "%s: Entering i4b_Raw\n", p->link.name);

  oldflag = fcntl(p->fd, F_GETFL, 0);
  if (oldflag < 0)
    return 0;
  fcntl(p->fd, F_SETFL, oldflag | O_NONBLOCK);

  return 1;
}

static void
i4b_Offline(struct physical *p)
{
  struct i4bdevice *dev = device2i4b(p->handler);

  if (p->fd >= 0) {
    timer_Stop(&dev->Timer);
    if (Online(dev)) {
      int dummy;

      dummy = 1;
      ioctl(p->fd, TIOCCDTR, &dummy);
    }
  }
}

static void
i4b_Cooked(struct physical *p)
{
  int oldflag;

  i4b_Offline(p);	/* In case of emergency close()s */

  if ((oldflag = fcntl(p->fd, F_GETFL, 0)) != -1)
    fcntl(p->fd, F_SETFL, oldflag & ~O_NONBLOCK);
}

static void
i4b_StopTimer(struct physical *p)
{
  struct i4bdevice *dev = device2i4b(p->handler);

  timer_Stop(&dev->Timer);
}

static void
i4b_Free(struct physical *p)
{
  struct i4bdevice *dev = device2i4b(p->handler);

  i4b_Offline(p);	/* In case of emergency close()s */
  free(dev);
}

static unsigned
i4b_Speed(struct physical *p)
{
  struct termios ios;
  unsigned ret;

  if (tcgetattr(p->fd, &ios) == -1 ||
      (ret = SpeedToUnsigned(cfgetispeed(&ios))) == 0)
    ret = 64000;

  return ret;
}

static const char *
i4b_OpenInfo(struct physical *p)
{
  struct i4bdevice *dev = device2i4b(p->handler);
  static char buf[26];

  if (Online(dev))
    snprintf(buf, sizeof buf, "carrier took %ds", dev->carrier_seconds);
  else
    *buf = '\0';

  return buf;
}

static int
i4b_Slot(struct physical *p)
{
  struct stat st;

  if (fstat(p->fd, &st) == 0)
    return minor(st.st_rdev);

  return -1;
}

static void
i4b_device2iov(struct device *d, struct iovec *iov, int *niov,
               int maxiov __unused, int *auxfd __unused, int *nauxfd __unused)
{
  struct i4bdevice *dev = device2i4b(d);
  int sz = physical_MaxDeviceSize();

  iov[*niov].iov_base = realloc(d, sz);
  if (iov[*niov].iov_base == NULL) {
    log_Printf(LogALERT, "Failed to allocate memory: %d\n", sz);
    AbortProgram(EX_OSERR);
  }
  iov[*niov].iov_len = sz;
  (*niov)++;

  if (dev->Timer.state != TIMER_STOPPED) {
    timer_Stop(&dev->Timer);
    dev->Timer.state = TIMER_RUNNING;
  }
}

static struct device basei4bdevice = {
  I4B_DEVICE,
  "i4b",
  0,
  { CD_REQUIRED, DEF_I4BCDDELAY },
  i4b_AwaitCarrier,
  NULL,
  i4b_Raw,
  i4b_Offline,
  i4b_Cooked,
  NULL,
  i4b_StopTimer,
  i4b_Free,
  NULL,
  NULL,
  i4b_device2iov,
  i4b_Speed,
  i4b_OpenInfo,
  i4b_Slot
};

struct device *
i4b_iov2device(int type, struct physical *p, struct iovec *iov, int *niov,
               int maxiov __unused, int *auxfd __unused, int *nauxfd __unused)
{
  if (type == I4B_DEVICE) {
    struct i4bdevice *dev = (struct i4bdevice *)iov[(*niov)++].iov_base;

    dev = realloc(dev, sizeof *dev);	/* Reduce to the correct size */
    if (dev == NULL) {
      log_Printf(LogALERT, "Failed to allocate memory: %d\n",
                 (int)(sizeof *dev));
      AbortProgram(EX_OSERR);
    }

    /* Refresh function pointers etc */
    memcpy(&dev->dev, &basei4bdevice, sizeof dev->dev);

    physical_SetupStack(p, dev->dev.name, PHYSICAL_NOFORCE);
    if (dev->Timer.state != TIMER_STOPPED) {
      dev->Timer.state = TIMER_STOPPED;
      p->handler = &dev->dev;		/* For the benefit of StartTimer */
      i4b_StartTimer(p);
    }
    return &dev->dev;
  }

  return NULL;
}

struct device *
i4b_Create(struct physical *p)
{
  struct i4bdevice *dev;
  int oldflag, dial;
  msg_vr_req_t req;
  telno_t number;

  if (p->fd < 0 || ioctl(p->fd, I4B_RBCH_VR_REQ, &req))
    /* Don't want this */
    return NULL;

  /*
   * We don't bother validating the version.... all versions of i4b that
   * support I4B_RBCH_VR_REQ are fair game :-)
   */

  if (*p->name.full == '\0') {
    physical_SetDevice(p, ttyname(p->fd));
    log_Printf(LogDEBUG, "%s: Input is an i4b version %d.%d.%d isdn "
               "device (%s)\n", p->link.name, req.version, req.release,
               req.step, p->name.full);
    dial = 0;
  } else {
    log_Printf(LogDEBUG, "%s: Opened %s (i4b version %d.%d.%d)\n",
               p->link.name, p->name.full, req.version, req.release, req.step);
    dial = 1;
  }

  /* We're gonna return an i4bdevice (unless something goes horribly wrong) */

  if ((dev = malloc(sizeof *dev)) == NULL) {
    /* Complete failure - parent doesn't continue trying to ``create'' */
    close(p->fd);
    p->fd = -1;
    return NULL;
  }

  memcpy(&dev->dev, &basei4bdevice, sizeof dev->dev);
  memset(&dev->Timer, '\0', sizeof dev->Timer);
  dev->mbits = -1;

  switch (p->cfg.cd.necessity) {
    case CD_VARIABLE:
      dev->dev.cd.delay = p->cfg.cd.delay;
      break;
    case CD_REQUIRED:
      dev->dev.cd = p->cfg.cd;
      break;
    case CD_NOTREQUIRED:
      log_Printf(LogWARN, "%s: Carrier must be set, using ``set cd %d!''\n",
                 p->link.name, dev->dev.cd.delay);
    case CD_DEFAULT:
      break;
  }

  oldflag = fcntl(p->fd, F_GETFL, 0);
  if (oldflag < 0) {
    /* Complete failure - parent doesn't continue trying to ``create'' */

    log_Printf(LogWARN, "%s: Open: Cannot get physical flags: %s\n",
               p->link.name, strerror(errno));
    i4b_Cooked(p);
    close(p->fd);
    p->fd = -1;
    free(dev);
    return NULL;
  } else
    fcntl(p->fd, F_SETFL, oldflag & ~O_NONBLOCK);

  if (dial) {
    strncpy(number, datalink_ChoosePhoneNumber(p->dl), sizeof number - 1);
    number[sizeof number - 1] = '\0';
    if (number[0] == '\0')
      dial = 0;
  }
  if (dial && ioctl(p->fd, I4B_RBCH_DIALOUT, number) == -1) {
    /* Complete failure - parent doesn't continue trying to ``create'' */

    log_Printf(LogWARN, "%s: ioctl(I4B_RBCH_DIALOUT): %s\n",
               p->link.name, strerror(errno));
    i4b_Cooked(p);
    close(p->fd);
    p->fd = -1;
    free(dev);
    return NULL;
  }

  physical_SetupStack(p, dev->dev.name, PHYSICAL_FORCE_SYNC);

  return &dev->dev;
}
