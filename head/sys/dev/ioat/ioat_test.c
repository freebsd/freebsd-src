/*-
 * Copyright (C) 2012 Intel Corporation
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include "ioat.h"
#include "ioat_hw.h"
#include "ioat_internal.h"
#include "ioat_test.h"

MALLOC_DEFINE(M_IOAT_TEST, "ioat_test", "ioat test allocations");

#define	IOAT_TEST_SIZE	0x40000
#define	IOAT_MAX_BUFS	8

struct test_transaction {
	uint8_t			num_buffers;
	void			*buf[IOAT_MAX_BUFS];
	uint32_t		length;
	struct ioat_test	*test;
};

static int g_thread_index = 1;
static struct cdev *g_ioat_cdev = NULL;

static void
ioat_test_transaction_destroy(struct test_transaction *tx)
{
	int i;

	for (i = 0; i < IOAT_MAX_BUFS; i++) {
		if (tx->buf[i] != NULL) {
			contigfree(tx->buf[i], IOAT_TEST_SIZE, M_IOAT_TEST);
			tx->buf[i] = NULL;
		}
	}

	free(tx, M_IOAT_TEST);
}

static struct
test_transaction *ioat_test_transaction_create(uint8_t num_buffers,
    uint32_t buffer_size)
{
	struct test_transaction *tx;
	int i;

	tx = malloc(sizeof(struct test_transaction), M_IOAT_TEST, M_NOWAIT | M_ZERO);
	if (tx == NULL)
		return (NULL);

	tx->num_buffers = num_buffers;
	tx->length = buffer_size;

	for (i = 0; i < num_buffers; i++) {
		tx->buf[i] = contigmalloc(buffer_size, M_IOAT_TEST, M_NOWAIT,
		    0, BUS_SPACE_MAXADDR, PAGE_SIZE, 0);

		if (tx->buf[i] == NULL) {
			ioat_test_transaction_destroy(tx);
			return (NULL);
		}
	}
	return (tx);
}

static void
ioat_dma_test_callback(void *arg)
{
	struct test_transaction *tx;
	struct ioat_test *test;

	tx = arg;
	test = tx->test;

	if (memcmp(tx->buf[0], tx->buf[1], tx->length) != 0) {
		ioat_log_message(0, "miscompare found\n");
		test->status = IOAT_TEST_MISCOMPARE;
	}
	atomic_add_32(&test->num_completions, 1);
	ioat_test_transaction_destroy(tx);
	if (test->num_completions == test->num_loops)
		wakeup(test);
}

static void
ioat_dma_test(void *arg)
{
	struct test_transaction *tx;
	struct ioat_test *test;
	bus_dmaengine_t dmaengine;
	uint32_t loops;
	int index, i;

	test = arg;
	loops = test->num_loops;

	test->status = IOAT_TEST_OK;
	test->num_completions = 0;

	index = g_thread_index++;
	dmaengine = ioat_get_dmaengine(test->channel_index);

	if (dmaengine == NULL) {
		ioat_log_message(0, "Couldn't acquire dmaengine\n");
		test->status = IOAT_TEST_NO_DMA_ENGINE;
		return;
	}

	ioat_log_message(0, "Thread %d: num_loops remaining: 0x%07x\n", index,
	    test->num_loops);

	for (loops = 0; loops < test->num_loops; loops++) {
		bus_addr_t src, dest;

		if (loops % 0x10000 == 0) {
			ioat_log_message(0, "Thread %d: "
			    "num_loops remaining: 0x%07x\n", index,
			    test->num_loops - loops);
		}

		tx = ioat_test_transaction_create(2, IOAT_TEST_SIZE);
		if (tx == NULL) {
			ioat_log_message(0, "tx == NULL - memory exhausted\n");
			atomic_add_32(&test->num_completions, 1);
			test->status = IOAT_TEST_NO_MEMORY;
			continue;
		}

		tx->test = test;
		wmb();

		/* fill in source buffer */
		for (i = 0; i < (IOAT_TEST_SIZE / sizeof(uint32_t)); i++) {
			uint32_t val = i + (loops << 16) + (index << 28);
			((uint32_t *)tx->buf[0])[i] = ~val;
			((uint32_t *)tx->buf[1])[i] = val;
		}

		src = pmap_kextract((vm_offset_t)tx->buf[0]);
		dest = pmap_kextract((vm_offset_t)tx->buf[1]);

		ioat_acquire(dmaengine);
		ioat_copy(dmaengine, src, dest, IOAT_TEST_SIZE,
		    ioat_dma_test_callback, tx, DMA_INT_EN);
		ioat_release(dmaengine);
	}

	while (test->num_completions < test->num_loops)
		tsleep(test, 0, "compl", 5 * hz);

}

static int
ioat_test_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{

	return (0);
}

static int
ioat_test_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{

	return (0);
}

static int
ioat_test_ioctl(struct cdev *dev, unsigned long cmd, caddr_t arg, int flag,
    struct thread *td)
{

	switch (cmd) {
	case IOAT_DMATEST:
		ioat_dma_test(arg);
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static struct cdevsw ioat_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_open =	ioat_test_open,
	.d_close =	ioat_test_close,
	.d_ioctl =	ioat_test_ioctl,
	.d_name =	"ioat_test",
};

static int
sysctl_enable_ioat_test(SYSCTL_HANDLER_ARGS)
{
	int error, enabled;

	enabled = (g_ioat_cdev != NULL);
	error = sysctl_handle_int(oidp, &enabled, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (enabled != 0 && g_ioat_cdev == NULL) {
		g_ioat_cdev = make_dev(&ioat_cdevsw, 0, UID_ROOT, GID_WHEEL,
		    0600, "ioat_test");
	} else if (enabled == 0 && g_ioat_cdev != NULL) {
		destroy_dev(g_ioat_cdev);
		g_ioat_cdev = NULL;
	}
	return (0);
}
SYSCTL_PROC(_hw_ioat, OID_AUTO, enable_ioat_test, CTLTYPE_INT | CTLFLAG_RW,
    0, 0, sysctl_enable_ioat_test, "I",
    "Non-zero: Enable the /dev/ioat_test device");
