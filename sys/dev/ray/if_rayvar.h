/*-
 * Copyright (C) 2000
 * Dr. Duncan McLennan Barclay, dmlb@ragnet.demon.co.uk.
 *
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DUNCAN BARCLAY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DUNCAN BARCLAY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

/*
 * Network parameters, used twice in softc to store what we want and what
 * we have.
 *
 * The current parameters are ONLY valid in a function called from the runq
 * and should not be accessed directly from ioctls.
 */
struct ray_nw_param {
    struct ray_cmd_net	p_1;
    struct ray_net_params \
    			p_2;
    u_int8_t		np_ap_status;
    int			np_promisc;	/* Promiscious mode status	*/
    int			np_framing;	/* Packet framing types		*/
    int			np_auth;	/* Authentication status	*/
    int			np_havenet;	/* True if we have a network	*/
};
#define np_upd_param	p_1.c_upd_param
#define	np_bss_id	p_1.c_bss_id
#define	np_inited	p_1.c_inited
#define	np_def_txrate	p_1.c_def_txrate
#define	np_encrypt	p_1.c_encrypt
#define np_net_type	p_2.p_net_type
#define np_ssid		p_2.p_ssid
#define np_priv_start	p_2.p_privacy_must_start
#define np_priv_join	p_2.p_privacy_can_join

/*
 * One of these structures per allocated device
 */
struct ray_softc {

    device_t dev;			/* Device */
    struct ifnet	*ifp;		/* Ethernet common 		*/
    struct callout_handle
    			tx_timerh;	/* Handle for tx timer	*/
    struct callout_handle
    			com_timerh;	/* Handle for command timer	*/

    bus_space_tag_t	am_bst;		/* Bus space tag for attribute memory */
    bus_space_handle_t	am_bsh;		/* Bus space handle for attribute mem */
    int			am_rid;		/* Resource id for attribute memory */
    struct resource*	am_res;		/* Resource for attribute memory */
    bus_space_tag_t	cm_bst;		/* Bus space tag for common memory */
    bus_space_handle_t	cm_bsh;		/* Bus space handle for common memory */
    int			cm_rid;		/* Resource id for common memory */
    struct resource*	cm_res;		/* Resource for common memory */
    int			irq_rid;	/* Resource id for irq */
    struct resource*	irq_res;	/* Resource for irq */
    void *		irq_handle;	/* Handle for irq handler */

    u_int8_t		sc_ccsinuse[64];/* ccss' in use -- not for tx	*/
    u_char		sc_gone;	/* 1 = Card bailed out		*/

    struct ray_ecf_startup_v5
    			sc_ecf_startup; /* Startup info from card	*/

    TAILQ_HEAD(ray_comq, ray_comq_entry) 
			sc_comq;	/* Command queue		*/

    struct ray_nw_param	sc_c;		/* current network params 	*/
    struct ray_nw_param sc_d;		/* desired network params	*/

    int			sc_checkcounters;
    u_int64_t		sc_rxoverflow;	/* Number of rx overflows	*/
    u_int64_t		sc_rxcksum;	/* Number of checksum errors	*/
    u_int64_t		sc_rxhcksum;	/* Number of header checksum errors */
    u_int8_t		sc_rxnoise;	/* Average receiver level	*/
    struct ray_siglev	sc_siglevs[RAY_NSIGLEVRECS]; /* Antenna/levels	*/
};

#define	sc_station_addr	sc_ecf_startup.e_station_addr
#define	sc_version	sc_ecf_startup.e_fw_build_string
#define	sc_tibsize	sc_ecf_startup.e_tibsize

/*
 * Command queue definitions
 */
typedef void (*ray_comqfn_t)(struct ray_softc *sc, struct ray_comq_entry *com);
struct ray_comq_entry {
	TAILQ_ENTRY(ray_comq_entry) c_chain;	/* Tail queue.		*/
	ray_comqfn_t	c_function;		/* Function to call */
	int		c_flags;		/* Flags		*/
	u_int8_t	c_retval;		/* Return value		*/
	void		*c_wakeup;		/* Sleeping on this	*/
	size_t		c_ccs;			/* CCS structure	*/
	struct ray_nw_param
			c_desired;		/* network settings	*/
	struct ray_param_req
    			*c_pr;			/* MIB report/update	*/
	char		*c_mesg;
};

/*
 * Macro's and constants
 */
static int mib_info[RAY_MIB_MAX+1][3] = RAY_MIB_INFO;

/* Indirections for reading/writing memory - from NetBSD/if_ray.c */
#ifndef offsetof
#define offsetof(type, member) \
    ((size_t)(&((type *)0)->member))
#endif /* offsetof */

#define ATTR_READ_1(sc, off) \
    ((u_int8_t)bus_space_read_1((sc)->am_bst, (sc)->am_bsh, (off)))

#define ATTR_WRITE_1(sc, off, val) \
    bus_space_write_1((sc)->am_bst, (sc)->am_bsh, (off), (val))

#define	SRAM_READ_1(sc, off) \
    ((u_int8_t)bus_space_read_1((sc)->cm_bst, (sc)->cm_bsh, (off)))

#define SRAM_READ_REGION(sc, off, p, n) \
    bus_space_read_region_1((sc)->cm_bst, (sc)->cm_bsh, (off), (void *)(p), (n))

#define	SRAM_READ_FIELD_1(sc, off, s, f) \
    SRAM_READ_1((sc), (off) + offsetof(struct s, f))

#define	SRAM_READ_FIELD_2(sc, off, s, f)			\
    ((((u_int16_t)SRAM_READ_1((sc), (off) + offsetof(struct s, f)) << 8) \
    |(SRAM_READ_1((sc), (off) + 1 + offsetof(struct s, f)))))

#define	SRAM_READ_FIELD_N(sc, off, s, f, p, n)	\
    SRAM_READ_REGION((sc), (off) + offsetof(struct s, f), (p), (n))

#define	SRAM_WRITE_1(sc, off, val)	\
    bus_space_write_1((sc)->cm_bst, (sc)->cm_bsh, (off), (val))

#define SRAM_WRITE_REGION(sc, off, p, n) \
    bus_space_write_region_1((sc)->cm_bst, (sc)->cm_bsh, (off), (void *)(p), (n))

#define	SRAM_WRITE_FIELD_1(sc, off, s, f, v) 	\
    SRAM_WRITE_1((sc), (off) + offsetof(struct s, f), (v))

#define	SRAM_WRITE_FIELD_2(sc, off, s, f, v) do {	\
    SRAM_WRITE_1((sc), (off) + offsetof(struct s, f), (((v) >> 8 ) & 0xff)); \
    SRAM_WRITE_1((sc), (off) + 1 + offsetof(struct s, f), ((v) & 0xff)); \
} while (0)

#define	SRAM_WRITE_FIELD_N(sc, off, s, f, p, n)	\
    SRAM_WRITE_REGION((sc), (off) + offsetof(struct s, f), (p), (n))

/* Framing types */
/* XXX maybe better as part of the if structure? */
#define RAY_FRAMING_ENCAPSULATION	0
#define RAY_FRAMING_TRANSLATION		1

/* Authentication states */
#define RAY_AUTH_UNAUTH		0
#define RAY_AUTH_WAITING	1
#define RAY_AUTH_AUTH		2
#define RAY_AUTH_NEEDED		3

/* Flags for runq entries */
#define RAY_COM_FWOK		0x0001		/* Wakeup on completion	*/
#define RAY_COM_FRUNNING	0x0002		/* This one running	*/
#define RAY_COM_FCOMPLETED	0x0004		/* This one completed	*/
#define RAY_COM_FWAIT		0x0008		/* Do not run the queue */
#define RAY_COM_FCHKRUNNING	0x0010		/* Check IFF_RUNNING	*/
#define RAY_COM_FDETACHED	0x0020		/* Card is gone		*/
#define RAY_COM_FWOKEN		0x0040		/* Woken by detach	*/
#define RAY_COM_FLAGS_PRINTFB	\
	"\020"			\
	"\001WOK"		\
	"\002RUNNING"		\
	"\003COMPLETED"		\
	"\004WAIT"		\
	"\005CHKRUNNING"	\
	"\006DETACHED"

#define RAY_COM_NEEDS_TIMO(cmd)	(		\
	 (cmd == RAY_CMD_DOWNLOAD_PARAMS) ||	\
	 (cmd == RAY_CMD_UPDATE_PARAMS) ||	\
	 (cmd == RAY_CMD_UPDATE_MCAST)		\
	)

#ifndef RAY_COM_TIMEOUT
#define RAY_COM_TIMEOUT		(hz / 2)
#endif

#ifndef RAY_RESET_TIMEOUT
#define RAY_RESET_TIMEOUT	(10 * hz)
#endif

#ifndef RAY_TX_TIMEOUT
#define RAY_TX_TIMEOUT		(hz / 2)
#endif

#define RAY_CCS_FREE(sc, ccs) \
    SRAM_WRITE_FIELD_1((sc), (ccs), ray_cmd, c_status, RAY_CCS_STATUS_FREE)

#define RAY_ECF_READY(sc) \
    (!(ATTR_READ_1((sc), RAY_ECFIR) & RAY_ECFIR_IRQ))

#define	RAY_ECF_START_CMD(sc)	ATTR_WRITE_1((sc), RAY_ECFIR, RAY_ECFIR_IRQ)

#define	RAY_HCS_CLEAR_INTR(sc)	ATTR_WRITE_1((sc), RAY_HCSIR, 0)

#define RAY_HCS_INTR(sc)	(ATTR_READ_1((sc), RAY_HCSIR) & RAY_HCSIR_IRQ)

#define RAY_PANIC(sc, fmt, args...) do {				\
    panic("ray%d: %s(%d) " fmt "\n", device_get_unit((sc)->dev),	\
	__func__ , __LINE__ , ##args);					\
} while (0)

#define RAY_PRINTF(sc, fmt, args...) do {				\
    device_printf((sc)->dev, "%s(%d) " fmt "\n",			\
        __func__ , __LINE__ , ##args);					\
} while (0)

#define RAY_COM_MALLOC(function, flags)	\
    ray_com_malloc((function), (flags), __STRING(function));

#define RAY_COM_FREE(com, ncom)	do {					\
    int i;								\
    for (i = 0; i < ncom; i++)						\
	    FREE(com[i], M_RAYCOM);					\
} while (0)

/*
 * This macro handles adding commands to the runq and quickly
 * getting away when the card is detached. The macro returns
 * from the current function with ENXIO.
 */
#define RAY_COM_RUNQ(sc, com, ncom, mesg, error) do {			\
    (error) = ray_com_runq_add((sc), (com), (ncom), (mesg));		\
    if ((error) == ENXIO) {						\
	    RAY_COM_FREE((com), (ncom));				\
	    return (error);						\
    } else if ((error) && ((error) != ENXIO))				\
	    RAY_PRINTF(sc, "got error from runq 0x%x", (error));	\
} while (0)

/*
 * There are a number of entry points into the ray_init_xxx routines.
 * These can be classed into two types: a) those that happen as a result
 * of a change to the cards operating parameters (e.g. BSSID change), and
 * b) those that happen as a result of a change to the interface parameters
 * (e.g. a change to the IP address). The second set of entries need not
 * send a command to the card when the card is IFF_RUNNING. The
 * RAY_COM_FCHKRUNNING flags indicates when the RUNNING flag should be
 * checked, and this macro does the necessary check and command abort.
 */
#define RAY_COM_CHKRUNNING(sc, com, ifp) do {				\
    if (((com)->c_flags & RAY_COM_FCHKRUNNING) &&			\
	((ifp)->if_flags & IFF_RUNNING)) {				\
	    ray_com_runq_done(sc);					\
	    return;							\
} } while (0)
    
    

#define RAY_COM_INIT(com, function, flags)	\
    ray_com_init((com), (function), (flags), __STRING(function));

#ifndef RAY_COM_CHECK
#define RAY_COM_CHECK(sc, com)
#endif /* RAY_COM_CHECK */

#ifndef RAY_MBUF_DUMP
#define RAY_MBUF_DUMP(sc, mask, m, s)
#endif /* RAY_MBUF_DUMP */

#ifndef RAY_RECERR
#define RAY_RECERR(sc, fmt, args...) do {				\
    struct ifnet *ifp = (sc)->ifp;				\
    if (ifp->if_flags & IFF_DEBUG) {					\
	    device_printf((sc)->dev, "%s(%d) " fmt "\n",		\
		__func__ , __LINE__ , ##args);				\
} } while (0)
#endif /* RAY_RECERR */

/* XXX this should be in CCSERR but don't work - probably need to use ##ifp->(iferrcounter)++;						\*/
#ifndef RAY_CCSERR
#define RAY_CCSERR(sc, status, iferrcounter) do {			\
    struct ifnet *ifp = (sc)->ifp;				\
    char *ss[] = RAY_CCS_STATUS_STRINGS;				\
    if ((status) != RAY_CCS_STATUS_COMPLETE) {				\
	if (ifp->if_flags & IFF_DEBUG) {				\
	    device_printf((sc)->dev,					\
	        "%s(%d) ECF command completed with status %s\n",	\
		__func__ , __LINE__ , ss[(status)]);			\
} } } while (0)
#endif /* RAY_CCSERR */

#ifndef RAY_MAP_CM
#define RAY_MAP_CM(sc)
#endif /* RAY_MAP_CM */

/*
 * Management information element payloads
 */
union ieee80211_information {
	char	ssid[IEEE80211_NWID_LEN+1];
	struct rates {
		u_int8_t	*p;
	} rates;
	struct fh {
		u_int16_t	dwell;
		u_int8_t	set;
		u_int8_t	pattern;
		u_int8_t	index;
	} fh;
	struct ds {
		u_int8_t	channel;
	} ds;
	struct cf {
		u_int8_t	count;
		u_int8_t	period;
		u_int8_t	maxdur[2];
		u_int8_t	dur[2];
	} cf;
	struct tim {
		u_int8_t	count;
		u_int8_t	period;
		u_int8_t	bitctl;
		/* u_int8_t	pvt[251]; The driver needs to use this. */
	} tim;
	struct ibss {
		u_int16_t	atim;
	} ibss;
	struct challenge {
		u_int8_t	*p;
		u_int8_t	len;
	} challenge;
};
