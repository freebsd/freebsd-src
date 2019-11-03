#ifndef __PSPAT_DISPATCHER_H__
#define __PSPAT_DISPATCHER_H__

#include "mailbox.h"

#include <sys/types.h>

struct pspat_dispatcher {
	struct pspat_mailbox  *mb;
};

/*
 * Runs a dispatcher one cycle.
 *
 * @d the dispatcher to run
 * @returns the number of buffers dispatched
 */
int pspat_dispatcher_run(struct pspat_dispatcher *d);

/*
 * Shuts down the dispatcher
 *
 * @b the dispatcher to run
 */
void pspat_dispatcher_shutdown(struct pspat_dispatcher *d);

#endif /* !__PSPAT_DISPATCHER_H__ */
