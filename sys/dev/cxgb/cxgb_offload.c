/**************************************************************************

Copyright (c) 2007-2008, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.


***************************************************************************/


#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/cxgb/cxgb_offload.c,v 1.8.2.3.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/pciio.h>
#include <sys/conf.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus_dma.h>
#include <sys/rman.h>
#include <sys/ioccom.h>
#include <sys/mbuf.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/proc.h>

#ifdef CONFIG_DEFINED
#include <cxgb_include.h>
#else
#include <dev/cxgb/cxgb_include.h>
#endif

#include <net/route.h>

#define VALIDATE_TID 0
MALLOC_DEFINE(M_CXGB, "cxgb", "Chelsio 10 Gigabit Ethernet and services");

TAILQ_HEAD(, cxgb_client) client_list;
TAILQ_HEAD(, t3cdev) ofld_dev_list;


static struct mtx cxgb_db_lock;


static int inited = 0;

static inline int
offload_activated(struct t3cdev *tdev)
{
	struct adapter *adapter = tdev2adap(tdev);
	
	return (isset(&adapter->open_device_map, OFFLOAD_DEVMAP_BIT));
}

static inline void
register_tdev(struct t3cdev *tdev)
{
	static int unit;

	mtx_lock(&cxgb_db_lock);
	snprintf(tdev->name, sizeof(tdev->name), "ofld_dev%d", unit++);
	TAILQ_INSERT_TAIL(&ofld_dev_list, tdev, entry);
	mtx_unlock(&cxgb_db_lock);
}

static inline void
unregister_tdev(struct t3cdev *tdev)
{
	mtx_lock(&cxgb_db_lock);
	TAILQ_REMOVE(&ofld_dev_list, tdev, entry);
	mtx_unlock(&cxgb_db_lock);	
}

#ifndef TCP_OFFLOAD_DISABLE
/**
 *	cxgb_register_client - register an offload client
 *	@client: the client
 *
 *	Add the client to the client list,
 *	and call backs the client for each activated offload device
 */
void
cxgb_register_client(struct cxgb_client *client)
{
	struct t3cdev *tdev;

	mtx_lock(&cxgb_db_lock);
	TAILQ_INSERT_TAIL(&client_list, client, client_entry);

	if (client->add) {
		TAILQ_FOREACH(tdev, &ofld_dev_list, entry) {
			if (offload_activated(tdev)) {
				client->add(tdev);
			} else
				CTR1(KTR_CXGB,
				    "cxgb_register_client: %p not activated", tdev);
			
		}
	}
	mtx_unlock(&cxgb_db_lock);
}

/**
 *	cxgb_unregister_client - unregister an offload client
 *	@client: the client
 *
 *	Remove the client to the client list,
 *	and call backs the client for each activated offload device.
 */
void
cxgb_unregister_client(struct cxgb_client *client)
{
	struct t3cdev *tdev;

	mtx_lock(&cxgb_db_lock);
	TAILQ_REMOVE(&client_list, client, client_entry);

	if (client->remove) {
		TAILQ_FOREACH(tdev, &ofld_dev_list, entry) {
			if (offload_activated(tdev))
				client->remove(tdev);
		}
	}
	mtx_unlock(&cxgb_db_lock);
}

/**
 *	cxgb_add_clients - activate register clients for an offload device
 *	@tdev: the offload device
 *
 *	Call backs all registered clients once a offload device is activated 
 */
void
cxgb_add_clients(struct t3cdev *tdev)
{
	struct cxgb_client *client;

	mtx_lock(&cxgb_db_lock);
	TAILQ_FOREACH(client, &client_list, client_entry) {
		if (client->add)
			client->add(tdev);
	}
	mtx_unlock(&cxgb_db_lock);
}

/**
 *	cxgb_remove_clients - activate register clients for an offload device
 *	@tdev: the offload device
 *
 *	Call backs all registered clients once a offload device is deactivated 
 */
void
cxgb_remove_clients(struct t3cdev *tdev)
{
	struct cxgb_client *client;

	mtx_lock(&cxgb_db_lock);
	TAILQ_FOREACH(client, &client_list, client_entry) {
		if (client->remove)
			client->remove(tdev);
	}
	mtx_unlock(&cxgb_db_lock);
}
#endif

/**
 * cxgb_ofld_recv - process n received offload packets
 * @dev: the offload device
 * @m: an array of offload packets
 * @n: the number of offload packets
 *
 * Process an array of ingress offload packets.  Each packet is forwarded
 * to any active network taps and then passed to the offload device's receive
 * method.  We optimize passing packets to the receive method by passing
 * it the whole array at once except when there are active taps.
 */
int
cxgb_ofld_recv(struct t3cdev *dev, struct mbuf **m, int n)
{

	return dev->recv(dev, m, n);
}

/*
 * Dummy handler for Rx offload packets in case we get an offload packet before
 * proper processing is setup.  This complains and drops the packet as it isn't
 * normal to get offload packets at this stage.
 */
static int
rx_offload_blackhole(struct t3cdev *dev, struct mbuf **m, int n)
{
	while (n--)
		m_freem(m[n]);
	return 0;
}

static void
dummy_neigh_update(struct t3cdev *dev, struct rtentry *neigh, uint8_t *enaddr,
    struct sockaddr *sa)
{
}

void
cxgb_set_dummy_ops(struct t3cdev *dev)
{
	dev->recv         = rx_offload_blackhole;
	dev->arp_update = dummy_neigh_update;
}

static int
do_smt_write_rpl(struct t3cdev *dev, struct mbuf *m)
{
	struct cpl_smt_write_rpl *rpl = cplhdr(m);

	if (rpl->status != CPL_ERR_NONE)
		log(LOG_ERR,
		       "Unexpected SMT_WRITE_RPL status %u for entry %u\n",
		       rpl->status, GET_TID(rpl));

	return CPL_RET_BUF_DONE;
}

static int
do_l2t_write_rpl(struct t3cdev *dev, struct mbuf *m)
{
	struct cpl_l2t_write_rpl *rpl = cplhdr(m);

	if (rpl->status != CPL_ERR_NONE)
		log(LOG_ERR,
		       "Unexpected L2T_WRITE_RPL status %u for entry %u\n",
		       rpl->status, GET_TID(rpl));

	return CPL_RET_BUF_DONE;
}

static int
do_rte_write_rpl(struct t3cdev *dev, struct mbuf *m)
{
	struct cpl_rte_write_rpl *rpl = cplhdr(m);

	if (rpl->status != CPL_ERR_NONE)
		log(LOG_ERR,
		       "Unexpected L2T_WRITE_RPL status %u for entry %u\n",
		       rpl->status, GET_TID(rpl));

	return CPL_RET_BUF_DONE;
}

static int
do_set_tcb_rpl(struct t3cdev *dev, struct mbuf *m)
{
	struct cpl_set_tcb_rpl *rpl = cplhdr(m);

	if (rpl->status != CPL_ERR_NONE)
		log(LOG_ERR,
		    "Unexpected SET_TCB_RPL status %u for tid %u\n",
			rpl->status, GET_TID(rpl));
	return CPL_RET_BUF_DONE;
}

static int
do_trace(struct t3cdev *dev, struct mbuf *m)
{
#if 0
	struct cpl_trace_pkt *p = cplhdr(m);


	skb->protocol = 0xffff;
	skb->dev = dev->lldev;
	skb_pull(skb, sizeof(*p));
	skb->mac.raw = mtod(m, (char *));
	netif_receive_skb(skb);
#endif	
	return 0;
}

/*
 * Process a received packet with an unknown/unexpected CPL opcode.
 */
static int
do_bad_cpl(struct t3cdev *dev, struct mbuf *m)
{
	log(LOG_ERR, "%s: received bad CPL command 0x%x\n", dev->name,
	    0xFF & *mtod(m, uint32_t *));
	return (CPL_RET_BUF_DONE | CPL_RET_BAD_MSG);
}

/*
 * Handlers for each CPL opcode
 */
static cpl_handler_func cpl_handlers[256];

/*
 * T3CDEV's receive method.
 */
int
process_rx(struct t3cdev *dev, struct mbuf **m, int n)
{
	while (n--) {
		struct mbuf *m0 = *m++;
		unsigned int opcode = G_OPCODE(ntohl(m0->m_pkthdr.csum_data));
		int ret;

		DPRINTF("processing op=0x%x m=%p data=%p\n", opcode, m0, m0->m_data);
		
		ret = cpl_handlers[opcode] (dev, m0);

#if VALIDATE_TID
		if (ret & CPL_RET_UNKNOWN_TID) {
			union opcode_tid *p = cplhdr(m0);

			log(LOG_ERR, "%s: CPL message (opcode %u) had "
			       "unknown TID %u\n", dev->name, opcode,
			       G_TID(ntohl(p->opcode_tid)));
		}
#endif
		if (ret & CPL_RET_BUF_DONE)
			m_freem(m0);
	}
	return 0;
}

/*
 * Add a new handler to the CPL dispatch table.  A NULL handler may be supplied
 * to unregister an existing handler.
 */
void
t3_register_cpl_handler(unsigned int opcode, cpl_handler_func h)
{
	if (opcode < NUM_CPL_CMDS)
		cpl_handlers[opcode] = h ? h : do_bad_cpl;
	else
		log(LOG_ERR, "T3C: handler registration for "
		       "opcode %x failed\n", opcode);
}

/*
 * Allocate a chunk of memory using kmalloc or, if that fails, vmalloc.
 * The allocated memory is cleared.
 */
void *
cxgb_alloc_mem(unsigned long size)
{

	return malloc(size, M_CXGB, M_ZERO|M_NOWAIT);
}

/*
 * Free memory allocated through t3_alloc_mem().
 */
void
cxgb_free_mem(void *addr)
{
	free(addr, M_CXGB);
}

static __inline int
adap2type(struct adapter *adapter) 
{ 
        int type = 0; 
 
        switch (adapter->params.rev) { 
        case T3_REV_A: 
                type = T3A; 
                break; 
        case T3_REV_B: 
        case T3_REV_B2: 
                type = T3B; 
                break; 
        case T3_REV_C: 
                type = T3C; 
                break; 
        } 
        return type; 
}

void
cxgb_adapter_ofld(struct adapter *adapter)
{
	struct t3cdev *tdev = &adapter->tdev;

	cxgb_set_dummy_ops(tdev);
	tdev->type = adap2type(adapter);
	tdev->adapter = adapter;
	register_tdev(tdev);	

}

void
cxgb_adapter_unofld(struct adapter *adapter)
{
	struct t3cdev *tdev = &adapter->tdev;

	tdev->recv = NULL;
	tdev->arp_update = NULL;
	unregister_tdev(tdev);	
}

void
cxgb_offload_init(void)
{
	int i;

	if (inited++)
		return;
	
	mtx_init(&cxgb_db_lock, "ofld db", NULL, MTX_DEF);

	TAILQ_INIT(&client_list);
	TAILQ_INIT(&ofld_dev_list);
	
	for (i = 0; i < 0x100; ++i)
		cpl_handlers[i] = do_bad_cpl;
	
	t3_register_cpl_handler(CPL_SMT_WRITE_RPL, do_smt_write_rpl);
	t3_register_cpl_handler(CPL_RTE_WRITE_RPL, do_rte_write_rpl);
	t3_register_cpl_handler(CPL_L2T_WRITE_RPL, do_l2t_write_rpl);

	t3_register_cpl_handler(CPL_SET_TCB_RPL, do_set_tcb_rpl);
	t3_register_cpl_handler(CPL_TRACE_PKT, do_trace);
	
}

void 
cxgb_offload_exit(void)
{

	if (--inited)
		return;

	mtx_destroy(&cxgb_db_lock);
}

MODULE_VERSION(if_cxgb, 1);
