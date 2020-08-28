/* $FreeBSD$ */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <net/netmap_user.h>
#include <pthread.h>
#include "libnetmap.h"

struct nmctx_pthread {
	struct nmctx up;
	pthread_mutex_t mutex;
};

static struct nmctx_pthread nmctx_pthreadsafe;

static void
nmctx_pthread_lock(struct nmctx *ctx, int lock)
{
	struct nmctx_pthread *ctxp =
		(struct nmctx_pthread *)ctx;
	if (lock) {
		pthread_mutex_lock(&ctxp->mutex);
	} else {
		pthread_mutex_unlock(&ctxp->mutex);
	}
}

void __attribute__ ((constructor))
nmctx_set_threadsafe(void)
{
	struct nmctx *old;

	pthread_mutex_init(&nmctx_pthreadsafe.mutex, NULL);
	old = nmctx_set_default(&nmctx_pthreadsafe.up);
	nmctx_pthreadsafe.up = *old;
	nmctx_pthreadsafe.up.lock = nmctx_pthread_lock;
}

int nmctx_threadsafe;
