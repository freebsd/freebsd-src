#ifndef __PSPAT_H__
#define __PSPAT_H__

#include "mailbox.h"

#include <sys/types.h>

struct pspat_dispatcher {
	struct pspat_mailbox  *mb;
};

/*
 * Runs the dispatch loop
 * TODO What return codes?
 */
int pspat_dispatcher_run(struct pspat_dispatcher *d);

/*
 * Shuts down the dispatcher
 */
void pspat_dispatcher_shutdown(struct pspat_dispatcher *d);

#endif /* !__PSPAT_H__
