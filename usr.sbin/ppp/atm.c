/*-
 * Copyright (c) 2000 Jakob Stoklund Olesen <stoklund@taxidriver.dk>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netnatm/natm.h>

#include <errno.h>
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
#include "main.h"
#include "atm.h"

/* String identifying PPPoA */
#define PPPOA		"PPPoA"
#define PPPOA_LEN	(sizeof(PPPOA) - 1)

struct atmdevice {
  struct device dev;		/* What struct physical knows about */
};

#define device2atm(d) ((d)->type == ATM_DEVICE ? (struct atmdevice *)d : NULL)

int
atm_DeviceSize(void)
{
  return sizeof(struct atmdevice);
}

static ssize_t
atm_Sendto(struct physical *p, const void *v, size_t n)
{
  ssize_t ret = write(p->fd, v, n);
  if (ret < 0) {
    log_Printf(LogDEBUG, "atm_Sendto(%ld): %s\n", (long)n, strerror(errno));
    return ret;
  }
  return ret;
}

static ssize_t
atm_Recvfrom(struct physical *p, void *v, size_t n)
{
    ssize_t ret = read(p->fd, (char*)v, n);
    if (ret < 0) {
      log_Printf(LogDEBUG, "atm_Recvfrom(%ld): %s\n", (long)n, strerror(errno));
      return ret;
    }
    return ret;
}

static void
atm_Free(struct physical *p)
{
  struct atmdevice *dev = device2atm(p->handler);

  free(dev);
}

static void
atm_device2iov(struct device *d, struct iovec *iov, int *niov,
               int maxiov, int *auxfd, int *nauxfd)
{
  int sz = physical_MaxDeviceSize();

  iov[*niov].iov_base = realloc(d, sz);
  if (iov[*niov].iov_base == NULL) {
    log_Printf(LogALERT, "Failed to allocate memory: %d\n", sz);
    AbortProgram(EX_OSERR);
  }
  iov[*niov].iov_len = sz;
  (*niov)++;
}

static const struct device baseatmdevice = {
  ATM_DEVICE,
  "atm",
  0,
  { CD_NOTREQUIRED, 0 },
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  atm_Free,
  atm_Recvfrom,
  atm_Sendto,
  atm_device2iov,
  NULL,
  NULL,
  NULL
};

struct device *
atm_iov2device(int type, struct physical *p, struct iovec *iov, int *niov,
               int maxiov, int *auxfd, int *nauxfd)
{
  if (type == ATM_DEVICE) {
    struct atmdevice *dev = (struct atmdevice *)iov[(*niov)++].iov_base;

    dev = realloc(dev, sizeof *dev);	/* Reduce to the correct size */
    if (dev == NULL) {
      log_Printf(LogALERT, "Failed to allocate memory: %d\n",
                 (int)(sizeof *dev));
      AbortProgram(EX_OSERR);
    }

    /* Refresh function pointers etc */
    memcpy(&dev->dev, &baseatmdevice, sizeof dev->dev);

    physical_SetupStack(p, dev->dev.name, PHYSICAL_FORCE_SYNCNOACF);
    return &dev->dev;
  }

  return NULL;
}

static struct atmdevice *
atm_CreateDevice(struct physical *p, const char *iface, unsigned vpi,
                 unsigned vci)
{
  struct atmdevice *dev;
  struct sockaddr_natm sock;

  if ((dev = calloc(1, sizeof *dev)) == NULL) {
    log_Printf(LogWARN, "%s: Cannot allocate an atm device: %s\n",
               p->link.name, strerror(errno));
    return NULL;
  }

  sock.snatm_len = sizeof sock;
  sock.snatm_family = AF_NATM;
  strncpy(sock.snatm_if, iface, IFNAMSIZ);
  sock.snatm_vpi = vpi;
  sock.snatm_vci = vci;

  log_Printf(LogPHASE, "%s: Connecting to %s:%u.%u\n", p->link.name,
             iface, vpi, vci);

  p->fd = socket(PF_NATM, SOCK_DGRAM, PROTO_NATMAAL5);
  if (p->fd >= 0) {
    log_Printf(LogDEBUG, "%s: Opened atm socket %s\n", p->link.name,
               p->name.full);
    if (connect(p->fd, (struct sockaddr *)&sock, sizeof sock) == 0)
      return dev;
    else
      log_Printf(LogWARN, "%s: connect: %s\n", p->name.full, strerror(errno));
  } else
    log_Printf(LogWARN, "%s: socket: %s\n", p->name.full, strerror(errno));

  close(p->fd);
  p->fd = -1;
  free(dev);

  return NULL;
}

struct device *
atm_Create(struct physical *p)
{
  struct atmdevice *dev;

  dev = NULL;
  if (p->fd < 0 && !strncasecmp(p->name.full, PPPOA, PPPOA_LEN)
      && p->name.full[PPPOA_LEN] == ':') {
    char iface[25];
    unsigned vci, vpi;

    if (sscanf(p->name.full + PPPOA_LEN + 1, "%25[A-Za-z0-9]:%u.%u", iface,
               &vpi, &vci) != 3) {
      log_Printf(LogWARN, "Malformed ATM device name \'%s\', "
                 "PPPoA:if:vpi.vci expected\n", p->name.full);
      return NULL;
    }

    dev = atm_CreateDevice(p, iface, vpi, vci);
  }

  if (dev) {
    memcpy(&dev->dev, &baseatmdevice, sizeof dev->dev);
    physical_SetupStack(p, dev->dev.name, PHYSICAL_FORCE_SYNCNOACF);
    if (p->cfg.cd.necessity != CD_DEFAULT)
      log_Printf(LogWARN, "Carrier settings ignored\n");
    return &dev->dev;
  }

  return NULL;
}
