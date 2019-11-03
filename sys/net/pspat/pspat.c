#include "pspat.h"
#include "mailbox.h"

#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/types.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>
#include <sys/smp.h>
#include <sys/module.h>
#include <sys/malloc.h>

/*
 * GLOBAL VARIABLE DEFINITIONS
 */

/* Global struct containing all of the information about the data structure */
struct pspat_system *pspat;
static struct pspat_system *pspat_ptr; /* For internal usage only */

/* Read-write lock for `pspat_info` */
struct rwlock pspat_rwlock;

/* Internal global lock */
static struct mtx pspat_glock;

/* Should the arbiter thread stop? */
static bool arb_thread_stop __read_mostly = false;

/* Should the dispatcher thread stop? */
static bool dispatcher_thread_stop __read_mostly = false;

/*
 * Data collection information
 */
struct pspat_stats     *pspat_stats;
unsigned long	      pspat_arb_loop_avg_ns;
unsigned long	      pspat_arb_loop_max_ns;
unsigned long	      pspat_arb_loop_avg_reqs;

#define DIV_ROUND_UP(n, d)  (((n) + (d) - 1 ) / (d))

MALLOC_DEFINE(M_PSPAT, "pspat", "PSPAT Networking Subsystem");

/*
 * Kernel thread running the arbiter
 *
 * data - a generic pointer representing the arbiter to run as a worker function
 */
static void arbiter_worker_func(void *data);

/*
 * Kernel thread running a dispatcher
 *
 * data - a generic pointer representing the dispatcher to run
 */
static void dispatcher_worker_func(void *data);

/*
 * Removes the internal state of the PSPAT subsystem, stopping any
 * kernel threads running on behalf of the PSPAT subsystem
 */
static int pspat_destroy(void);

/*
 * Sets up the internal state of the PSPAT subsystem
 * returns 0 on success, or a negative error code on failure
 */
static int pspat_create(void);

/*
 * Sets up the PSPAT subsystem, including system control and the internal
 * state.
 * Returns 0 on success or a negative error code on failure
 */
static int pspat_init(void);

/*
 * Tears down the PSPAT subsystem, including system control and the internal
 * state.
 * Returns 0 on success or a negative error code on failure
 */
static void pspat_fini(void);


static void
arbiter_worker_func(void *data) {
	struct pspat_arbiter *arb = (struct pspat_arbiter *)data;
	struct timespec ts;

	bool arb_registered = false;

	while (!arb_thread_stop) {
		if (!pspat_enable) {
			if (arb_registered) {
				/* PSPAT is disabled but arbiter is still
				 * registered, we need to unregister */
				mtx_lock(&pspat_glock);
				pspat_arbiter_shutdown(arb);
				rw_wlock(&pspat_rwlock);
				pspat = NULL;
				mtx_unlock(&pspat_glock);
				rwlock_unlock(&pspat_rwlock);

				arb_registered = false;
				printf("PSPAT Arbiter unregistered\n");
			}

			kthread_suspend(curthread, 0);
		} else {
			if (!arb_registered) {
				/* PSPAT is enabled but arbiter is not
				 * registered, we need to register */

				mtx_lock(&pspat_glock);
				rw_wlock(&pspat_rwlock);
				pspat = pspat_ptr;
				rw_wunlock(&pspat_rwlock);
				mtx_unlock(&pspat_glock);

				arb_registered = true;
				printf("PSPAT Arbiter is registered!\n");
				nanotime(&ts);
				arb->last_ts = ts.tv_nsec << 10;
				arb->num_loops = 0;
				arb->num_picos = 0;
				arb->max_picos = 0;
			}

			pspat_arbiter_run(arb);
		}
	}

	kthread_exit();
}

static void
dispatcher_worker_func(void *data)
{
	struct pspat_dispatcher *d = (struct pspat_dispatcher *)data;

	while(!dispatcher_thread_stop) {
		if (pspat_xmit_mode != PSPAT_XMIT_MODE_DISPATCH || !pspat_enable) {
			printf("PSPAT Dispatcher deactivated!\n");
			pspat_dispatcher_shutdown(d);
			ktrhead_suspend(curthread, 0);
			printf("PSPAT Dispatcher activated!\n");
		} else {
			pspat_dispatcher_run(d);
		}
	}

	kthread_exit();
}

static int
pspat_create(void)
{
	int cpus, i, dispatchers, ret;
	unsigned long mb_entries, mb_line_size;
	size_t mb_size, arb_size;
	struct pspat_mailbox *m;

	cpus = mp_ncpus;
	dispatchers = 1;
	mb_entries = pspat_mailbox_entries;
	mb_line_size = pspat_mailbox_line_size;
	mb_size = pspat_mb_size(mb_entries);

	mtx_lock(&pspat_glock);

	arb_size = roundup(sizeof(struct pspat) + cpus * sizeof(struct pspat_queue), CACHE_LINE_SIZE);

	pspat_pages = roundup(arb_size + mb_size * (cpus + dispatchers), PAGE_SIZE);

	pspat_ptr = malloc(pspat_pages, M_PSPAT, M_WAITOK | M_ZERO);
	if (pspat_ptr == NULL) {
		ret = -ENOMEM;
		goto unlock;
	}

	pspat_ptr->arbiter.n_queues = cpus;

	/* Initialize all mailboxes */
	m = (struct pspat_mailbox *)((char *)arbp + arb_size);

	for (int i = 0; i < cpus; i++) {
		char name[PSPAT_MB_NAMSZ];
		snprintf(name, PSPAT_MB_NAMSZ, "CL-%d", i);

		ret = pspat_mb_init(m, name, mb_entries, mb_line_size);

		if (ret) {
			goto free_pspat;
		}

		arbp->arbiter.queues[i].inq = m;
		TAILQ_INIT(&arbp->arbiter.queues[i].mb_to_clear);
		m = (struct pspat_mailbox *) ((char *)m + mb_size);
	}

	TAILQ_INIT(&arbp->mb_to_delete);

	/* Initialize Dispatchers */

	for (i = 0; i < dispatchers; i++) {
		char name[PSPAT_MB_NAMSZ];
		snprintf(name, PSPAT_MB_NAMSZ, "T-%d", i);

		ret = pspat_mb_init(m, name, mb_entries, mb_line_size);

		if (ret) {
			goto free_pspat;
		}

		arbp->dispatchers[i].mb = m;
		m = (struct pspat_mailbox *) ((char *)m + mb_size);
	}

	arbp->arbiter.fs = NULL;

	ret = kthread_add(arb_worker_func, arbp, NULL, &arbp->arb_thread, 0, 0, "pspat_arbiter_thread");
	if (ret) {
		goto fail;
	}

	ret = kthread_add(dispatcher_worker_func, &arbp->dispatchers[0], NULL, &arbp->dispatcher_thread, 0, 0, "PSPAT_dispatcher_thread");
	if (ret) {
		goto fail;
	}

	printf("PSPAT Arbiter created with %d per-core queues\n", arbp->n_queues);
	mtx_unlock(&pspat_glock);
	return 0;
stop_arbiter:
	arb_thread_stop = true;
	pspat_ptr->arb_thread = NULL;

free_pspat:
	free(pspat_ptr, M_PSPAT);

unlock:
	mtx_unlock(&pspat_glock);
	return ret;
}

static int
pspat_init(void)
{
	int ret;

	mtx_init(&pspat_glock, "pspat_glock", NULL, MTX_DEF);
	rw_init(&pspat_rwlock, "pspat_rwlock");

	ret = pspat_sysctl_init();
	if (ret) {
		printf("PSPAT Sysctl Init failed\n");
		return ret;
	}

	ret = pspat_create();
	if (ret) {
		printf("Failed to create arbiter\n");
		pspat_sysctl_fini();
		return ret;
	}

	return 0;
}

static int
pspat_destroy(void)
{
	mtx_lock(&pspat_glock);

	rw_wlock(&pspat_rwlock);
	pspat = NULL;
	rw_wunlock(&pspat_rwlock);

	if (pspat_ptr->arb_thread != NULL) {
		arb_thread_stop = true;
		pspat_ptr->arb_thread = NULL;
	}

	if (pspat_ptr->dispatcher_thread != NULL) {
		dispatcher_thread_stop = true;
		pspat_ptr->dispatcher_thread = NULL;
	}

	pspat_dispatcher_shutdown(&pspat_ptr->dispatchers[0]);
	pspat_arb_shutdown(&pspat_ptr->arbiter);
	free(pspat_ptr, M_PSPAT);
	pspat_ptr = NULL;

	printf("PSPAT has been destroyed!");
	mtx_unlock(&pspat_glock);

	return 0;
}

static int
pspat_fini(void)
{
	pspat_destroy();
	pspat_sysctl_fini();
	rw_destroy(&pspat_rwlock);
	mtx_destroy(&pspat_glock);
}

static int pspat_module_handler(struct module *module, int event, void *arg) {
	int err = 0;

	switch (event) {
	    case MPD_LOAD:
		ret = pspat_init();
		break;
	    case MOD_UNLOAD:
		pspat_fini();
		break;
	    default:
		err = EOPNOTSUPP;
		break;
	}

	return err;
}

static moduledata_t pspat_data = {
	"pspat",
	pspat_module_handler,
	NULL
};

DECLARE_MODULE(pspat, pspat_data, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(pspat, 1);

/* Called on thread_exit() to clean-up PSPAT mailbox, if any. */
void
exit_pspat(void)
{
	struct pspat_arbiter *arb;
	struct pspat_queue *pq;
	int cpu;

	if (curthread->pspat_mb == NULL)
		return;

	curthread->pspat_mb->dead = 1;

retry:
	rw_wlock(&pspat_rwlock);
	arb = &(pspat->arbiter);
	if (arb) {
		/* If the arbiter is running, we cannot delete the mailbox
		 * by ourselves. Instead, we set the "dead" flag and insert
		 * the mailbox in the client list.
		 */
		cpu = curthread->td_oncpu;
		pq = arb->queues + cpu;
		if (pspat_mb_insert(pq->inq, curthread->pspat_mb) == 0) {
			curthread->pspat_mb = NULL;
		}
	}
	rw_wunlock(&pspat_rwlock);
	if (curthread->pspat_mb) {
		/* the mailbox is still there */
		if (arb) {
			/* We failed to push PSPAT_LAST_SKB but the
			 * arbiter was running. We must try again.
			 */
			printf("PSPAT Try again to destroy mailbox\n");
			pause("Wait before retrying", 100);
			goto retry;
		} else {
			/* The arbiter is not running. Since
			 * pspat_shutdown() drains everything, any
			 * new arbiter will not see this mailbox.
			 * Therefore, we can safely free it up.
			 */
			pspat_mb_delete(curthread->pspat_mb);
			curthread->pspat_mb = NULL;
		}
	}
}
