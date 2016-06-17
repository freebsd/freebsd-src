/******************************************************************************
 *
 *	(C)Copyright 1998,1999 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

#ifndef	_CMTDEF_
#define _CMTDEF_

/* **************************************************************** */

/*
 * implementation specific constants
 * MODIIFY THE FOLLWOING THREE DEFINES
 */
#define AMDPLC			/* if Amd PLC chip used */
#ifdef	CONC
#define NUMPHYS		12	/* 2 for SAS or DAS, more for Concentrator */
#else
#ifdef	CONC_II
#define NUMPHYS		24	/* 2 for SAS or DAS, more for Concentrator */
#else
#define NUMPHYS		2	/* 2 for SAS or DAS, more for Concentrator */
#endif
#endif
#define NUMMACS		1	/* only 1 supported at the moment */
#define NUMPATHS	2	/* primary and secondary path supported */

/*
 * DO NOT MODIFY BEYOND THIS POINT
 */

/* **************************************************************** */

#if	NUMPHYS > 2
#define CONCENTRATOR
#endif

/*
 * Definitions for comfortable LINT usage
 */
#ifdef	lint
#define LINT_USE(x)	(x)=(x)
#else
#define LINT_USE(x)
#endif

#ifdef	DEBUG
#define	DB_PR(flag,a,b,c)	{ if (flag) printf(a,b,c) ; }
#else
#define	DB_PR(flag,a,b,c)
#endif

#ifdef DEBUG_BRD
#define DB_ECM(a,b,c)		DB_PR((smc->debug.d_smt&1),a,b,c)
#define DB_ECMN(n,a,b,c)	DB_PR((smc->debug.d_ecm >=(n)),a,b,c)
#define DB_RMT(a,b,c)		DB_PR((smc->debug.d_smt&2),a,b,c)
#define DB_RMTN(n,a,b,c)	DB_PR((smc->debug.d_rmt >=(n)),a,b,c)
#define DB_CFM(a,b,c)		DB_PR((smc->debug.d_smt&4),a,b,c)
#define DB_CFMN(n,a,b,c)	DB_PR((smc->debug.d_cfm >=(n)),a,b,c)
#define DB_PCM(a,b,c)		DB_PR((smc->debug.d_smt&8),a,b,c)
#define DB_PCMN(n,a,b,c)	DB_PR((smc->debug.d_pcm >=(n)),a,b,c)
#define DB_SMT(a,b,c)		DB_PR((smc->debug.d_smtf),a,b,c)
#define DB_SMTN(n,a,b,c)	DB_PR((smc->debug.d_smtf >=(n)),a,b,c)
#define DB_SBA(a,b,c)		DB_PR((smc->debug.d_sba),a,b,c)
#define DB_SBAN(n,a,b,c)	DB_PR((smc->debug.d_sba >=(n)),a,b,c)
#define DB_ESS(a,b,c)		DB_PR((smc->debug.d_ess),a,b,c)
#define DB_ESSN(n,a,b,c)	DB_PR((smc->debug.d_ess >=(n)),a,b,c)
#else
#define DB_ECM(a,b,c)		DB_PR((debug.d_smt&1),a,b,c)
#define DB_ECMN(n,a,b,c)	DB_PR((debug.d_ecm >=(n)),a,b,c)
#define DB_RMT(a,b,c)		DB_PR((debug.d_smt&2),a,b,c)
#define DB_RMTN(n,a,b,c)	DB_PR((debug.d_rmt >=(n)),a,b,c)
#define DB_CFM(a,b,c)		DB_PR((debug.d_smt&4),a,b,c)
#define DB_CFMN(n,a,b,c)	DB_PR((debug.d_cfm >=(n)),a,b,c)
#define DB_PCM(a,b,c)		DB_PR((debug.d_smt&8),a,b,c)
#define DB_PCMN(n,a,b,c)	DB_PR((debug.d_pcm >=(n)),a,b,c)
#define DB_SMT(a,b,c)		DB_PR((debug.d_smtf),a,b,c)
#define DB_SMTN(n,a,b,c)	DB_PR((debug.d_smtf >=(n)),a,b,c)
#define DB_SBA(a,b,c)		DB_PR((debug.d_sba),a,b,c)
#define DB_SBAN(n,a,b,c)	DB_PR((debug.d_sba >=(n)),a,b,c)
#define DB_ESS(a,b,c)		DB_PR((debug.d_ess),a,b,c)
#define DB_ESSN(n,a,b,c)	DB_PR((debug.d_ess >=(n)),a,b,c)
#endif

#ifndef	SS_NOT_DS
#define	SK_LOC_DECL(type,var)	type var
#else
#define	SK_LOC_DECL(type,var)	static type var
#endif
/*
 * PHYs and PORTS
 * Note: Don't touch the definition of PA and PB. Those might be used
 *	by some "for" loops.
 */
#define PA		0
#define PB		1
#if	defined(SUPERNET_3) || defined(CONC_II)
/*
 * The port indices have to be different,
 * because the MAC output goes through the 2. PLC
 * Conc II: It has to be the first port in the row.
 */
#define PS		0	/* Internal PLC which is the same as PA */
#else
#define PS		1
#endif
#define PM		2		/* PM .. PA+NUM_PHYS-1 */

/*
 * PHY types - as in path descriptor 'fddiPHYType'
 */
#define TA			0	/* A port */
#define TB			1	/* B port */
#define TS			2	/* S port */
#define TM			3	/* M port */
#define TNONE			4


/*
 * indexes in MIB
 */
#define INDEX_MAC	1
#define INDEX_PATH	1
#define INDEX_PORT	1


/*
 * policies
 */
#define POLICY_AA	(1<<0)		/* reject AA */
#define POLICY_AB	(1<<1)		/* reject AB */
#define POLICY_AS	(1<<2)		/* reject AS */
#define POLICY_AM	(1<<3)		/* reject AM */
#define POLICY_BA	(1<<4)		/* reject BA */
#define POLICY_BB	(1<<5)		/* reject BB */
#define POLICY_BS	(1<<6)		/* reject BS */
#define POLICY_BM	(1<<7)		/* reject BM */
#define POLICY_SA	(1<<8)		/* reject SA */
#define POLICY_SB	(1<<9)		/* reject SB */
#define POLICY_SS	(1<<10)		/* reject SS */
#define POLICY_SM	(1<<11)		/* reject SM */
#define POLICY_MA	(1<<12)		/* reject MA */
#define POLICY_MB	(1<<13)		/* reject MB */
#define POLICY_MS	(1<<14)		/* reject MS */
#define POLICY_MM	(1<<15)		/* reject MM */

/*
 * commands
 */

/*
 * EVENTS
 * event classes
 */
#define EVENT_ECM	1		/* event class ECM */
#define EVENT_CFM	2		/* event class CFM */
#define EVENT_RMT	3		/* event class RMT */
#define EVENT_SMT	4		/* event class SMT */
#define EVENT_PCM	5		/* event class PCM */
#define EVENT_PCMA	5		/* event class PCMA */
#define EVENT_PCMB	6		/* event class PCMB */

/* WARNING :
 * EVENT_PCM* must be last in the above list
 * if more then two ports are used, EVENT_PCM .. EVENT_PCMA+NUM_PHYS-1
 * are used !
 */

#define EV_TOKEN(class,event)	(((u_long)(class)<<16L)|((u_long)(event)))
#define EV_T_CLASS(token)	((int)((token)>>16)&0xffff)
#define EV_T_EVENT(token)	((int)(token)&0xffff)

/*
 * ECM events
 */
#define EC_CONNECT	1		/* connect request */
#define EC_DISCONNECT	2		/* disconnect request */
#define EC_TRACE_PROP	3		/* trace propagation */
#define EC_PATH_TEST	4		/* path test */
#define EC_TIMEOUT_TD	5		/* timer TD_min */
#define EC_TIMEOUT_TMAX	6		/* timer trace_max */
#define EC_TIMEOUT_IMAX	7		/* timer I_max */
#define EC_TIMEOUT_INMAX 8		/* timer IN_max */
#define EC_TEST_DONE	9		/* path test done */

/*
 * CFM events
 */
#define CF_LOOP		1		/* cf_loop flag from PCM */
#define CF_LOOP_A	1		/* cf_loop flag from PCM */
#define CF_LOOP_B	2		/* cf_loop flag from PCM */
#define CF_JOIN		3		/* cf_join flag from PCM */
#define CF_JOIN_A	3		/* cf_join flag from PCM */
#define CF_JOIN_B	4		/* cf_join flag from PCM */

/*
 * PCM events
 */
#define PC_START		1
#define PC_STOP			2
#define PC_LOOP			3
#define PC_JOIN			4
#define PC_SIGNAL		5
#define PC_REJECT		6
#define PC_MAINT    		7
#define PC_TRACE		8
#define PC_PDR			9
#define PC_ENABLE		10
#define PC_DISABLE		11

/*
 * must be ordered as in LineStateType
 */
#define PC_QLS			12
#define PC_ILS			13
#define PC_MLS			14
#define PC_HLS			15
#define PC_LS_PDR		16
#define PC_LS_NONE		17
#define LS2MIB(x)	((x)-PC_QLS)
#define MIB2LS(x)	((x)+PC_QLS)

#define PC_TIMEOUT_TB_MAX	18	/* timer TB_max */
#define PC_TIMEOUT_TB_MIN	19	/* timer TB_min */
#define PC_TIMEOUT_C_MIN	20	/* timer C_Min */
#define PC_TIMEOUT_T_OUT	21	/* timer T_Out */
#define PC_TIMEOUT_TL_MIN	22	/* timer TL_Min */
#define PC_TIMEOUT_T_NEXT	23	/* timer t_next[] */
#define PC_TIMEOUT_LCT		24
#define PC_NSE			25	/* NOISE hardware timer */
#define PC_LEM			26	/* LEM done */

/*
 * RMT events				  meaning		from
 */
#define RM_RING_OP	1		/* ring operational	MAC	*/
#define RM_RING_NON_OP	2		/* ring not operational	MAC	*/
#define RM_MY_BEACON	3		/* recvd my beacon	MAC	*/
#define RM_OTHER_BEACON	4		/* recvd other beacon	MAC	*/
#define RM_MY_CLAIM	5		/* recvd my claim	MAC	*/
#define RM_TRT_EXP	6		/* TRT exp		MAC	*/
#define RM_VALID_CLAIM	7		/* claim from dup addr	MAC	*/
#define RM_JOIN		8		/* signal rm_join	CFM	*/
#define RM_LOOP		9		/* signal rm_loop	CFM	*/
#define RM_DUP_ADDR	10		/* dup_addr_test hange	SMT-NIF	*/
#define RM_ENABLE_FLAG	11		/* enable flag */

#define RM_TIMEOUT_NON_OP	12	/* timeout T_Non_OP	*/
#define RM_TIMEOUT_T_STUCK	13	/* timeout T_Stuck	*/
#define RM_TIMEOUT_ANNOUNCE	14	/* timeout T_Announce	*/
#define RM_TIMEOUT_T_DIRECT	15	/* timeout T_Direct	*/
#define RM_TIMEOUT_D_MAX	16	/* timeout D_Max	*/
#define RM_TIMEOUT_POLL		17	/* claim/beacon poller	*/
#define RM_TX_STATE_CHANGE	18	/* To restart timer for D_Max */

/*
 * SMT events
 */
#define SM_TIMER	1		/* timer */
#define SM_FAST		2		/* smt_force_irq */

/* PC modes */
#define PM_NONE		0
#define PM_PEER		1
#define PM_TREE		2

/*
 * PCM withhold codes
 * MIB PC-WithholdType ENUM
 */
#define PC_WH_NONE	0		/* ok */
#define PC_WH_M_M	1		/* M to M */
#define PC_WH_OTHER	2		/* other incompatible phys */
#define PC_WH_PATH	3		/* path not available */
/*
 * LCT duration
 */
#define LC_SHORT	1		/* short LCT */
#define LC_MEDIUM	2		/* medium LCT */
#define LC_LONG		3		/* long LCT */
#define LC_EXTENDED	4		/* extended LCT */

/*
 * path_test values
 */
#define PT_NONE		0
#define PT_TESTING	1		/* test is running */
#define PT_PASSED	2		/* test passed */
#define PT_FAILED	3		/* test failed */
#define PT_PENDING	4		/* path test follows */
#define PT_EXITING	5		/* disconnected while in trace/leave */

/*
 * duplicate address test
 * MIB DupAddressTest ENUM
 */
#define DA_NONE		0		/* 		*/
#define DA_PASSED	1		/* test passed */
#define DA_FAILED	2		/* test failed */


/*
 * optical bypass
 */
#define BP_DEINSERT	0		/* disable bypass */
#define BP_INSERT	1		/* enable bypass */

/*
 * ODL enable/disable
 */
#define PM_TRANSMIT_DISABLE	0	/* disable xmit */
#define PM_TRANSMIT_ENABLE	1	/* enable xmit */

/*
 * parameter for config_mux
 * note : number is index in config_endec table !
 */
#define MUX_THRUA	0		/* through A */
#define MUX_THRUB	1		/* through B */
#define MUX_WRAPA	2		/* wrap A */
#define MUX_WRAPB	3		/* wrap B */
#define MUX_ISOLATE	4		/* isolated */
#define MUX_WRAPS	5		/* SAS */

/*
 * MAC control
 */
#define MA_RESET	0
#define MA_BEACON	1
#define MA_CLAIM	2
#define MA_DIRECTED	3		/* directed beacon */
#define MA_TREQ		4		/* change T_Req */
#define MA_OFFLINE	5		/* switch MAC to offline */


/*
 * trace prop
 * bit map for trace propagation
 */
#define ENTITY_MAC	(NUMPHYS)
#define ENTITY_PHY(p)	(p)
#define ENTITY_BIT(m)	(1<<(m))

/*
 * Resource Tag Types
 */
#define PATH_ISO	0	/* isolated */
#define PATH_PRIM	3	/* primary path */
#define PATH_THRU	5	/* through path */

#define RES_MAC		2	/* resource type MAC */
#define RES_PORT	4	/* resource type PORT */


/*
 * CFM state
 * oops: MUST MATCH CF-StateType in SMT7.2 !
 */
#define SC0_ISOLATED	0		/* isolated */
#define SC1_WRAP_A	5		/* wrap A (not used) */
#define SC2_WRAP_B	6		/* wrap B (not used) */
#define SC4_THRU_A	12		/* through A */
#define SC5_THRU_B	7		/* through B (used in SMT 6.2) */
#define SC7_WRAP_S	8		/* SAS (not used) */
#define SC9_C_WRAP_A	9		/* c wrap A */
#define SC10_C_WRAP_B	10		/* c wrap B */
#define SC11_C_WRAP_S	11		/* c wrap S */

/*
 * convert MIB time in units of 80nS to uS
 */
#define MIB2US(t)		((t)/12)
#define SEC2MIB(s)	((s)*12500000L)
/*
 * SMT timer
 */
struct smt_timer {
	struct smt_timer	*tm_next ;	/* linked list */
	struct s_smc		*tm_smc ;	/* pointer to context */
	u_long			tm_delta ;	/* delta time */
	u_long			tm_token ;	/* token value */
	u_short			tm_active ;	/* flag : active/inactive */
	u_short			tm_pad ;	/* pad field */
} ;

/*
 * communication structures
 */
struct mac_parameter {
	u_long	t_neg ;		/* T_Neg parameter */
	u_long	t_pri ;		/* T_Pri register in MAC */
} ;

/*
 * MAC counters
 */
struct mac_counter {
	u_long	mac_nobuf_counter ;	/* MAC SW counter: no buffer */
	u_long	mac_r_restart_counter ;	/* MAC SW counter: rx restarted */
} ;

/*
 * para struct context for SMT parameters
 */
struct s_pcon {
	int	pc_len ;
	int	pc_err ;
	int	pc_badset ;
	void	*pc_p ;
} ;


/*
 * link error monitor
 */
#define LEM_AVG	5
struct lem_counter {
#ifdef	AM29K
	int	lem_on	;
	u_long	lem_errors ;
	u_long	lem_symbols ;
	u_long	lem_tsymbols ;
	int	lem_s_count ;
	int	lem_n_s ;
	int	lem_values ;
	int	lem_index ;
	int	lem_avg_ber[LEM_AVG] ;
	int	lem_sum ;
#else
	u_short	lem_float_ber ;		/* 10E-nn bit error rate */
	u_long	lem_errors ;		/* accumulated error count */
	u_short	lem_on	;
#endif
} ;

#define NUMBITS	10


#ifdef	AMDPLC

/*
 * PLC state table
 */
struct s_plc {
	u_short	p_state ;		/* current state */
	u_short	p_bits ;		/* number of bits to send */
	u_short	p_start ;		/* first bit pos */
	u_short	p_pad ;			/* padding for alignment */
	u_long soft_err ;		/* error counter */
	u_long parity_err ;		/* error counter */
	u_long ebuf_err ;		/* error counter */
	u_long ebuf_cont ;		/* continous error counter */
	u_long phyinv ;			/* error counter */
	u_long vsym_ctr ;		/* error counter */
	u_long mini_ctr ;		/* error counter */
	u_long tpc_exp ;		/* error counter */
	u_long np_err ;			/* error counter */
	u_long b_pcs ;			/* error counter */
	u_long b_tpc ;			/* error counter */
	u_long b_tne ;			/* error counter */
	u_long b_qls ;			/* error counter */
	u_long b_ils ;			/* error counter */
	u_long b_hls ;			/* error counter */
} ;
#endif

#ifdef	PROTOTYP_INC
#include "fddi/driver.pro"
#else	/* PROTOTYP_INC */
/*
 * function prototypes
 */
#include "h/mbuf.h"	/* Type definitions for MBUFs */
void hwt_restart(                       /* hwt.c */
#ifdef ANSIC
        struct s_smc *smc
#endif
	) ;

SMbuf *smt_build_frame(                 /* smt.c */
#ifdef ANSIC
        struct s_smc *smc,
        int class,
        int type,
        int length
#endif
	) ;

SMbuf *smt_get_mbuf(                     /* drvsr.c */
#ifdef ANSIC
        struct s_smc *smc
#endif
	) ;

void *sm_to_para(                       /* smt.c */
#ifdef ANSIC
	struct s_smc *smc,
        struct smt_header *sm,
        int para
#endif
	) ;

#ifndef SK_UNUSED
#define SK_UNUSED(var)		(void)(var)
#endif

void queue_event() ;
void ecm() ;
void ecm_init() ;
void rmt() ;
void rmt_init() ;
void pcm() ;
void pcm_init() ;
void cfm() ;
void cfm_init() ;
void smt_timer_start() ;
void smt_timer_stop() ;
void pcm_status_state() ;
void plc_config_mux() ;
void sm_lem_evaluate() ;
void smt_clear_una_dna() ;
void mac_status_para() ;
void mac_update_counter() ;
void sm_pm_ls_latch() ;
void sm_ma_control() ;
void sm_mac_check_beacon_claim() ;
void config_mux() ;
void smt_agent_init() ;
void smt_timer_init() ;
void smt_received_pack() ;
void smt_add_para() ;
void smt_swap_para() ;
void ev_init() ;
void hwt_init() ;
u_long hwt_read() ;
void hwt_stop() ;
void hwt_start() ;
void smt_send_mbuf() ;
void smt_free_mbuf() ;
void sm_pm_bypass_req() ;
void rmt_indication() ;
void cfm_state_change() ;
void rx_indication() ;
void tx_indication() ;
#ifndef NO_SMT_PANIC
void smt_panic() ;
#else
#ifdef	DEBUG
void smt_panic() ;
#else
#define	smt_panic(smc,text)
#endif	/* DEBUG */
#endif	/* NO_SMT_PANIC */
void smt_stat_counter() ;
void smt_timer_poll() ;
u_long smt_get_time() ;
u_long smt_get_tid() ;
void smt_timer_done() ;
void smt_set_defaults() ;
void smt_fixup_mib() ;
void smt_reset_defaults() ;
void smt_agent_task() ;
void smt_please_reconnect() ;
int smt_check_para() ;
void driver_get_bia() ;
#ifdef	SUPERNET_3
void drv_reset_indication() ;
#endif	/* SUPERNET_3 */
void smt_start_watchdog() ;

void smt_event() ;
void pcm_event() ;
void rmt_event() ;
void cfm_event() ;
void timer_event() ;
void ev_dispatcher() ;

void smt_get_state() ;
void ecm_get_state() ;
void pcm_get_state() ;
void rmt_get_state() ;

void ecm_state_change() ;
int sm_pm_bypass_present() ;
void pcm_state_change() ;
void rmt_state_change() ;
int sm_pm_get_ls() ;
int pcm_get_s_port() ;
int pcm_rooted_station() ;
int cfm_get_mac_input() ;
int cfm_get_mac_output() ;
int port_to_mib() ;
int cem_build_path() ;
int sm_mac_get_tx_state() ;
int is_individual() ;
int is_my_addr() ;
int is_broadcast() ;
int is_equal() ;
char *get_pcmstate() ;

int smt_action() ;
u_short smt_online() ;
void smt_force_irq() ;
void smt_pmf_received_pack() ;
void smt_send_frame() ;
void smt_set_timestamp() ;
void mac_set_rx_mode() ;
int mac_add_multicast() ;
int mac_set_func_addr() ;
void mac_del_multicast() ;
void mac_update_multicast() ;
void mac_clear_multicast() ;
void mac_rx_directed_beacon() ;
void set_formac_tsync() ;
void formac_reinit_tx() ;
void formac_tx_restart() ;
void process_receive() ;
void init_driver_fplus() ;

void rtm_irq() ;
void rtm_set_timer() ;
void ring_status_indication() ;
void llc_recover_tx() ;
void llc_restart_tx() ;
void plc_clear_irq() ;
void plc_irq() ;
int smt_set_mac_opvalues() ;
#ifdef	TAG_MODE
void mac_drv_pci_fix() ;
void mac_do_pci_fix() ;
void mac_drv_clear_tx_queue() ;
void mac_drv_repair_descr() ;
u_long hwt_quick_read() ;
void hwt_wait_time() ;
#endif

#ifdef SMT_PNMI
#ifdef ANSIC
int pnmi_init (struct s_smc* smc);
int pnmi_process_ndis_id (struct s_smc* smc, u_long ndis_oid, void* buf,
		int len, int* BytesAccessed, int* BytesNeeded, u_char action);
#else
int pnmi_init ();
int pnmi_process_ndis_id ();
#endif
#endif

#ifdef	SBA
#ifndef _H2INC
void sba() ;
#endif
void sba_raf_received_pack() ;
void sba_timer_poll() ;
void smt_init_sba() ;
#endif
#ifdef	ESS
int ess_raf_received_pack() ;
void ess_timer_poll() ;
void ess_para_change() ;
#endif

#ifdef	BOOT
#define smt_srf_event(a,b,c,d)
#define smt_init_evc(a)
#else
void smt_init_evc() ;
void smt_srf_event() ;
#endif

#ifndef SMT_REAL_TOKEN_CT
void smt_emulate_token_ct();
#endif

#if defined(DEBUG) && !defined(BOOT)
void dump_smt() ;
#else
#define	dump_smt(smc,sm,text)
#endif

#ifdef	DEBUG
char	*addr_to_string() ;
void	dump_hex() ;
#endif
#endif	/* PROTOTYP_INC */

/* PNMI default defines */
#ifndef PNMI_INIT
#define	PNMI_INIT(smc)	/* Nothing */
#endif
#ifndef PNMI_GET_ID
#define PNMI_GET_ID( smc, ndis_oid, buf, len, BytesWritten, BytesNeeded ) \
		( 1 ? (-1) : (-1) )
#endif
#ifndef PNMI_SET_ID
#define PNMI_SET_ID( smc, ndis_oid, buf, len, BytesRead, BytesNeeded, \
		set_type) ( 1 ? (-1) : (-1) )
#endif

/*
 * SMT_PANIC defines
 */
#ifndef	SMT_PANIC
#define	SMT_PANIC(smc,nr,msg)	smt_panic (smc, msg)
#endif

#ifndef	SMT_ERR_LOG
#define	SMT_ERR_LOG(smc,nr,msg)	SMT_PANIC (smc, nr, msg)
#endif

#ifndef	SMT_EBASE
#define	SMT_EBASE	100
#endif

#define	SMT_E0100	SMT_EBASE + 0
#define	SMT_E0100_MSG	"cfm FSM: illegal ce_type"
#define	SMT_E0101	SMT_EBASE + 1
#define	SMT_E0101_MSG	"CEM: case ???"
#define	SMT_E0102	SMT_EBASE + 2
#define	SMT_E0102_MSG	"CEM A: illegal state"
#define	SMT_E0103	SMT_EBASE + 3
#define	SMT_E0103_MSG	"CEM B: illegal state"
#define	SMT_E0104	SMT_EBASE + 4
#define	SMT_E0104_MSG	"CEM M: illegal state"
#define	SMT_E0105	SMT_EBASE + 5
#define	SMT_E0105_MSG	"CEM S: illegal state"
#define	SMT_E0106	SMT_EBASE + 6
#define	SMT_E0106_MSG	"CFM : illegal state"
#define	SMT_E0107	SMT_EBASE + 7
#define	SMT_E0107_MSG	"ECM : illegal state"
#define	SMT_E0108	SMT_EBASE + 8
#define	SMT_E0108_MSG	"prop_actions : NAC in DAS CFM"
#define	SMT_E0109	SMT_EBASE + 9
#define	SMT_E0109_MSG	"ST2U.FM_SERRSF error in special frame"
#define	SMT_E0110	SMT_EBASE + 10
#define	SMT_E0110_MSG	"ST2U.FM_SRFRCTOV recv. count. overflow"
#define	SMT_E0111	SMT_EBASE + 11
#define	SMT_E0111_MSG	"ST2U.FM_SNFSLD NP & FORMAC simult. load"
#define	SMT_E0112	SMT_EBASE + 12
#define	SMT_E0112_MSG	"ST2U.FM_SRCVFRM single-frame recv.-mode"
#define	SMT_E0113	SMT_EBASE + 13
#define	SMT_E0113_MSG	"FPLUS: Buffer Memory Error"
#define	SMT_E0114	SMT_EBASE + 14
#define	SMT_E0114_MSG	"ST2U.FM_SERRSF error in special frame"
#define	SMT_E0115	SMT_EBASE + 15
#define	SMT_E0115_MSG	"ST3L: parity error in receive queue 2"
#define	SMT_E0116	SMT_EBASE + 16
#define	SMT_E0116_MSG	"ST3L: parity error in receive queue 1"
#define	SMT_E0117	SMT_EBASE + 17
#define	SMT_E0117_MSG	"E_SMT_001: RxD count for receive queue 1 = 0"
#define	SMT_E0118	SMT_EBASE + 18
#define	SMT_E0118_MSG	"PCM : illegal state"
#define	SMT_E0119	SMT_EBASE + 19
#define	SMT_E0119_MSG	"smt_add_para"
#define	SMT_E0120	SMT_EBASE + 20
#define	SMT_E0120_MSG	"smt_set_para"
#define	SMT_E0121	SMT_EBASE + 21
#define	SMT_E0121_MSG	"illegal event in dispatcher"
#define	SMT_E0122	SMT_EBASE + 22
#define	SMT_E0122_MSG	"RMT : illegal state"
#define	SMT_E0123	SMT_EBASE + 23
#define	SMT_E0123_MSG	"SBA: state machine has illegal state"
#define	SMT_E0124	SMT_EBASE + 24
#define	SMT_E0124_MSG	"sba_free_session() called with NULL pointer"
#define	SMT_E0125	SMT_EBASE + 25
#define	SMT_E0125_MSG	"SBA : illegal session pointer"
#define	SMT_E0126	SMT_EBASE + 26
#define	SMT_E0126_MSG	"smt_free_mbuf() called with NULL pointer\n"
#define	SMT_E0127	SMT_EBASE + 27
#define	SMT_E0127_MSG	"sizeof evcs"
#define	SMT_E0128	SMT_EBASE + 28
#define	SMT_E0128_MSG	"evc->evc_cond_state = 0"
#define	SMT_E0129	SMT_EBASE + 29
#define	SMT_E0129_MSG	"evc->evc_multiple = 0"
#define	SMT_E0130	SMT_EBASE + 30
#define	SMT_E0130_MSG	write_mdr_warning
#define	SMT_E0131	SMT_EBASE + 31
#define	SMT_E0131_MSG	cam_warning
#define SMT_E0132	SMT_EBASE + 32
#define SMT_E0132_MSG	"ST1L.FM_SPCEPDx parity/coding error"
#define SMT_E0133	SMT_EBASE + 33
#define SMT_E0133_MSG	"ST1L.FM_STBURx tx buffer underrun"
#define SMT_E0134	SMT_EBASE + 34
#define SMT_E0134_MSG	"ST1L.FM_SPCEPDx parity error"
#define SMT_E0135	SMT_EBASE + 35
#define SMT_E0135_MSG	"RMT: duplicate MAC address detected. Ring left!"
#define SMT_E0136	SMT_EBASE + 36
#define SMT_E0136_MSG	"Elasticity Buffer hang-up"
#define SMT_E0137	SMT_EBASE + 37
#define SMT_E0137_MSG	"SMT: queue overrun"
#define SMT_E0138	SMT_EBASE + 38
#define SMT_E0138_MSG	"RMT: duplicate MAC address detected. Ring NOT left!"
#endif	/* _CMTDEF_ */
