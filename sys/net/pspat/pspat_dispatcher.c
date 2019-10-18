#include "pspat_dispatcher.h"
#include "mailbox.h"

#include <sys/types.h>

/*
 * Runs the dispatch loop
 * TODO What return codes?
 */
int pspat_dispatcher_run(struct pspat_dispatcher *d) {
	struct pspat_mailbox *m = d->mb;
	struct mbuf *mbf = NULL;
	int ndeq = 0;

	while (ndeq < pspat_dispatch_batch && ((mbf = pspat_mb_extract(m)) != NULL)) {
		pspat_txqs_flush(mbf);
		ndeq ++;
	}

	pspat_dispatch_deq += ndeq;
	pspat_mb_clear(m);

	if(pspat_debug_xmit && ndeq) {
		printf("PSPAT Sender processed %d mbfs\n", ndeq);
	}

	return ndeq;
}

/*
 * Shuts down the dispatcher
 */
void pspat_dispatcher_shutdown(struct pspat_dispatcher *d) {
	struct mbuf *mbf;
	int n = 0;

	/* Drain the sender mailbox. */
	while ( (mbf = pspat_mb_extract(s->mb)) != NULL ) {
		m_free(mbf);
		n ++;
	}
	printf("%s: Sender MB drained, found %d mbfs\n", __func__, n);
}

#endif /* !__PSPAT_H__
