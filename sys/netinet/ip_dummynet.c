/*
 * Copyright (c) 1998 Luigi Rizzo
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 *	$Id: ip_dummynet.c,v 1.1.2.4 1999/05/04 18:24:50 luigi Exp $
 */

/*
 * This module implements IP dummynet, a bandwidth limiter/delay emulator
 * used in conjunction with the ipfw package.
 *
 * Changes:
 *
 * 980821: changed conventions in the queueing logic
 *	packets passed from dummynet to ip_in/out are prepended with
 *	a vestigial mbuf type MT_DUMMYNET which contains a pointer
 *	to the matching rule.
 *	ip_input/output will extract the parameters, free the vestigial mbuf,
 *	and do the processing.
 *     
 * 980519:	fixed behaviour when deleting rules.
 * 980518:	added splimp()/splx() to protect against races
 * 980513:	initial release
 */

/* include files marked with XXX are probably not needed */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>			/* XXX */
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>

#ifdef BRIDGE
#include <netinet/if_ether.h> /* for struct arpcom */
#include <net/bridge.h>
#endif

static struct dn_pipe *all_pipes = NULL ;	/* list of all pipes */

static int dn_debug = 0 ;			/* verbose */
static int dn_calls = 0 ;			/* number of calls */
static int dn_idle = 1;
#ifdef SYSCTL_NODE
SYSCTL_NODE(_net_inet_ip, OID_AUTO, dummynet, CTLFLAG_RW, 0, "Dummynet");
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, debug, CTLFLAG_RW, &dn_debug, 0, "");
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, calls, CTLFLAG_RD, &dn_calls, 0, "");
SYSCTL_INT(_net_inet_ip_dummynet, OID_AUTO, idle, CTLFLAG_RD, &dn_idle, 0, "");
#endif

static int ip_dn_ctl(int optname, struct mbuf **mm);

static void dummynet(void);
static void dn_restart(void);
static void dn_move(struct dn_pipe *pipe, int immediate);

/*
 * the following is needed when deleting a pipe, because rules can
 * hold references to the pipe.
 */
extern LIST_HEAD (ip_fw_head, ip_fw_chain) ip_fw_chain;

/*
 * invoked to reschedule the periodic task if necessary.
 * Should only be called when dn_idle = 1 ;
 */
static void
dn_restart()
{
    struct dn_pipe *pipe;

    if (!dn_idle)
	return;
	
    for (pipe = all_pipes ; pipe ; pipe = pipe->next ) {
	/* if there any pipe that needs work, restart */
	if (pipe->r.head || pipe->p.head || pipe->numbytes < 0 ) {
	    dn_idle = 0;
	    timeout(dummynet, (caddr_t)NULL, 1);
	    return ;
	}
    }
}

/*
 * move packets from R-queue to P-queue
 */
static void
dn_move(struct dn_pipe *pipe, int immediate)
{
    struct dn_pkt *tmp, *pkt;
 
    /*
     * consistency check, should catch new pipes which are
     * not initialized properly.
     */
    if ( pipe->p.head == NULL &&
		pipe->ticks_from_last_insert != pipe->delay) {
	printf("Warning, empty pipe and delay %d (should be %a)d\n",
		pipe->ticks_from_last_insert, pipe->delay);
	pipe->ticks_from_last_insert = pipe->delay;
    }
    /* this ought to go in dn_dequeue() */
    if (!immediate && pipe->ticks_from_last_insert < pipe->delay)
	pipe->ticks_from_last_insert++;
    if ( pkt = pipe->r.head ) {
	/*
	 * Move at most numbytes bytes from src and move to dst.
	 * delay is set to ticks_from_last_insert, which
	 * is reset after the first insertion;
	 */
	while ( pkt ) {
	    int len = pkt->dn_m->m_pkthdr.len ;

	    /*
	     * queue limitation: pass packets down if the len is
	     * such that the pkt would go out before the next tick.
	     */
	    if (pipe->bandwidth) {
		int len_scaled = len*8*hz ;
		/* numbytes is in bit/sec, scaled 8*hz ... */
		if (pipe->numbytes < len_scaled)
		    break;
		pipe->numbytes -= len_scaled;
	    }
	    pipe->r_len--; /* elements in queue */
	    pipe->r_len_bytes -= len ;

	    /*
	     * to add delay jitter, must act here. A lower value
	     * (bounded to 0) means lower delay.
	     */
	    pkt->delay = pipe->ticks_from_last_insert;
	    pipe->ticks_from_last_insert = 0;
	    /* compensate the decrement done next in dn_dequeue */
	    if (!immediate && pkt->delay >0 && pipe->p.head==NULL)
		pkt->delay++;
	    if (pipe->p.head == NULL)
		pipe->p.head = pkt;
	    else
		(struct dn_pkt *)pipe->p.tail->dn_next = pkt;
	    pipe->p.tail = pkt;
	    pkt = (struct dn_pkt *)pkt->dn_next;
	    pipe->p.tail->dn_next = NULL;
	}
	pipe->r.head = pkt;
 
	/*** XXX just a sanity check */
	if ( ( pkt == NULL && pipe->r_len != 0) ||
	     ( pkt != NULL && pipe->r_len == 0) )
	    printf("-- Warning, pipe head %x len %d\n",
		    pkt, pipe->r_len);
    }
 
    /*
     * deliver packets downstream after the delay in the P-queue.
     */

    if (pipe->p.head == NULL)
	return;
    if (!immediate)
	pipe->p.head->delay--;
    while ( (pkt = pipe->p.head) && pkt->delay < 1) {
	/*
	 * first unlink, then call procedures since ip_input()
	 * can result in a call to ip_output cnd viceversa,
	 * thus causing nested calls
	 */
	pipe->p.head = (struct dn_pkt *) pkt->dn_next ;

	/*
	 * the trick to avoid flow-id settings here is to prepend a
	 * vestigial mbuf to the packet, with the following values:
	 * m_type = MT_DUMMYNET
	 * m_next = the actual mbuf to be processed by ip_input/output
	 * m_data = the matching rule
	 * The vestigial element is the same memory area used by
	 * the dn_pkt, and IS FREED HERE because it can contain
	 * parameters passed to the called routine. The buffer IS NOT
	 * A REAL MBUF, just a block of memory acquired with malloc().
	 */
	switch (pkt->dn_dir) {
	case DN_TO_IP_OUT: {
	    struct rtentry *tmp_rt = pkt->ro.ro_rt ;

	    (void)ip_output((struct mbuf *)pkt, (struct mbuf *)pkt->ifp,
			&(pkt->ro), pkt->dn_dst, NULL);
	    if (tmp_rt)
		 tmp_rt->rt_refcnt--; /* XXX return a reference count */
	    }
	    break ;
	case DN_TO_IP_IN :
	    ip_input((struct mbuf *)pkt) ;
	    break ;
#ifdef BRIDGE
	case DN_TO_BDG_FWD : {
	    struct mbuf *m = pkt ;
	    bdg_forward( &m, pkt->ifp);
	    if (m)
		m_freem( m );
	    }
	    break ;
#endif
	default:
	    printf("dummynet: bad switch %d!\n", pkt->dn_dir);
	    m_freem(pkt->dn_m);
	    break ;
	}
	FREE(pkt, M_IPFW);
    }
}

/*
 * this is the periodic task that moves packets between the R-
 * and the P- queue
 */
void
dummynet()
{
    struct dn_pipe *p ;
    int s ;

    dn_calls++ ;
    for (p = all_pipes ; p ; p = p->next ) {
	/*
	 * Increment the amount of data that can be sent. However,
	 * don't do that if the channel is idle
	 * (r.head == NULL && numbytes >= bandwidth).
	 * This bug fix is from tim shepard (shep@bbn.com)
	 */
        s = splimp();
	if (p->r.head != NULL || p->numbytes < p->bandwidth )
		p->numbytes += p->bandwidth ;
	dn_move(p, 0); /* is it really 0 (also below) ? */
	splx(s);
    }
 
    /*
     * finally, if some queue has data, restart the timer.
     */
    s = splimp();
    dn_idle = 1;
    dn_restart();
    splx(s);
}

/*
 * dummynet hook for packets.
 * input and output use the same code, so i use bit 16 in the pipe
 * number to chose the direction: 1 for output packets, 0 for input.
 * for input, only m is significant. For output, also the others.
 */
int
dummynet_io(int pipe_nr, int dir,
	struct mbuf *m, struct ifnet *ifp, struct route *ro,
	struct sockaddr_in *dst,
	struct ip_fw_chain *rule)
{
    struct dn_pkt *pkt;
    struct dn_pipe *pipe;
    int len = m->m_pkthdr.len ;

    int s=splimp();

    pipe_nr &= 0xffff ;
    /*
     * locate pipe. First time is expensive, next have direct access.
     */

    if ( (pipe = rule->rule->pipe_ptr) == NULL ) {
	for (pipe=all_pipes; pipe && pipe->pipe_nr !=pipe_nr; pipe=pipe->next)
	    ;
	if (pipe == NULL) {
	    splx(s);
	    if (dn_debug)
		printf("warning, pkt for no pipe %d\n", pipe_nr);
	    m_freem(m);
	    return 0 ;
	} else
	    rule->rule->pipe_ptr = pipe ;
    }
 
    /*
     * should i drop ?
     * This section implements random packet drop.
     */
    if ( (pipe->plr && random() < pipe->plr) ||
         (pipe->queue_size && pipe->r_len >= pipe->queue_size) ||
         (pipe->queue_size_bytes &&
	    len + pipe->r_len_bytes > pipe->queue_size_bytes) ||
		(pkt = (struct dn_pkt *)malloc(sizeof (*pkt),
			M_IPFW, M_NOWAIT) ) == NULL ) {
	splx(s);
	if (dn_debug)
	    printf("-- dummynet: drop from pipe %d, have %d pks, %d bytes\n",
		pipe_nr,  pipe->r_len, pipe->r_len_bytes);
	pipe->r_drops++ ;
	m_freem(m);
	return 0 ; /* XXX error */
    }
    bzero(pkt, sizeof(*pkt) );
    /* build and enqueue packet */
    pkt->hdr.mh_type = MT_DUMMYNET ;
    (struct ip_fw_chain *)pkt->hdr.mh_data = rule ;
    pkt->dn_next = NULL;
    pkt->dn_m = m;
    pkt->dn_dir = dir ;
    pkt->delay = 0;

    pkt->ifp = ifp;
    if (dir == DN_TO_IP_OUT) {
	/*
	 * we need to copy *ro because for icmp pkts (and maybe others)
	 * the caller passed a pointer into the stack.
	 */
	pkt->ro = *ro;
	if (ro->ro_rt)
	    ro->ro_rt->rt_refcnt++ ; /* XXX */
	/*
	 * and again, dst might be a pointer into *ro...
	 */
	if (dst == &ro->ro_dst) /* dst points into ro */
	    dst = &(pkt->ro.ro_dst) ;

	pkt->dn_dst = dst;
    }
    if (pipe->r.head == NULL)
	pipe->r.head = pkt;
    else
	(struct dn_pkt *)pipe->r.tail->dn_next = pkt;
    pipe->r.tail = pkt;
    pipe->r_len++;
    pipe->r_len_bytes += len ;

    /* 
     * here we could implement RED if we like to
     */

    if (pipe->r.head == pkt) {       /* process immediately */
        dn_move(pipe, 1);
    }
    if (dn_idle)
	dn_restart();
    splx(s);
    return 0;
}

/*
 * dispose all packets queued on a pipe
 */
static void
purge_pipe(struct dn_pipe *pipe)
{
    struct dn_pkt *pkt, *n ;
    struct rtentry *tmp_rt ;

    for (pkt = pipe->r.head ; pkt ; ) {
	if (tmp_rt = pkt->ro.ro_rt )
	     tmp_rt->rt_refcnt--; /* XXX return a reference count */
	m_freem(pkt->dn_m);
	n = pkt ;
	pkt = (struct dn_pkt *)pkt->dn_next ;
	free(n, M_IPFW) ;
    }
    for (pkt = pipe->p.head ; pkt ; ) {
	if (tmp_rt = pkt->ro.ro_rt )
	     tmp_rt->rt_refcnt--; /* XXX return a reference count */
	m_freem(pkt->dn_m);
	n = pkt ;
	pkt = (struct dn_pkt *)pkt->dn_next ;
	free(n, M_IPFW) ;
    }
}

/*
 * delete all pipes returning memory
 */
static void
dummynet_flush()
{
    struct dn_pipe *q, *p = all_pipes ;
    int s = splnet() ;

    all_pipes = NULL ;
    splx(s) ;
    /*
     * purge all queued pkts and delete all pipes
     */
    for ( ; p ; ) {
	purge_pipe(p);
	q = p ;
	p = p->next ;	
	free(q, M_IPFW);
    }
}

extern struct ip_fw_chain *ip_fw_default_rule ;
/*
 * when a firewall rule is deleted, scan all pipes and remove the flow-id
 * from packets matching this rule.
 */
void
dn_rule_delete(void *r)
{

    struct dn_pipe *q, *p = all_pipes ;

    for ( p= all_pipes ; p ; p = p->next ) {
	struct dn_pkt *x ;
	for (x = p->r.head ; x ; x = (struct dn_pkt *)x->dn_next )
	    if (x->hdr.mh_data == r)
		x->hdr.mh_data = (void *)ip_fw_default_rule ;
	for (x = p->p.head ; x ; x = (struct dn_pkt *)x->dn_next )
	    if (x->hdr.mh_data == r)
		x->hdr.mh_data = (void *)ip_fw_default_rule ;
    }
}

/*
 * handler for the various dummynet socket options
 * (get, flush, config, del)
 */
static int
ip_dn_ctl(int optname, struct mbuf **mm)
{
	struct mbuf *m ;
	if (optname == IP_DUMMYNET_GET) {
	    struct dn_pipe *p = all_pipes ;
	    *mm = m = m_get(M_WAIT, MT_SOOPTS);
	    m->m_len = 0 ;
	    m->m_next = NULL ;
	    for (; p ;  p = p->next ) {
		struct dn_pipe *q = mtod(m,struct dn_pipe *) ;
		memcpy( m->m_data, p, sizeof(*p) );
		/*
		 * return bw and delay in bits/s and ms, respectively
		 */
		q->delay = (q->delay * 1000) / hz ;

		m->m_len = sizeof(*p) ;
		m->m_next = m_get(M_WAIT, MT_SOOPTS);
		m = m->m_next ;
		m->m_len = 0 ;
	    }
	    return 0 ;
	}
	if (securelevel > 2) { /* like in the firewall code... */
	    if (m) (void)m_free(m);
	    return (EPERM) ;
	}
	m = *mm ;
	if (optname == IP_DUMMYNET_FLUSH) {
	    dummynet_flush() ;
	    if (m) (void)m_free(m);
	    return 0 ;
	}
	if (!m)		/* need an argument for the following */
		return (EINVAL);
	if (optname == IP_DUMMYNET_CONFIGURE) {
	    struct dn_pipe *p = mtod(m,struct dn_pipe *) ;
	    struct dn_pipe *x, *a, *b ;
	    if (m->m_len != sizeof (*p) ) {
		printf("dn_pipe Invalid length, %d instead of %d\n",
			m->m_len, sizeof(*p) );
		(void)m_free(m);
		return (EINVAL);
	    }
	    /*
	     * The config program passes parameters as follows:
	     * bandwidth = bits/second (0 = no limits);
	     * delay = ms
	     *    must be translated in ticks.
	     * queue_size = slots (0 = no limit)
	     * queue_size_bytes = bytes (0 = no limit)
	     *	  only one can be set, must be bound-checked
	     */
	    p->delay = ( p->delay * hz ) / 1000 ;
	    if (p->queue_size == 0 && p->queue_size_bytes == 0)
		p->queue_size = 50 ;
	    if (p->queue_size != 0 )	/* buffers are prevailing */
		p->queue_size_bytes = 0 ;
	    if (p->queue_size > 100)
		p->queue_size = 100 ;
	    if (p->queue_size_bytes > 1024*1024)
		p->queue_size_bytes = 1024*1024 ;
#if 0
	    printf("ip_dn: config pipe %d %d bit/s %d ms %d bufs\n",
		p->pipe_nr,
		p->bandwidth * 8 * hz ,
		p->delay * 1000 / hz , p->queue_size);
#endif
	    for (a = NULL , b = all_pipes ; b && b->pipe_nr < p->pipe_nr ;
		 a = b , b = b->next) ;
	    if (b && b->pipe_nr == p->pipe_nr) {
		/* XXX should spl and flush old pipe... */
		b->bandwidth = p->bandwidth ;
		b->delay = p->delay ;
		b->ticks_from_last_insert = p->delay ;
		b->queue_size = p->queue_size ;
		b->queue_size_bytes = p->queue_size_bytes ;
		b->plr = p->plr ;
	    } else {
		int s ;
		x = malloc(sizeof(struct dn_pipe), M_IPFW, M_DONTWAIT) ;
		if (x == NULL) {
		    printf("ip_dummynet.c: sorry no memory\n");
		    return (ENOSPC) ;
		}
		bzero(x, sizeof(*x) );
		x->bandwidth = p->bandwidth ;
		x->delay = p->delay ;
		x->ticks_from_last_insert = p->delay ;
		x->pipe_nr = p->pipe_nr ;
		x->queue_size = p->queue_size ;
		x->queue_size_bytes = p->queue_size_bytes ;
		x->plr = p->plr ;

		s = splnet() ;
		x->next = b ;
		if (a == NULL)
		    all_pipes = x ;
		else
		    a->next = x ;
		splx(s);
	    }
	    (void)m_free(m);
	    return 0 ;
	}
	if (optname == IP_DUMMYNET_DEL) {
	    struct dn_pipe *p = mtod(m,struct dn_pipe *) ;
	    struct dn_pipe *x, *a, *b ;

	    for (a = NULL , b = all_pipes ; b && b->pipe_nr < p->pipe_nr ;
		 a = b , b = b->next) ;
	    if (b && b->pipe_nr == p->pipe_nr) {	/* found pipe */
		int s = splnet() ;
		struct ip_fw_chain *chain = ip_fw_chain.lh_first;

		if (a == NULL)
		    all_pipes = b->next ;
		else
		    a->next = b->next ;
		/*
		 * remove references to this pipe from the ip_fw rules.
		 */
		for (; chain; chain = chain->chain.le_next) {
		    register struct ip_fw *const f = chain->rule;
		    if (f->pipe_ptr == b)
			f->pipe_ptr = NULL ;
		}
		splx(s);
		purge_pipe(b);	/* remove pkts from here */
		free(b, M_IPFW);
	    }
	}
	return 0 ;
}

void
ip_dn_init(void)
{
    printf("DUMMYNET initialized (990504)\n");
    all_pipes = NULL ;
    ip_dn_ctl_ptr = ip_dn_ctl;
}

#ifdef DUMMYNET_MODULE

#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

MOD_MISC(dummynet);

static ip_dn_ctl_t *old_dn_ctl_ptr ;

static int
dummynet_load(struct lkm_table *lkmtp, int cmd)
{
	int s=splnet();
	old_dn_ctl_ptr = ip_dn_ctl_ptr;
	ip_dn_init();
	splx(s);
	return 0;
}

static int
dummynet_unload(struct lkm_table *lkmtp, int cmd)
{
	int s=splnet();
	ip_dn_ctl_ptr =  old_dn_ctl_ptr;
	splx(s);
	dummynet_flush();
	printf("DUMMYNET unloaded\n");
	return 0;
}

int
dummynet_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
    DISPATCH(lkmtp, cmd, ver, dummynet_load, dummynet_unload, lkm_nullcmd);
}
#endif
