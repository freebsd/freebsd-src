#ifndef __PSPAT_H__
#define __PSPAT_H__

#include "mailbox.h"
#include "pspat_arbiter.h"
#include "pspat_dispatcher.h"
#include "pspat_system.h"
#include "pspat_opts.h"

#include <sys/types.h>
#include <sys/kthread.h>
#include <sys/rwlock.h>

MALLOC_DECLARE(M_PSPAT);

/*
 * Exits the PSPAT subsystem
 */
void exit_pspat(void);

/*
 * Creates the queue for the client
 * Returns 0 on success, or TODO What error codes can this return?
 */
int pspat_create_client_queue(void);


/*
 * TODO: Figure out specifics about this
 * Seems to just start up the process
 */
int pspat_client_handler(struct mbuf *mbuf, struct ip_fw_args *fwa);

#endif /* !__PSPAT_H__
