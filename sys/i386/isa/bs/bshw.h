/*	$Id$	*/
/*
 * [NetBSD for NEC PC98 series]
 *  Copyright (c) 1994, 1995, 1996 NetBSD/pc98 porting staff.
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1994, 1995, 1996 Naofumi HONDA.  All rights reserved.
 */

/* NEC port offsets */
#define	BSHW_DEFAULT_PORT	0xcc0
#define	BSHW_IOSZ	5

#define	addr_port	0
#define	stat_port	0
#define	ctrl_port	2
#define	cmd_port	4

#define	BSHW_MAX_OFFSET		12
#define	BSHW_SEL_TIMEOUT	0x80

#define	BSHW_READ		BSR_IOR
#define	BSHW_WRITE		0x0

#define	BSHW_CMD_CHECK(CCB, CAT) (bshw_cmd[(CCB)->cmd[0]] & (CAT))

/*********************************************************
 * static inline declare.
 *********************************************************/
static BS_INLINE void write_wd33c93 __P((struct bs_softc *, u_int, u_int8_t));
static BS_INLINE u_int8_t read_wd33c93 __P((struct bs_softc *, u_int));
static BS_INLINE u_int8_t bshw_get_auxstat __P((struct bs_softc *));
static BS_INLINE u_int8_t bshw_get_busstat __P((struct bs_softc *));
static BS_INLINE u_int8_t bshw_get_status_insat __P((struct bs_softc *));
static BS_INLINE u_int8_t bshw_read_data __P((struct bs_softc *));
static BS_INLINE void bshw_write_data __P((struct bs_softc *, u_int8_t));
static BS_INLINE void bshw_set_count __P((struct bs_softc *, u_int));
static BS_INLINE u_int bshw_get_count __P((struct bs_softc *));
static BS_INLINE void bshw_set_dst_id __P((struct bs_softc *, u_int, u_int));
static BS_INLINE void bshw_set_lun __P((struct bs_softc *, u_int));
static BS_INLINE u_int bshw_get_src_id __P((struct bs_softc *));
static BS_INLINE void bshw_negate_ack __P((struct bs_softc *));
static BS_INLINE void bshw_assert_atn __P((struct bs_softc *));
static BS_INLINE void bshw_assert_select __P((struct bs_softc *));
static BS_INLINE void bshw_start_xfer __P((struct bs_softc *));
static BS_INLINE void bshw_start_sxfer __P((struct bs_softc *));
static BS_INLINE void bshw_cmd_pass __P((struct bs_softc *, u_int));
static BS_INLINE void bshw_start_sat __P((struct bs_softc *, u_int));
static BS_INLINE void bshw_abort_cmd __P((struct bs_softc *));
static BS_INLINE void bshw_set_sync_reg __P((struct bs_softc *, u_int));
static BS_INLINE void bshw_set_poll_trans __P((struct bs_softc *, u_int));
static BS_INLINE void bshw_set_dma_trans __P((struct bs_softc *, u_int));

/*********************************************************
 * global declare
 *********************************************************/
void bs_dma_xfer __P((struct targ_info *, u_int));
void bs_dma_xfer_end __P((struct targ_info *));
void bshw_dmaabort __P((struct bs_softc *, struct targ_info *));

void bshw_adj_syncdata __P((struct syncdata *));
void bshw_set_synchronous __P((struct bs_softc *, struct targ_info *));

void bs_smit_xfer_end __P((struct targ_info *));
void bshw_smitabort __P((struct bs_softc *));

void bshw_setup_ctrl_reg __P((struct bs_softc *, u_int));
int bshw_chip_reset __P((struct bs_softc *));
void bshw_bus_reset __P((struct bs_softc *));
int bshw_board_probe __P((struct bs_softc *, u_int *, u_int *));
void bshw_lock __P((struct bs_softc *));
void bshw_unlock __P((struct bs_softc *));
void bshw_get_syncreg __P((struct bs_softc *));
void bshw_issue_satcmd __P((struct bs_softc *, struct ccb *, int));
void bshw_print_port __P((struct bs_softc *));

void bs_lc_smit_xfer __P((struct targ_info *, u_int));

extern struct dvcfg_hwsel bshw_hwsel;
extern u_int8_t bshw_cmd[];

/*********************************************************
 * hw
 *********************************************************/
struct bshw {
#define	BSHW_SYNC_RELOAD	0x01
#define	BSHW_SMFIFO		0x02
#define	BSHW_DOUBLE_DMACHAN	0x04
	u_int hw_flags;

	u_int sregaddr;

	int ((*dma_init) __P((struct bs_softc *)));
	void ((*dma_start) __P((struct bs_softc *)));
	void ((*dma_stop) __P((struct bs_softc *)));
};

/*********************************************************
 * inline funcs.
 *********************************************************/
/*
 * XXX: If your board does not work well, Please try BS_NEEDS_WEIGHT.
 */
static BS_INLINE void
write_wd33c93(bsc, addr, data)
	struct bs_softc *bsc;
	u_int addr;
	u_int8_t data;
{

	BUS_IOW(addr_port, addr);
	BUS_IOW(ctrl_port, data);
}

static BS_INLINE u_int8_t
read_wd33c93(bsc, addr)
	struct bs_softc *bsc;
	u_int addr;
{

	BUS_IOW(addr_port, addr);
	return BUS_IOR(ctrl_port);
}

/* status */
static BS_INLINE u_int8_t
bshw_get_auxstat(bsc)
	struct bs_softc *bsc;
{

	return BUS_IOR(stat_port);
}

static BS_INLINE u_int8_t
bshw_get_busstat(bsc)
	struct bs_softc *bsc;
{

	return read_wd33c93(bsc, wd3s_stat);
}

static BS_INLINE u_int8_t
bshw_get_status_insat(bsc)
	struct bs_softc *bsc;
{

	return read_wd33c93(bsc, wd3s_lun);
}

/* data */
static BS_INLINE u_int8_t
bshw_read_data(bsc)
	struct bs_softc *bsc;
{

	return read_wd33c93(bsc, wd3s_data);
}

static BS_INLINE void
bshw_write_data(bsc, data)
	struct bs_softc *bsc;
	u_int8_t data;
{

	write_wd33c93(bsc, wd3s_data, data);
}

/* counter */
static BS_INLINE void
bshw_set_count(bsc, count)
	struct bs_softc *bsc;
	u_int count;
{

	BUS_IOW(addr_port, wd3s_cnt);
	BUS_IOW(ctrl_port, count >> 16);
	BUS_IOW(ctrl_port, count >> 8);
	BUS_IOW(ctrl_port, count);
}

static BS_INLINE u_int
bshw_get_count(bsc)
	struct bs_softc *bsc;
{
	u_int count;

	BUS_IOW(addr_port, wd3s_cnt);
	count = (((u_int) BUS_IOR(ctrl_port)) << 16);
	count += (((u_int) BUS_IOR(ctrl_port)) << 8);
	count += ((u_int) BUS_IOR(ctrl_port));
	return count;
}

/* ID */
static BS_INLINE void
bshw_set_lun(bsc, lun)
	struct bs_softc *bsc;
	u_int lun;
{

	write_wd33c93(bsc, wd3s_lun, lun);
}

static BS_INLINE void
bshw_set_dst_id(bsc, target, lun)
	struct bs_softc *bsc;
	u_int target, lun;
{

	write_wd33c93(bsc, wd3s_did, target);
	write_wd33c93(bsc, wd3s_lun, lun);
}

static BS_INLINE u_int
bshw_get_src_id(bsc)
	struct bs_softc *bsc;
{

	return (read_wd33c93(bsc, wd3s_sid) & SIDR_IDM);
}

/* phase */
static BS_INLINE void
bshw_negate_ack(bsc)
	struct bs_softc *bsc;
{

	write_wd33c93(bsc, wd3s_cmd, WD3S_NEGATE_ACK);
}

static BS_INLINE void
bshw_assert_atn(bsc)
	struct bs_softc *bsc;
{

	write_wd33c93(bsc, wd3s_cmd, WD3S_ASSERT_ATN);
}

static BS_INLINE void
bshw_assert_select(bsc)
	struct bs_softc *bsc;
{

	write_wd33c93(bsc, wd3s_cmd, WD3S_SELECT_ATN);
}

static BS_INLINE void
bshw_start_xfer(bsc)
	struct bs_softc *bsc;
{

	write_wd33c93(bsc, wd3s_cmd, WD3S_TFR_INFO);
}

static BS_INLINE void
bshw_start_sxfer(bsc)
	struct bs_softc *bsc;
{

	write_wd33c93(bsc, wd3s_cmd, WD3S_SBT | WD3S_TFR_INFO);
}

static BS_INLINE void
bshw_cmd_pass(bsc, ph)
	struct bs_softc *bsc;
	u_int ph;
{

	write_wd33c93(bsc, wd3s_cph, ph);
}

static BS_INLINE void
bshw_start_sat(bsc, flag)
	struct bs_softc *bsc;
	u_int flag;
{

	write_wd33c93(bsc, wd3s_cmd,
		      (flag ? WD3S_SELECT_ATN_TFR : WD3S_SELECT_NO_ATN_TFR));
}


static BS_INLINE void
bshw_abort_cmd(bsc)
	struct bs_softc *bsc;
{

	write_wd33c93(bsc, wd3s_cmd, WD3S_ABORT);
}

/* transfer mode */
static BS_INLINE void
bshw_set_sync_reg(bsc, val)
	struct bs_softc *bsc;
	u_int val;
{

	write_wd33c93(bsc, wd3s_synch, val);
}

static BS_INLINE void
bshw_set_poll_trans(bsc, flags)
	struct bs_softc *bsc;
	u_int flags;
{

	if (bsc->sc_flags & BSDMATRANSFER)
	{
		bsc->sc_flags &= ~BSDMATRANSFER;
		bshw_setup_ctrl_reg(bsc, flags);
	}
}

static BS_INLINE void
bshw_set_dma_trans(bsc, flags)
	struct bs_softc *bsc;
	u_int flags;
{

	if ((bsc->sc_flags & BSDMATRANSFER) == 0)
	{
		bsc->sc_flags |= BSDMATRANSFER;
		bshw_setup_ctrl_reg(bsc, flags);
	}
}

static BS_INLINE void memcopy __P((void *from, void *to, register size_t len));

static BS_INLINE void
memcopy(from, to, len)
	void *from, *to;
	register size_t len;
{

	len >>= 2;
	__asm __volatile("cld\n\trep\n\tmovsl" : :
			 "S" (from), "D" (to), "c" (len) :
			 "%esi", "%edi", "%ecx");
}
