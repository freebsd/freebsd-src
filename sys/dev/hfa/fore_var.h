/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Host protocol control blocks
 *
 */

#ifndef _FORE_VAR_H
#define _FORE_VAR_H

/*
 * Device VCC Entry
 *
 * Contains the common and Fore-specific information for each VCC 
 * which is opened through a Fore device.
 */
struct fore_vcc {
        struct cmn_vcc  fv_cmn;		/* Common VCC stuff */
	Fore_aal	fv_aal;		/* CP version of AAL */
};
typedef struct fore_vcc  Fore_vcc;

#define	fv_next		fv_cmn.cv_next
#define	fv_toku		fv_cmn.cv_toku
#define	fv_upper	fv_cmn.cv_upper
#define	fv_connvc	fv_cmn.cv_connvc
#define	fv_state	fv_cmn.cv_state
#define	fv_flags	fv_cmn.cv_flags

/*
 * VCC Flags
 */
#define	FVF_ACTCMD	0x01		/* Activate command issued */


/*
 * Host Transmit Queue Element
 *
 * Defines the host's view of the CP PDU Transmit Queue
 */
struct h_xmit_queue {
	struct h_xmit_queue	*hxq_next;	/* Next element in queue */
	Xmit_queue	*hxq_cpelem;	/* CP queue element */
	Q_status	*hxq_status;	/* Element status word */
	Xmit_descr	*hxq_descr;	/* Element's transmit descriptor */
	Xmit_descr	*hxq_descr_dma;	/* Element's transmit descriptor */
	Fore_vcc	*hxq_vcc;	/* Data's VCC */
	KBuffer		*hxq_buf;	/* Data's buffer chain head */
	H_dma		hxq_dma[XMIT_MAX_SEGS];	/* DMA addresses for segments */
};
typedef struct h_xmit_queue	H_xmit_queue;



/*
 * Host Receive Queue Element
 *
 * Defines the host's view of the CP PDU Receive Queue
 */
struct h_recv_queue {
	struct h_recv_queue	*hrq_next;	/* Next element in queue */
	Recv_queue	*hrq_cpelem;	/* CP queue element */
	Q_status	*hrq_status;	/* Element status word */
	Recv_descr	*hrq_descr;	/* Element's receive descriptor */
	Recv_descr	*hrq_descr_dma;	/* Element's receive descriptor */
};
typedef struct h_recv_queue	H_recv_queue;



/*
 * Host Buffer Supply Queue Element
 *
 * Defines the host's view of the CP Buffer Supply Queue
 */
struct h_buf_queue {
	struct h_buf_queue	*hbq_next;	/* Next element in queue */
	Buf_queue	*hbq_cpelem;	/* CP queue element */
	Q_status	*hbq_status;	/* Element status word */
	Buf_descr	*hbq_descr;	/* Element's buffer descriptor array */
	Buf_descr	*hbq_descr_dma;	/* Element's buffer descriptor array */
};
typedef struct h_buf_queue	H_buf_queue;



/*
 * Host Command Queue Element
 *
 * Defines the host's view of the CP Command Queue
 */
struct h_cmd_queue {
	struct h_cmd_queue	*hcq_next;	/* Next element in queue */
	Cmd_queue	*hcq_cpelem;	/* CP queue element */
	Q_status	*hcq_status;	/* Element status word */
	Cmd_code	hcq_code;	/* Command code */
	void		*hcq_arg;	/* Command-specific argument */
};
typedef struct h_cmd_queue	H_cmd_queue;



/*
 * Host Buffer Handle
 *
 * For each buffer supplied to the CP, there will be one of these structures
 * embedded into the non-data portion of the buffer.  This will allow us to
 * track which buffers are currently "controlled" by the CP.  The address of
 * this structure will supplied to/returned from the CP as the buffer handle.
 */
struct buf_handle {
	Qelem_t		bh_qelem;	/* Queuing element */
	u_int		bh_type;	/* Buffer type (see below) */
	H_dma		bh_dma;		/* Buffer DMA address */
};
typedef struct buf_handle	Buf_handle;
#define	SIZEOF_Buf_handle	16

/*
 * Buffer Types
 */
#define	BHT_S1_SMALL	1		/* Buffer strategy 1, small */
#define	BHT_S1_LARGE	2		/* Buffer strategy 1, large */
#define	BHT_S2_SMALL	3		/* Buffer strategy 2, small */
#define	BHT_S2_LARGE	4		/* Buffer strategy 2, large */



/*
 * Device Unit Structure
 *
 * Contains all the information for a single device (adapter).
 */
struct fore_unit {
	Cmn_unit	fu_cmn;		/* Common unit stuff */
#ifdef sun
	struct dev_info	*fu_devinfo;	/* Device node for this unit */
#endif
	Fore_reg	*fu_ctlreg;	/* Device control register */
#ifdef FORE_SBUS
	Fore_reg	*fu_intlvl;	/* Interrupt level register */
#endif
#ifdef FORE_PCI
	Fore_reg	*fu_imask;	/* Interrupt mask register */
	Fore_reg	*fu_psr;	/* PCI specific register */
	pcici_t		fu_pcitag;	/* PCI tag */
#endif
	Fore_mem	*fu_ram;	/* Device RAM */
	u_int		fu_ramsize;	/* Size of device RAM */
	Mon960		*fu_mon;	/* Monitor program interface */
	Aali		*fu_aali;	/* Microcode program interface */
	u_int		fu_timer;	/* Watchdog timer value */

	/* Transmit Queue */
	H_xmit_queue	fu_xmit_q[XMIT_QUELEN];	/* Host queue */
	H_xmit_queue	*fu_xmit_head;	/* Queue head */
	H_xmit_queue	*fu_xmit_tail;	/* Queue tail */
	Q_status	*fu_xmit_stat;	/* Status array (host) */
	Q_status	*fu_xmit_statd;	/* Status array (DMA) */

	/* Receive Queue */
	H_recv_queue	fu_recv_q[RECV_QUELEN];	/* Host queue */
	H_recv_queue	*fu_recv_head;	/* Queue head */
	Q_status	*fu_recv_stat;	/* Status array (host) */
	Q_status	*fu_recv_statd;	/* Status array (DMA) */
	Recv_descr	*fu_recv_desc;	/* Descriptor array (host) */
	Recv_descr	*fu_recv_descd;	/* Descriptor array (DMA) */

	/* Buffer Supply Queue - Strategy 1 Small */
	H_buf_queue	fu_buf1s_q[BUF1_SM_QUELEN];	/* Host queue */
	H_buf_queue	*fu_buf1s_head;	/* Queue head */
	H_buf_queue	*fu_buf1s_tail;	/* Queue tail */
	Q_status	*fu_buf1s_stat;	/* Status array (host) */
	Q_status	*fu_buf1s_statd;/* Status array (DMA) */
	Buf_descr	*fu_buf1s_desc;	/* Descriptor array (host) */
	Buf_descr	*fu_buf1s_descd;/* Descriptor array (DMA) */
	Queue_t		fu_buf1s_bq;	/* Queue of supplied buffers */
	u_int		fu_buf1s_cnt;	/* Count of supplied buffers */

	/* Buffer Supply Queue - Strategy 1 Large */
	H_buf_queue	fu_buf1l_q[BUF1_LG_QUELEN];	/* Host queue */
	H_buf_queue	*fu_buf1l_head;	/* Queue head */
	H_buf_queue	*fu_buf1l_tail;	/* Queue tail */
	Q_status	*fu_buf1l_stat;	/* Status array (host) */
	Q_status	*fu_buf1l_statd;/* Status array (DMA) */
	Buf_descr	*fu_buf1l_desc;	/* Descriptor array (host) */
	Buf_descr	*fu_buf1l_descd;/* Descriptor array (DMA) */
	Queue_t		fu_buf1l_bq;	/* Queue of supplied buffers */
	u_int		fu_buf1l_cnt;	/* Count of supplied buffers */

	/* Command Queue */
	H_cmd_queue	fu_cmd_q[CMD_QUELEN];	/* Host queue */
	H_cmd_queue	*fu_cmd_head;	/* Queue head */
	H_cmd_queue	*fu_cmd_tail;	/* Queue tail */
	Q_status	*fu_cmd_stat;	/* Status array (host) */
	Q_status	*fu_cmd_statd;	/* Status array (DMA) */

	Fore_stats	*fu_stats;	/* Device statistics buffer */
	Fore_stats	*fu_statsd;	/* Device statistics buffer (DMA) */
	time_t		fu_stats_time;	/* Last stats request timestamp */
	int		fu_stats_ret;	/* Stats request return code */
#ifdef FORE_PCI
	Fore_prom	*fu_prom;	/* Device PROM buffer */
	Fore_prom	*fu_promd;	/* Device PROM buffer (DMA) */
#endif
	struct callout_handle fu_thandle;	/* Timer handle */
};
typedef struct fore_unit	Fore_unit;

#define	fu_pif		fu_cmn.cu_pif
#define	fu_unit		fu_cmn.cu_unit
#define	fu_flags	fu_cmn.cu_flags
#define	fu_mtu		fu_cmn.cu_mtu
#define	fu_open_vcc	fu_cmn.cu_open_vcc
#define	fu_vcc		fu_cmn.cu_vcc
#define	fu_intrpri	fu_cmn.cu_intrpri
#define	fu_savepri	fu_cmn.cu_savepri
#define	fu_vcc_pool	fu_cmn.cu_vcc_pool
#define	fu_nif_pool	fu_cmn.cu_nif_pool
#define	fu_ioctl	fu_cmn.cu_ioctl
#define	fu_instvcc	fu_cmn.cu_instvcc
#define	fu_openvcc	fu_cmn.cu_openvcc
#define	fu_closevcc	fu_cmn.cu_closevcc
#define	fu_output	fu_cmn.cu_output
#define	fu_config	fu_cmn.cu_config

/*
 * Device flags (in addition to CUF_* flags)
 */
#define	FUF_STATCMD	0x80		/* Statistics request in progress */


/*
 * Macros to access CP memory
 */
#define	CP_READ(x)	ntohl((u_long)(x))
#define	CP_WRITE(x)	htonl((u_long)(x))

#endif	/* _FORE_VAR_H */
