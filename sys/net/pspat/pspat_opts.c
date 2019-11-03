#include "pspat.h"
#include "pspat_system.h"
#include "pspat_opts.h"

#include <sys/kthread.h>
#include <sys/smp.h>

/*
 * Settings for the PSPAT subsystem
 */
bool		    pspat_enable __read_mostly = false;
bool		    pspat_debug_xmit __read_mostly = false;
int				pspat_xmit_mode __read_mostly = PSPAT_XMIT_MODE_ARB;
unsigned long	pspat_rate __read_mostly = 40000000000; /* 40GB/s */
unsigned long	pspat_arb_interval_ns __read_mostly = 1000;
unsigned long	pspat_arb_backpressure_drop = 0;
unsigned long	pspat_arb_dispatch_drop = 0;
unsigned long	pspat_dispatch_deq = 0;
unsigned long 	pspat_arb_loop_avg_ns = 0;
unsigned long 	pspat_arb_loop_max_ns = 0;
unsigned long 	pspat_arb_loop_avg_reqs = 0;
unsigned long 	pspat_mailbox_entries = 512;
unsigned long 	pspat_mailbox_line_size = 128;
unsigned long	*pspat_rounds; /* Currently unused */
uint32_t		pspat_arb_batch __read_mostly = 512;
uint32_t		pspat_dispatch_batch __read_mostly = 256;
/*
 * Data collection information
 */
struct pspat_stats     *pspat_stats;

void *pspat_cpu_names = NULL;

int (*orig_oid_handler)(SYSCTL_HANDLER_ARGS);

static struct sysctl_ctx_list clist;

SYSCTL_DECL(_net);

/*
 * TODO Figure out how sysctl oid works and actually document these functions
 */
static int pspat_enable_oid_handler(struct sysctl_oid *oidp, void *arg1, intmax_t arg2, struct sysctl_req *req);

/*
 * TODO Figure out how sysctl oid works and actually document these functions
 */
static int pspat_xmit_mode_oid_handler(struct sysctl_oid *oidp, void *arg1, intmax_t arg2, struct sysctl_req *req);

static int
pspat_enable_oid_handler(struct sysctl_oid *oidp, void *arg1, intmax_t arg2, struct sysctl_req *req) {
	int ret = orig_oid_handler(oidp, arg1, arg2, req);

	if(ret || !pspat_enable || pspat_info == NULL) {
		return ret;
	}

	kthread_resume(pspat->arb_thread);
	kthread_resume(pspat->dispatcher_thread);
	return 0;
}

static int
pspat_xmit_mode_oid_handler(struct sysctl_oid *oidp, void *arg1, intmax_t arg2, struct sysctl_req *req) {
	int ret = orig_oid_handler(oidp, arg1, arg2, req);

	if(ret || !pspat_enable || pspat_info == NULL) {
		return ret;
	}

	kthread_resume(pspat->dispatcher_thread);
	return 0;
}

int
pspat_sysctl_init(void)
{
	int cpus, i, n;
	char *name;
	size_t size;

	struct sysctl_oid *pspat_oid;
	struct sysctl_oid *pspat_cpu_oid;
	struct sysctl_oid *oidp, *t;

	cpus = mp_ncpus;

	size = (cpus + 1) * sizeof(unsigned long);
	pspat_rounds = malloc(size, M_PSPAT, M_WAITOK | M_ZERO);

	if(pspat_rounds == NULL) {
		printf("PSPAT is unable to allocate rounds counter array\n");
		return -ENOMEM;
	}

	sysctl_ctx_init(&clist);

	pspat_oid = SYSCTL_ADD_NODE(&clist, SYSCTL_STATIC_CHILDREN(_net), OID_AUTO, "pspat", CTLFLAG_RD, 0, "Pspat under net");

	pspat_cpu_oid = SYSCTL_ADD_NODE(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "cpu", CTLFLAG_RD, 0, "cpu under pspat");

	oidp = SYSCTL_ADD_BOOL(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "enable",
			       CTLFLAG_RW, &pspat_enable, false, "enable under pspat");

	orig_oid_handler = oidp->oid_handler;
	oidp->oid_handler = &pspat_enable_oid_handler;

	oidp = SYSCTL_ADD_INT(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "xmit_mode",
			      CTLFLAG_RW, &pspat_xmit_mode, PSPAT_XMIT_MODE_ARB, "xmit mode under pspat");
	oidp->oid_handler = &pspat_xmit_mode_oid_handler;

	oidp = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "rounds", CTLFLAG_RD,
			      pspat_rounds, 0, "Rounds under PSPAT");

	oidp = SYSCTL_ADD_BOOL(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "debug xmit", CTLFLAG_RW,
			       &pspat_debug_xmit, false, "debug_xmit under pspat");

	oidp = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "arb_interval_ns", CTLFLAG_RW,
			      &pspat_arb_interval_ns, pspat_arb_interval_ns, "arb_interval_ns under pspat");

	oidp = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "rate", CTLFLAG_RW,
			      &pspat_rate, pspat_rate, "rate under pspat");

	oidp = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "arb_backpressure_drop", CTLFLAG_RD,
			      &pspat_arb_backpressure_drop, pspat_arb_backpressure_drop, "arb_backpressure_drop under pspat");

	oidp = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "arb_dispatch_drop", CTLFLAG_RD,
			      &pspat_arb_dispatch_drop, pspat_arb_dispatch_drop, "arb_dispatch_drop under pspat");

	oidp = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "dispatch_deq", CTLFLAG_RD,
			      &pspat_dispatch_deq, pspat_dispatch_deq, "dispatch_deq under pspat");

	oidp = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "arb_loop_avg_ns", CTLFLAG_RD,
			      &pspat_arb_loop_avg_ns, pspat_arb_loop_avg_ns, "arb_loop_avg_ns under pspat");

	oidp = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "arb_loop_max_ns", CTLFLAG_RD,
			      &pspat_arb_loop_max_ns, pspat_arb_loop_max_ns, "arb_loop_max_ns under pspat");

	oidp = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "arb_loop_avg_reqs", CTLFLAG_RD,
			      &pspat_arb_loop_avg_reqs, pspat_arb_loop_avg_reqs, "arb_loop_avg_reqs under pspat");

	oidp = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "mailbox_entries", CTLFLAG_RW,
			      &pspat_mailbox_entries, pspat_mailbox_entries, "mailbox_entrie sunder pspat");

	oidp = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_oid), OID_AUTO, "mailbox_line_size", CTLFLAG_RW,
			      &pspat_mailbox_line_size, pspat_mailbox_line_size, "mailbox_line_size under pspat");

	size = cpus * 16; /* Space for the sysctl names */

	pspat_cpu_names = malloc(size, M_PSPAT, M_WAITOK);
	if (pspat_cpu_names == NULL) {
		printf("PSPAT did not have enough space for per-cpu sysctl names");
		sysctl_ctx_free(&clist);
		free(pspat_rounds, M_PSPAT);
		return -ENOMEM;
	}

	name = pspat_cpu_names;

	for(i = 0; i < cpus; i++) {
		n = snprintf(name, 16, "inq-drop-%d", i);

		t = SYSCTL_ADD_U64(&clist, SYSCTL_CHILDREN(pspat_cpu_oid), OID_AUTO, name, CTLFLAG_RW,
				   &pspat_stats[i].inq_drop, 0, name);

		name += n + 1;
	}

	return 0;
}


void
pspat_sysctl_fini(void) {
	sysctl_ctx_free(&clist);
	if (pspat_cpu_names != NULL) {
		free(pspat_cpu_names, M_PSPAT);
	}

	if(pspat_rounds != NULL) {
		free(pspat_rounds, M_PSPAT);
	}
}
