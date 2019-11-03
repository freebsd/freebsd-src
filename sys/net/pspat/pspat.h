#ifndef __PSPAT_H__
#define __PSPAT_H__

#include "mailbox.h"
#include "pspat_arbiter.h"
#include "pspat_dispatcher.h"
#include "pspat_system.h"
#include "pspat_opts.h"

#include <sys/types.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

MALLOC_DECLARE(M_PSPAT);

/*
 * Exits the PSPAT subsystem
 */
void exit_pspat(void);

/*
 * Creates the queue for the client
 * @returns 0 on success or -ENOMEM if there is not enough memory to allocate a
 * client queue
 */
int pspat_create_client_queue(void);


/*
 * Sends the mbuf to the arbiter with the given arguments
 *
 * @mbuf the buffer to send to the arbiter
 * @fwa the IP / scheduling arguments
 * @returns 0 on success or a negative error code on failure
 */
int pspat_client_handler(struct mbuf *mbuf, struct ip_fw_args *fwa);

#endif /* !__PSPAT_H__ */
