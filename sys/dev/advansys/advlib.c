/*
 * Low level routines for the Advanced Systems Inc. SCSI controllers chips
 *
 * Copyright (c) 1996-1997, 1999-2000 Justin Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
/*
 * Ported from:
 * advansys.c - Linux Host Driver for AdvanSys SCSI Adapters
 *     
 * Copyright (c) 1995-1996 Advanced System Products, Inc.
 * All Rights Reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h> 
#include <sys/rman.h> 

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_da.h>
#include <cam/scsi/scsi_cd.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/advansys/advansys.h>
#include <dev/advansys/advmcode.h>

struct adv_quirk_entry {
	struct scsi_inquiry_pattern inq_pat;
	u_int8_t quirks;
#define ADV_QUIRK_FIX_ASYN_XFER_ALWAYS	0x01
#define ADV_QUIRK_FIX_ASYN_XFER		0x02
};

static struct adv_quirk_entry adv_quirk_table[] =
{
	{
		{ T_CDROM, SIP_MEDIA_REMOVABLE, "HP", "*", "*" },
		ADV_QUIRK_FIX_ASYN_XFER_ALWAYS|ADV_QUIRK_FIX_ASYN_XFER
	},
	{
		{ T_CDROM, SIP_MEDIA_REMOVABLE, "NEC", "CD-ROM DRIVE", "*" },
		0
	},
	{
		{
		  T_SEQUENTIAL, SIP_MEDIA_REMOVABLE,
		  "TANDBERG", " TDC 36", "*"
		},
		0
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "WANGTEK", "*", "*" },
		0
	},
	{
		{
		  T_PROCESSOR, SIP_MEDIA_REMOVABLE|SIP_MEDIA_FIXED,
		  "*", "*", "*"
		},
		0
	},
	{
		{
		  T_SCANNER, SIP_MEDIA_REMOVABLE|SIP_MEDIA_FIXED,
		  "*", "*", "*"
		},
		0
	},
	{
		/* Default quirk entry */
		{
		  T_ANY, SIP_MEDIA_REMOVABLE|SIP_MEDIA_FIXED,
		  /*vendor*/"*", /*product*/"*", /*revision*/"*"
                }, 
                ADV_QUIRK_FIX_ASYN_XFER,
	}
};

/*
 * Allowable periods in ns
 */
static u_int8_t adv_sdtr_period_tbl[] =
{
	25,
	30,
	35,
	40,
	50,
	60,
	70,
	85
};

static u_int8_t adv_sdtr_period_tbl_ultra[] =
{
	12,
	19,
	25,
	32,
	38,
	44,
	50,
	57,
	63,
	69,
	75,
	82,
	88, 
	94,
	100,
	107
};

struct ext_msg {
	u_int8_t msg_type;
	u_int8_t msg_len;
	u_int8_t msg_req;
	union {
		struct {
			u_int8_t sdtr_xfer_period;
			u_int8_t sdtr_req_ack_offset;
		} sdtr;
		struct {
       			u_int8_t wdtr_width;
		} wdtr;
		struct {
			u_int8_t mdp[4];
		} mdp;
	} u_ext_msg;
	u_int8_t res;
};

#define	xfer_period	u_ext_msg.sdtr.sdtr_xfer_period
#define	req_ack_offset	u_ext_msg.sdtr.sdtr_req_ack_offset
#define	wdtr_width	u_ext_msg.wdtr.wdtr_width
#define	mdp_b3		u_ext_msg.mdp_b3
#define	mdp_b2		u_ext_msg.mdp_b2
#define	mdp_b1		u_ext_msg.mdp_b1
#define	mdp_b0		u_ext_msg.mdp_b0

/*
 * Some of the early PCI adapters have problems with
 * async transfers.  Instead use an offset of 1.
 */
#define ASYN_SDTR_DATA_FIX_PCI_REV_AB 0x41

/* LRAM routines */
static void	 adv_read_lram_16_multi(struct adv_softc *adv, u_int16_t s_addr,
					u_int16_t *buffer, int count);
static void	 adv_write_lram_16_multi(struct adv_softc *adv,
					 u_int16_t s_addr, u_int16_t *buffer,
					 int count);
static void	 adv_mset_lram_16(struct adv_softc *adv, u_int16_t s_addr,
				  u_int16_t set_value, int count);
static u_int32_t adv_msum_lram_16(struct adv_softc *adv, u_int16_t s_addr,
				  int count);

static int	 adv_write_and_verify_lram_16(struct adv_softc *adv,
					      u_int16_t addr, u_int16_t value);
static u_int32_t adv_read_lram_32(struct adv_softc *adv, u_int16_t addr);


static void	 adv_write_lram_32(struct adv_softc *adv, u_int16_t addr,
				   u_int32_t value);
static void	 adv_write_lram_32_multi(struct adv_softc *adv,
					 u_int16_t s_addr, u_int32_t *buffer,
					 int count);

/* EEPROM routines */
static u_int16_t adv_read_eeprom_16(struct adv_softc *adv, u_int8_t addr);
static u_int16_t adv_write_eeprom_16(struct adv_softc *adv, u_int8_t addr,
				     u_int16_t value);
static int	 adv_write_eeprom_cmd_reg(struct adv_softc *adv,
					  u_int8_t cmd_reg);
static int	 adv_set_eeprom_config_once(struct adv_softc *adv,
					    struct adv_eeprom_config *eeconfig);

/* Initialization */
static u_int32_t adv_load_microcode(struct adv_softc *adv, u_int16_t s_addr,
				    u_int16_t *mcode_buf, u_int16_t mcode_size);

static void	 adv_reinit_lram(struct adv_softc *adv);
static void	 adv_init_lram(struct adv_softc *adv);
static int	 adv_init_microcode_var(struct adv_softc *adv);
static void	 adv_init_qlink_var(struct adv_softc *adv);

/* Interrupts */
static void	 adv_disable_interrupt(struct adv_softc *adv);
static void	 adv_enable_interrupt(struct adv_softc *adv);
static void	 adv_toggle_irq_act(struct adv_softc *adv);

/* Chip Control */
static int	 adv_host_req_chip_halt(struct adv_softc *adv);
static void	 adv_set_chip_ih(struct adv_softc *adv, u_int16_t ins_code);
#if UNUSED
static u_int8_t  adv_get_chip_scsi_ctrl(struct adv_softc *adv);
#endif

/* Queue handling and execution */
static __inline int
		 adv_sgcount_to_qcount(int sgcount);

static __inline int
adv_sgcount_to_qcount(int sgcount)
{
	int	n_sg_list_qs;

	n_sg_list_qs = ((sgcount - 1) / ADV_SG_LIST_PER_Q);
	if (((sgcount - 1) % ADV_SG_LIST_PER_Q) != 0)
		n_sg_list_qs++;
	return (n_sg_list_qs + 1);
}

static void	 adv_get_q_info(struct adv_softc *adv, u_int16_t s_addr,
				u_int16_t *inbuf, int words);
static u_int	 adv_get_num_free_queues(struct adv_softc *adv, u_int8_t n_qs);
static u_int8_t  adv_alloc_free_queues(struct adv_softc *adv,
				       u_int8_t free_q_head, u_int8_t n_free_q);
static u_int8_t  adv_alloc_free_queue(struct adv_softc *adv,
				      u_int8_t free_q_head);
static int	 adv_send_scsi_queue(struct adv_softc *adv,
				     struct adv_scsi_q *scsiq,
				     u_int8_t n_q_required);
static void	 adv_put_ready_sg_list_queue(struct adv_softc *adv,
					     struct adv_scsi_q *scsiq,
					     u_int q_no);
static void	 adv_put_ready_queue(struct adv_softc *adv,
				     struct adv_scsi_q *scsiq, u_int q_no);
static void	 adv_put_scsiq(struct adv_softc *adv, u_int16_t s_addr,
			       u_int16_t *buffer, int words);

/* Messages */
static void	 adv_handle_extmsg_in(struct adv_softc *adv,
				      u_int16_t halt_q_addr, u_int8_t q_cntl,
				      target_bit_vector target_id,
				      int tid);
static void	 adv_msgout_sdtr(struct adv_softc *adv, u_int8_t sdtr_period,
				 u_int8_t sdtr_offset);
static void	 adv_set_sdtr_reg_at_id(struct adv_softc *adv, int id,
					u_int8_t sdtr_data);


/* Exported functions first */

void
advasync(void *callback_arg, u_int32_t code, struct cam_path *path, void *arg)
{
	struct adv_softc *adv;

	adv = (struct adv_softc *)callback_arg;
	switch (code) {
	case AC_FOUND_DEVICE:
	{
		struct ccb_getdev *cgd;
		target_bit_vector target_mask;
		int num_entries;
        	caddr_t match;
		struct adv_quirk_entry *entry;
		struct adv_target_transinfo* tinfo;
 
		cgd = (struct ccb_getdev *)arg;

		target_mask = ADV_TID_TO_TARGET_MASK(cgd->ccb_h.target_id);

		num_entries = sizeof(adv_quirk_table)/sizeof(*adv_quirk_table);
		match = cam_quirkmatch((caddr_t)&cgd->inq_data,
				       (caddr_t)adv_quirk_table,
				       num_entries, sizeof(*adv_quirk_table),
				       scsi_inquiry_match);
        
		if (match == NULL)
			panic("advasync: device didn't match wildcard entry!!");

		entry = (struct adv_quirk_entry *)match;

		if (adv->bug_fix_control & ADV_BUG_FIX_ASYN_USE_SYN) {
			if ((entry->quirks & ADV_QUIRK_FIX_ASYN_XFER_ALWAYS)!=0)
				adv->fix_asyn_xfer_always |= target_mask;
			else
				adv->fix_asyn_xfer_always &= ~target_mask;
			/*
			 * We start out life with all bits set and clear them
			 * after we've determined that the fix isn't necessary.
			 * It may well be that we've already cleared a target
			 * before the full inquiry session completes, so don't
			 * gratuitously set a target bit even if it has this
			 * quirk.  But, if the quirk exonerates a device, clear
			 * the bit now.
			 */
			if ((entry->quirks & ADV_QUIRK_FIX_ASYN_XFER) == 0)
				adv->fix_asyn_xfer &= ~target_mask;
		}
		/*
		 * Reset our sync settings now that we've determined
		 * what quirks are in effect for the device.
		 */
		tinfo = &adv->tinfo[cgd->ccb_h.target_id];
		adv_set_syncrate(adv, cgd->ccb_h.path,
				 cgd->ccb_h.target_id,
				 tinfo->current.period,
				 tinfo->current.offset,
				 ADV_TRANS_CUR);
		break;
	}
	case AC_LOST_DEVICE:
	{
		u_int target_mask;

		if (adv->bug_fix_control & ADV_BUG_FIX_ASYN_USE_SYN) {
			target_mask = 0x01 << xpt_path_target_id(path);
			adv->fix_asyn_xfer |= target_mask;
		}

		/*
		 * Revert to async transfers
		 * for the next device.
		 */
		adv_set_syncrate(adv, /*path*/NULL,
				 xpt_path_target_id(path),
				 /*period*/0,
				 /*offset*/0,
				 ADV_TRANS_GOAL|ADV_TRANS_CUR);
	}
	default:
		break;
	}
}

void
adv_set_bank(struct adv_softc *adv, u_int8_t bank)
{
	u_int8_t control;

	/*
	 * Start out with the bank reset to 0
	 */
	control = ADV_INB(adv, ADV_CHIP_CTRL)
		  &  (~(ADV_CC_SINGLE_STEP | ADV_CC_TEST
			| ADV_CC_DIAG | ADV_CC_SCSI_RESET
			| ADV_CC_CHIP_RESET | ADV_CC_BANK_ONE));
	if (bank == 1) {
		control |= ADV_CC_BANK_ONE;
	} else if (bank == 2) {
		control |= ADV_CC_DIAG | ADV_CC_BANK_ONE;
	}
	ADV_OUTB(adv, ADV_CHIP_CTRL, control);
}

u_int8_t
adv_read_lram_8(struct adv_softc *adv, u_int16_t addr)
{
	u_int8_t   byte_data;
	u_int16_t  word_data;

	/*
	 * LRAM is accessed on 16bit boundaries.
	 */
	ADV_OUTW(adv, ADV_LRAM_ADDR, addr & 0xFFFE);
	word_data = ADV_INW(adv, ADV_LRAM_DATA);
	if (addr & 1) {
#if BYTE_ORDER == BIG_ENDIAN
		byte_data = (u_int8_t)(word_data & 0xFF);
#else
		byte_data = (u_int8_t)((word_data >> 8) & 0xFF);
#endif
	} else {
#if BYTE_ORDER == BIG_ENDIAN
		byte_data = (u_int8_t)((word_data >> 8) & 0xFF);
#else		
		byte_data = (u_int8_t)(word_data & 0xFF);
#endif
	}
	return (byte_data);
}

void
adv_write_lram_8(struct adv_softc *adv, u_int16_t addr, u_int8_t value)
{
	u_int16_t word_data;

	word_data = adv_read_lram_16(adv, addr & 0xFFFE);
	if (addr & 1) {
		word_data &= 0x00FF;
		word_data |= (((u_int8_t)value << 8) & 0xFF00);
	} else {
		word_data &= 0xFF00;
		word_data |= ((u_int8_t)value & 0x00FF);
	}
	adv_write_lram_16(adv, addr & 0xFFFE, word_data);
}


u_int16_t
adv_read_lram_16(struct adv_softc *adv, u_int16_t addr)
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, addr);
	return (ADV_INW(adv, ADV_LRAM_DATA));
}

void
adv_write_lram_16(struct adv_softc *adv, u_int16_t addr, u_int16_t value)
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, addr);
	ADV_OUTW(adv, ADV_LRAM_DATA, value);
}

/*
 * Determine if there is a board at "iobase" by looking
 * for the AdvanSys signatures.  Return 1 if a board is
 * found, 0 otherwise.
 */
int                         
adv_find_signature(bus_space_tag_t tag, bus_space_handle_t bsh)
{                            
	u_int16_t signature;

	if (bus_space_read_1(tag, bsh, ADV_SIGNATURE_BYTE) == ADV_1000_ID1B) {
		signature = bus_space_read_2(tag, bsh, ADV_SIGNATURE_WORD);
		if ((signature == ADV_1000_ID0W)
		 || (signature == ADV_1000_ID0W_FIX))
			return (1);
	}
	return (0);
}

void
adv_lib_init(struct adv_softc *adv)
{
	if ((adv->type & ADV_ULTRA) != 0) {
		adv->sdtr_period_tbl = adv_sdtr_period_tbl_ultra;
		adv->sdtr_period_tbl_size = sizeof(adv_sdtr_period_tbl_ultra);
	} else {
		adv->sdtr_period_tbl = adv_sdtr_period_tbl;
		adv->sdtr_period_tbl_size = sizeof(adv_sdtr_period_tbl);		
	}
}

u_int16_t
adv_get_eeprom_config(struct adv_softc *adv, struct
		      adv_eeprom_config  *eeprom_config)
{
	u_int16_t	sum;
	u_int16_t	*wbuf;
	u_int8_t	cfg_beg;
	u_int8_t	cfg_end;
	u_int8_t	s_addr;

	wbuf = (u_int16_t *)eeprom_config;
	sum = 0;

	for (s_addr = 0; s_addr < 2; s_addr++, wbuf++) {
		*wbuf = adv_read_eeprom_16(adv, s_addr);
		sum += *wbuf;
	}

	if (adv->type & ADV_VL) {
		cfg_beg = ADV_EEPROM_CFG_BEG_VL;
		cfg_end = ADV_EEPROM_MAX_ADDR_VL;
	} else {
		cfg_beg = ADV_EEPROM_CFG_BEG;
		cfg_end = ADV_EEPROM_MAX_ADDR;
	}

	for (s_addr = cfg_beg; s_addr <= (cfg_end - 1); s_addr++, wbuf++) {
		*wbuf = adv_read_eeprom_16(adv, s_addr);
		sum += *wbuf;
#if ADV_DEBUG_EEPROM
		printf("Addr 0x%x: 0x%04x\n", s_addr, *wbuf);
#endif
	}
	*wbuf = adv_read_eeprom_16(adv, s_addr);
	return (sum);
}

int
adv_set_eeprom_config(struct adv_softc *adv,
		      struct adv_eeprom_config *eeprom_config)
{
	int	retry;

	retry = 0;
	while (1) {
		if (adv_set_eeprom_config_once(adv, eeprom_config) == 0) {
			break;
		}
		if (++retry > ADV_EEPROM_MAX_RETRY) {
			break;
		}
	}
	return (retry > ADV_EEPROM_MAX_RETRY);
}

int
adv_reset_chip(struct adv_softc *adv, int reset_bus)
{
	adv_stop_chip(adv);
	ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_CHIP_RESET | ADV_CC_HALT
				     | (reset_bus ? ADV_CC_SCSI_RESET : 0));
	DELAY(60);

	adv_set_chip_ih(adv, ADV_INS_RFLAG_WTM);
	adv_set_chip_ih(adv, ADV_INS_HALT);

	if (reset_bus)
		ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_CHIP_RESET | ADV_CC_HALT);

	ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_HALT);
	if (reset_bus)
		DELAY(200 * 1000);

	ADV_OUTW(adv, ADV_CHIP_STATUS, ADV_CIW_CLR_SCSI_RESET_INT);
	ADV_OUTW(adv, ADV_CHIP_STATUS, 0);
	return (adv_is_chip_halted(adv));
}

int
adv_test_external_lram(struct adv_softc* adv)
{
	u_int16_t	q_addr;
	u_int16_t	saved_value;
	int		success;

	success = 0;

	q_addr = ADV_QNO_TO_QADDR(241);
	saved_value = adv_read_lram_16(adv, q_addr);
	if (adv_write_and_verify_lram_16(adv, q_addr, 0x55AA) == 0) {
		success = 1;
		adv_write_lram_16(adv, q_addr, saved_value);
	}
	return (success);
}


int
adv_init_lram_and_mcode(struct adv_softc *adv)
{
	u_int32_t	retval;

	adv_disable_interrupt(adv);

	adv_init_lram(adv);

	retval = adv_load_microcode(adv, 0, (u_int16_t *)adv_mcode,
				    adv_mcode_size);
	if (retval != adv_mcode_chksum) {
		printf("adv%d: Microcode download failed checksum!\n",
		       adv->unit);
		return (1);
	}
	
	if (adv_init_microcode_var(adv) != 0)
		return (1);

	adv_enable_interrupt(adv);
	return (0);
}

u_int8_t
adv_get_chip_irq(struct adv_softc *adv)
{
	u_int16_t	cfg_lsw;
	u_int8_t	chip_irq;

	cfg_lsw = ADV_INW(adv, ADV_CONFIG_LSW);

	if ((adv->type & ADV_VL) != 0) {
		chip_irq = (u_int8_t)(((cfg_lsw >> 2) & 0x07));
		if ((chip_irq == 0) ||
		    (chip_irq == 4) ||
		    (chip_irq == 7)) {
			return (0);
		}
		return (chip_irq + (ADV_MIN_IRQ_NO - 1));
	}
	chip_irq = (u_int8_t)(((cfg_lsw >> 2) & 0x03));
	if (chip_irq == 3)
		chip_irq += 2;
	return (chip_irq + ADV_MIN_IRQ_NO);
}

u_int8_t
adv_set_chip_irq(struct adv_softc *adv, u_int8_t irq_no)
{
	u_int16_t	cfg_lsw;

	if ((adv->type & ADV_VL) != 0) {
		if (irq_no != 0) {
			if ((irq_no < ADV_MIN_IRQ_NO)
			 || (irq_no > ADV_MAX_IRQ_NO)) {
				irq_no = 0;
			} else {
				irq_no -= ADV_MIN_IRQ_NO - 1;
			}
		}
		cfg_lsw = ADV_INW(adv, ADV_CONFIG_LSW) & 0xFFE3;
		cfg_lsw |= 0x0010;
		ADV_OUTW(adv, ADV_CONFIG_LSW, cfg_lsw);
		adv_toggle_irq_act(adv);

		cfg_lsw = ADV_INW(adv, ADV_CONFIG_LSW) & 0xFFE0;
		cfg_lsw |= (irq_no & 0x07) << 2;
		ADV_OUTW(adv, ADV_CONFIG_LSW, cfg_lsw);
		adv_toggle_irq_act(adv);
	} else if ((adv->type & ADV_ISA) != 0) {
		if (irq_no == 15)
			irq_no -= 2;
		irq_no -= ADV_MIN_IRQ_NO;
		cfg_lsw = ADV_INW(adv, ADV_CONFIG_LSW) & 0xFFF3;
		cfg_lsw |= (irq_no & 0x03) << 2;
		ADV_OUTW(adv, ADV_CONFIG_LSW, cfg_lsw);
	}
	return (adv_get_chip_irq(adv));
}

void
adv_set_chip_scsiid(struct adv_softc *adv, int new_id)
{
	u_int16_t cfg_lsw;

	cfg_lsw = ADV_INW(adv, ADV_CONFIG_LSW);
	if (ADV_CONFIG_SCSIID(cfg_lsw) == new_id)
		return;
    	cfg_lsw &= ~ADV_CFG_LSW_SCSIID;
	cfg_lsw |= (new_id & ADV_MAX_TID) << ADV_CFG_LSW_SCSIID_SHIFT;
	ADV_OUTW(adv, ADV_CONFIG_LSW, cfg_lsw);
}

int
adv_execute_scsi_queue(struct adv_softc *adv, struct adv_scsi_q *scsiq,
		       u_int32_t datalen)
{
	struct		adv_target_transinfo* tinfo;
	u_int32_t	*p_data_addr;
	u_int32_t	*p_data_bcount;
	int		disable_syn_offset_one_fix;
	int		retval;
	u_int		n_q_required;
	u_int32_t	addr;
	u_int8_t	sg_entry_cnt;
	u_int8_t	target_ix;
	u_int8_t	sg_entry_cnt_minus_one;
	u_int8_t	tid_no;

	scsiq->q1.q_no = 0;
	retval = 1;  /* Default to error case */
	target_ix = scsiq->q2.target_ix;
	tid_no = ADV_TIX_TO_TID(target_ix);
	tinfo = &adv->tinfo[tid_no];

	if (scsiq->cdbptr[0] == REQUEST_SENSE) {
		/* Renegotiate if appropriate. */
		adv_set_syncrate(adv, /*struct cam_path */NULL,
				 tid_no, /*period*/0, /*offset*/0,
				 ADV_TRANS_CUR);
		if (tinfo->current.period != tinfo->goal.period) {
			adv_msgout_sdtr(adv, tinfo->goal.period,
					tinfo->goal.offset);
			scsiq->q1.cntl |= (QC_MSG_OUT | QC_URGENT);
		}
	}

	if ((scsiq->q1.cntl & QC_SG_HEAD) != 0) {
		sg_entry_cnt = scsiq->sg_head->entry_cnt;
		sg_entry_cnt_minus_one = sg_entry_cnt - 1;

#ifdef DIAGNOSTIC
		if (sg_entry_cnt <= 1) 
			panic("adv_execute_scsi_queue: Queue "
			      "with QC_SG_HEAD set but %d segs.", sg_entry_cnt);

		if (sg_entry_cnt > ADV_MAX_SG_LIST)
			panic("adv_execute_scsi_queue: "
			      "Queue with too many segs.");

		if ((adv->type & (ADV_ISA | ADV_VL | ADV_EISA)) != 0) {
			int i;

			for (i = 0; i < sg_entry_cnt_minus_one; i++) {
				addr = scsiq->sg_head->sg_list[i].addr +
				       scsiq->sg_head->sg_list[i].bytes;

				if ((addr & 0x0003) != 0)
					panic("adv_execute_scsi_queue: SG "
					      "with odd address or byte count");
			}
		}
#endif
		p_data_addr =
		    &scsiq->sg_head->sg_list[sg_entry_cnt_minus_one].addr;
		p_data_bcount =
		    &scsiq->sg_head->sg_list[sg_entry_cnt_minus_one].bytes;

		n_q_required = adv_sgcount_to_qcount(sg_entry_cnt);
		scsiq->sg_head->queue_cnt = n_q_required - 1;
	} else {
		p_data_addr = &scsiq->q1.data_addr;
		p_data_bcount = &scsiq->q1.data_cnt;
		n_q_required = 1;
	}

	disable_syn_offset_one_fix = FALSE;

	if ((adv->fix_asyn_xfer & scsiq->q1.target_id) != 0
	 && (adv->fix_asyn_xfer_always & scsiq->q1.target_id) == 0) {

		if (datalen != 0) {
			if (datalen < 512) {
				disable_syn_offset_one_fix = TRUE;
			} else {
				if (scsiq->cdbptr[0] == INQUIRY
				 || scsiq->cdbptr[0] == REQUEST_SENSE
				 || scsiq->cdbptr[0] == READ_CAPACITY
				 || scsiq->cdbptr[0] == MODE_SELECT_6 
				 || scsiq->cdbptr[0] == MODE_SENSE_6
				 || scsiq->cdbptr[0] == MODE_SENSE_10 
				 || scsiq->cdbptr[0] == MODE_SELECT_10 
				 || scsiq->cdbptr[0] == READ_TOC) {
					disable_syn_offset_one_fix = TRUE;
				}
			}
		}
	}

	if (disable_syn_offset_one_fix) {
		scsiq->q2.tag_code &=
		    ~(MSG_SIMPLE_Q_TAG|MSG_HEAD_OF_Q_TAG|MSG_ORDERED_Q_TAG);
		scsiq->q2.tag_code |= (ADV_TAG_FLAG_DISABLE_ASYN_USE_SYN_FIX
				     | ADV_TAG_FLAG_DISABLE_DISCONNECT);
	}

	if ((adv->bug_fix_control & ADV_BUG_FIX_IF_NOT_DWB) != 0
	 && (scsiq->cdbptr[0] == READ_10 || scsiq->cdbptr[0] == READ_6)) {
		u_int8_t extra_bytes;

		addr = *p_data_addr + *p_data_bcount;
		extra_bytes = addr & 0x0003;
		if (extra_bytes != 0
		 && ((scsiq->q1.cntl & QC_SG_HEAD) != 0
		  || (scsiq->q1.data_cnt & 0x01FF) == 0)) {
			scsiq->q2.tag_code |= ADV_TAG_FLAG_EXTRA_BYTES;
			scsiq->q1.extra_bytes = extra_bytes;
			*p_data_bcount -= extra_bytes;
		}
	}

	if ((adv_get_num_free_queues(adv, n_q_required) >= n_q_required)
	 || ((scsiq->q1.cntl & QC_URGENT) != 0))
		retval = adv_send_scsi_queue(adv, scsiq, n_q_required);
	
	return (retval);
}


u_int8_t
adv_copy_lram_doneq(struct adv_softc *adv, u_int16_t q_addr,
		    struct adv_q_done_info *scsiq, u_int32_t max_dma_count)
{
	u_int16_t val;
	u_int8_t  sg_queue_cnt;

	adv_get_q_info(adv, q_addr + ADV_SCSIQ_DONE_INFO_BEG,
		       (u_int16_t *)scsiq,
		       (sizeof(scsiq->d2) + sizeof(scsiq->d3)) / 2);

#if BYTE_ORDER == BIG_ENDIAN
	adv_adj_endian_qdone_info(scsiq);
#endif

	val = adv_read_lram_16(adv, q_addr + ADV_SCSIQ_B_STATUS);
	scsiq->q_status = val & 0xFF;
	scsiq->q_no = (val >> 8) & 0XFF;

	val = adv_read_lram_16(adv, q_addr + ADV_SCSIQ_B_CNTL);
	scsiq->cntl = val & 0xFF;
	sg_queue_cnt = (val >> 8) & 0xFF;

	val = adv_read_lram_16(adv,q_addr + ADV_SCSIQ_B_SENSE_LEN);
	scsiq->sense_len = val & 0xFF;
	scsiq->extra_bytes = (val >> 8) & 0xFF;

	/*
	 * Due to a bug in accessing LRAM on the 940UA, the residual
	 * is split into separate high and low 16bit quantities.
	 */
	scsiq->remain_bytes =
	    adv_read_lram_16(adv, q_addr + ADV_SCSIQ_DW_REMAIN_XFER_CNT);
	scsiq->remain_bytes |=
	    adv_read_lram_16(adv, q_addr + ADV_SCSIQ_W_ALT_DC1) << 16;

	/*
	 * XXX Is this just a safeguard or will the counter really
	 * have bogus upper bits?
	 */
	scsiq->remain_bytes &= max_dma_count;

	return (sg_queue_cnt);
}

int
adv_start_chip(struct adv_softc *adv)
{
	ADV_OUTB(adv, ADV_CHIP_CTRL, 0);
	if ((ADV_INW(adv, ADV_CHIP_STATUS) & ADV_CSW_HALTED) != 0)
		return (0);
	return (1);
}

int
adv_stop_execution(struct adv_softc *adv)
{
	int count;

	count = 0;
	if (adv_read_lram_8(adv, ADV_STOP_CODE_B) == 0) {
		adv_write_lram_8(adv, ADV_STOP_CODE_B,
				 ADV_STOP_REQ_RISC_STOP);
		do {
			if (adv_read_lram_8(adv, ADV_STOP_CODE_B) &
				ADV_STOP_ACK_RISC_STOP) {
				return (1);
			}
			DELAY(1000);
		} while (count++ < 20);
	}
	return (0);
}

int
adv_is_chip_halted(struct adv_softc *adv)
{
	if ((ADV_INW(adv, ADV_CHIP_STATUS) & ADV_CSW_HALTED) != 0) {
		if ((ADV_INB(adv, ADV_CHIP_CTRL) & ADV_CC_HALT) != 0) {
			return (1);
		}
	}
	return (0);
}

/*
 * XXX The numeric constants and the loops in this routine
 * need to be documented.
 */
void
adv_ack_interrupt(struct adv_softc *adv)
{
	u_int8_t	host_flag;
	u_int8_t	risc_flag;
	int		loop;

	loop = 0;
	do {
		risc_flag = adv_read_lram_8(adv, ADVV_RISC_FLAG_B);
		if (loop++ > 0x7FFF) {
			break;
		}
	} while ((risc_flag & ADV_RISC_FLAG_GEN_INT) != 0);

	host_flag = adv_read_lram_8(adv, ADVV_HOST_FLAG_B);
	adv_write_lram_8(adv, ADVV_HOST_FLAG_B,
			 host_flag | ADV_HOST_FLAG_ACK_INT);

	ADV_OUTW(adv, ADV_CHIP_STATUS, ADV_CIW_INT_ACK);
	loop = 0;
	while (ADV_INW(adv, ADV_CHIP_STATUS) & ADV_CSW_INT_PENDING) {
		ADV_OUTW(adv, ADV_CHIP_STATUS, ADV_CIW_INT_ACK);
		if (loop++ > 3) {
			break;
		}
	}

	adv_write_lram_8(adv, ADVV_HOST_FLAG_B, host_flag);
}

/*
 * Handle all conditions that may halt the chip waiting
 * for us to intervene.
 */
void
adv_isr_chip_halted(struct adv_softc *adv)
{
	u_int16_t	  int_halt_code;
	u_int16_t	  halt_q_addr;
	target_bit_vector target_mask;
	target_bit_vector scsi_busy;
	u_int8_t	  halt_qp;
	u_int8_t	  target_ix;
	u_int8_t	  q_cntl;
	u_int8_t	  tid_no;

	int_halt_code = adv_read_lram_16(adv, ADVV_HALTCODE_W);
	halt_qp = adv_read_lram_8(adv, ADVV_CURCDB_B);
	halt_q_addr = ADV_QNO_TO_QADDR(halt_qp);
	target_ix = adv_read_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_TARGET_IX);
	q_cntl = adv_read_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_CNTL);
	tid_no = ADV_TIX_TO_TID(target_ix);
	target_mask = ADV_TID_TO_TARGET_MASK(tid_no);
	if (int_halt_code == ADV_HALT_DISABLE_ASYN_USE_SYN_FIX) {
		/*
		 * Temporarily disable the async fix by removing
		 * this target from the list of affected targets,
		 * setting our async rate, and then putting us
		 * back into the mask.
		 */
		adv->fix_asyn_xfer &= ~target_mask;
		adv_set_syncrate(adv, /*struct cam_path */NULL,
				 tid_no, /*period*/0, /*offset*/0,
				 ADV_TRANS_ACTIVE);
		adv->fix_asyn_xfer |= target_mask;
	} else if (int_halt_code == ADV_HALT_ENABLE_ASYN_USE_SYN_FIX) {
		adv_set_syncrate(adv, /*struct cam_path */NULL,
				 tid_no, /*period*/0, /*offset*/0,
				 ADV_TRANS_ACTIVE);
	} else if (int_halt_code == ADV_HALT_EXTMSG_IN) {
		adv_handle_extmsg_in(adv, halt_q_addr, q_cntl,
				     target_mask, tid_no);
	} else if (int_halt_code == ADV_HALT_CHK_CONDITION) {
		struct	  adv_target_transinfo* tinfo;
		union	  ccb *ccb;
		u_int32_t cinfo_index;
		u_int8_t  tag_code;
		u_int8_t  q_status;

		tinfo = &adv->tinfo[tid_no];
		q_cntl |= QC_REQ_SENSE;

		/* Renegotiate if appropriate. */
		adv_set_syncrate(adv, /*struct cam_path */NULL,
				 tid_no, /*period*/0, /*offset*/0,
				 ADV_TRANS_CUR);
		if (tinfo->current.period != tinfo->goal.period) {
			adv_msgout_sdtr(adv, tinfo->goal.period,
					tinfo->goal.offset);
			q_cntl |= QC_MSG_OUT;
		}
		adv_write_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_CNTL, q_cntl);

		/* Don't tag request sense commands */
		tag_code = adv_read_lram_8(adv,
					   halt_q_addr + ADV_SCSIQ_B_TAG_CODE);
		tag_code &=
		    ~(MSG_SIMPLE_Q_TAG|MSG_HEAD_OF_Q_TAG|MSG_ORDERED_Q_TAG);

		if ((adv->fix_asyn_xfer & target_mask) != 0
		 && (adv->fix_asyn_xfer_always & target_mask) == 0) {
			tag_code |= (ADV_TAG_FLAG_DISABLE_DISCONNECT
				 | ADV_TAG_FLAG_DISABLE_ASYN_USE_SYN_FIX);
		}
		adv_write_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_TAG_CODE,
				 tag_code);
		q_status = adv_read_lram_8(adv,
					   halt_q_addr + ADV_SCSIQ_B_STATUS);
		q_status |= (QS_READY | QS_BUSY);
		adv_write_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_STATUS,
				 q_status);
		/*
		 * Freeze the devq until we can handle the sense condition.
		 */
		cinfo_index =
		    adv_read_lram_32(adv, halt_q_addr + ADV_SCSIQ_D_CINFO_IDX);
		ccb = adv->ccb_infos[cinfo_index].ccb;
		xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
		ccb->ccb_h.status |= CAM_DEV_QFRZN;
		adv_abort_ccb(adv, tid_no, ADV_TIX_TO_LUN(target_ix),
			      /*ccb*/NULL, CAM_REQUEUE_REQ,
			      /*queued_only*/TRUE);
		scsi_busy = adv_read_lram_8(adv, ADVV_SCSIBUSY_B);
		scsi_busy &= ~target_mask;
		adv_write_lram_8(adv, ADVV_SCSIBUSY_B, scsi_busy);
		/*
		 * Ensure we have enough time to actually
		 * retrieve the sense.
		 */
		untimeout(adv_timeout, (caddr_t)ccb, ccb->ccb_h.timeout_ch);
		ccb->ccb_h.timeout_ch =
		    timeout(adv_timeout, (caddr_t)ccb, 5 * hz);
	} else if (int_halt_code == ADV_HALT_SDTR_REJECTED) {
		struct	ext_msg out_msg;

		adv_read_lram_16_multi(adv, ADVV_MSGOUT_BEG,
				       (u_int16_t *) &out_msg,
				       sizeof(out_msg)/2);

		if ((out_msg.msg_type == MSG_EXTENDED)
		 && (out_msg.msg_len == MSG_EXT_SDTR_LEN)
		 && (out_msg.msg_req == MSG_EXT_SDTR)) {

			/* Revert to Async */
			adv_set_syncrate(adv, /*struct cam_path */NULL,
					 tid_no, /*period*/0, /*offset*/0,
					 ADV_TRANS_GOAL|ADV_TRANS_ACTIVE);
		}
		q_cntl &= ~QC_MSG_OUT;
		adv_write_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_CNTL, q_cntl);
	} else if (int_halt_code == ADV_HALT_SS_QUEUE_FULL) {
		u_int8_t scsi_status;
		union ccb *ccb;
		u_int32_t cinfo_index;
		
		scsi_status = adv_read_lram_8(adv, halt_q_addr
					      + ADV_SCSIQ_SCSI_STATUS);
		cinfo_index =
		    adv_read_lram_32(adv, halt_q_addr + ADV_SCSIQ_D_CINFO_IDX);
		ccb = adv->ccb_infos[cinfo_index].ccb;
		xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
		ccb->ccb_h.status |= CAM_DEV_QFRZN|CAM_SCSI_STATUS_ERROR;
		ccb->csio.scsi_status = SCSI_STATUS_QUEUE_FULL; 
		adv_abort_ccb(adv, tid_no, ADV_TIX_TO_LUN(target_ix),
			      /*ccb*/NULL, CAM_REQUEUE_REQ,
			      /*queued_only*/TRUE);
		scsi_busy = adv_read_lram_8(adv, ADVV_SCSIBUSY_B);
		scsi_busy &= ~target_mask;
		adv_write_lram_8(adv, ADVV_SCSIBUSY_B, scsi_busy);		
	} else {
		printf("Unhandled Halt Code %x\n", int_halt_code);
	}
	adv_write_lram_16(adv, ADVV_HALTCODE_W, 0);
}

void
adv_sdtr_to_period_offset(struct adv_softc *adv,
			  u_int8_t sync_data, u_int8_t *period,
			  u_int8_t *offset, int tid)
{
	if (adv->fix_asyn_xfer & ADV_TID_TO_TARGET_MASK(tid)
	 && (sync_data == ASYN_SDTR_DATA_FIX_PCI_REV_AB)) {
		*period = *offset = 0;
	} else {
		*period = adv->sdtr_period_tbl[((sync_data >> 4) & 0xF)];
		*offset = sync_data & 0xF;
	}
}

void
adv_set_syncrate(struct adv_softc *adv, struct cam_path *path,
		 u_int tid, u_int period, u_int offset, u_int type)
{
	struct adv_target_transinfo* tinfo;
	u_int old_period;
	u_int old_offset;
	u_int8_t sdtr_data;

	tinfo = &adv->tinfo[tid];

	/* Filter our input */
	sdtr_data = adv_period_offset_to_sdtr(adv, &period,
					      &offset, tid);

	old_period = tinfo->current.period;
	old_offset = tinfo->current.offset;

	if ((type & ADV_TRANS_CUR) != 0
	 && ((old_period != period || old_offset != offset)
	  || period == 0 || offset == 0) /*Changes in asyn fix settings*/) {
		int s;
		int halted;

		s = splcam();
		halted = adv_is_chip_halted(adv);
		if (halted == 0)
			/* Must halt the chip first */
			adv_host_req_chip_halt(adv);

		/* Update current hardware settings */
		adv_set_sdtr_reg_at_id(adv, tid, sdtr_data);

		/*
		 * If a target can run in sync mode, we don't need
		 * to check it for sync problems.
		 */
		if (offset != 0)
			adv->fix_asyn_xfer &= ~ADV_TID_TO_TARGET_MASK(tid);

		if (halted == 0)
			/* Start the chip again */
			adv_start_chip(adv);

		splx(s);
		tinfo->current.period = period;
		tinfo->current.offset = offset;

		if (path != NULL) {
			/*
			 * Tell the SCSI layer about the
			 * new transfer parameters.
			 */
			struct	ccb_trans_settings neg;

			neg.sync_period = period;
			neg.sync_offset = offset;
			neg.valid = CCB_TRANS_SYNC_RATE_VALID
				  | CCB_TRANS_SYNC_OFFSET_VALID;
			xpt_setup_ccb(&neg.ccb_h, path, /*priority*/1);
			xpt_async(AC_TRANSFER_NEG, path, &neg);
		}
	}

	if ((type & ADV_TRANS_GOAL) != 0) {
		tinfo->goal.period = period;
		tinfo->goal.offset = offset;
	}

	if ((type & ADV_TRANS_USER) != 0) {
		tinfo->user.period = period;
		tinfo->user.offset = offset;
	}
}

u_int8_t
adv_period_offset_to_sdtr(struct adv_softc *adv, u_int *period,
			  u_int *offset, int tid)
{
	u_int i;
	u_int dummy_offset;
	u_int dummy_period;

	if (offset == NULL) {
		dummy_offset = 0;
		offset = &dummy_offset;
	}

	if (period == NULL) {
		dummy_period = 0;
		period = &dummy_period;
	}

	*offset = MIN(ADV_SYN_MAX_OFFSET, *offset);
	if (*period != 0 && *offset != 0) {
		for (i = 0; i < adv->sdtr_period_tbl_size; i++) {
			if (*period <= adv->sdtr_period_tbl[i]) {
				/*       
				 * When responding to a target that requests
				 * sync, the requested  rate may fall between
				 * two rates that we can output, but still be
				 * a rate that we can receive.  Because of this,
				 * we want to respond to the target with
				 * the same rate that it sent to us even
				 * if the period we use to send data to it
				 * is lower.  Only lower the response period
				 * if we must.
				 */        
				if (i == 0 /* Our maximum rate */)
					*period = adv->sdtr_period_tbl[0];
				return ((i << 4) | *offset);
			}
		}
	}
	
	/* Must go async */
	*period = 0;
	*offset = 0;
	if (adv->fix_asyn_xfer & ADV_TID_TO_TARGET_MASK(tid))
		return (ASYN_SDTR_DATA_FIX_PCI_REV_AB);
	return (0);
}

/* Internal Routines */

static void
adv_read_lram_16_multi(struct adv_softc *adv, u_int16_t s_addr,
		       u_int16_t *buffer, int count)
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, s_addr);
	ADV_INSW(adv, ADV_LRAM_DATA, buffer, count);
}

static void
adv_write_lram_16_multi(struct adv_softc *adv, u_int16_t s_addr,
			u_int16_t *buffer, int count)
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, s_addr);
	ADV_OUTSW(adv, ADV_LRAM_DATA, buffer, count);
}

static void
adv_mset_lram_16(struct adv_softc *adv, u_int16_t s_addr,
		 u_int16_t set_value, int count)
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, s_addr);
	bus_space_set_multi_2(adv->tag, adv->bsh, ADV_LRAM_DATA,
			      set_value, count);
}

static u_int32_t
adv_msum_lram_16(struct adv_softc *adv, u_int16_t s_addr, int count)
{
	u_int32_t	sum;
	int		i;

	sum = 0;
	ADV_OUTW(adv, ADV_LRAM_ADDR, s_addr);
	for (i = 0; i < count; i++)
		sum += ADV_INW(adv, ADV_LRAM_DATA);
	return (sum);
}

static int
adv_write_and_verify_lram_16(struct adv_softc *adv, u_int16_t addr,
			     u_int16_t value)
{
	int	retval;

	retval = 0;
	ADV_OUTW(adv, ADV_LRAM_ADDR, addr);
	ADV_OUTW(adv, ADV_LRAM_DATA, value);
	DELAY(10000);
	ADV_OUTW(adv, ADV_LRAM_ADDR, addr);
	if (value != ADV_INW(adv, ADV_LRAM_DATA))
		retval = 1;
	return (retval);
}

static u_int32_t
adv_read_lram_32(struct adv_softc *adv, u_int16_t addr)
{
	u_int16_t           val_low, val_high;

	ADV_OUTW(adv, ADV_LRAM_ADDR, addr);

#if BYTE_ORDER == BIG_ENDIAN
	val_high = ADV_INW(adv, ADV_LRAM_DATA);
	val_low = ADV_INW(adv, ADV_LRAM_DATA);
#else
	val_low = ADV_INW(adv, ADV_LRAM_DATA);
	val_high = ADV_INW(adv, ADV_LRAM_DATA);
#endif

	return (((u_int32_t)val_high << 16) | (u_int32_t)val_low);
}

static void
adv_write_lram_32(struct adv_softc *adv, u_int16_t addr, u_int32_t value)
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, addr);

#if BYTE_ORDER == BIG_ENDIAN
	ADV_OUTW(adv, ADV_LRAM_DATA, (u_int16_t)((value >> 16) & 0xFFFF));
	ADV_OUTW(adv, ADV_LRAM_DATA, (u_int16_t)(value & 0xFFFF));
#else
	ADV_OUTW(adv, ADV_LRAM_DATA, (u_int16_t)(value & 0xFFFF));
	ADV_OUTW(adv, ADV_LRAM_DATA, (u_int16_t)((value >> 16) & 0xFFFF));
#endif
}

static void
adv_write_lram_32_multi(struct adv_softc *adv, u_int16_t s_addr,
			u_int32_t *buffer, int count)
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, s_addr);
	ADV_OUTSW(adv, ADV_LRAM_DATA, (u_int16_t *)buffer, count * 2);
}

static u_int16_t
adv_read_eeprom_16(struct adv_softc *adv, u_int8_t addr)
{
	u_int16_t read_wval;
	u_int8_t  cmd_reg;

	adv_write_eeprom_cmd_reg(adv, ADV_EEPROM_CMD_WRITE_DISABLE);
	DELAY(1000);
	cmd_reg = addr | ADV_EEPROM_CMD_READ;
	adv_write_eeprom_cmd_reg(adv, cmd_reg);
	DELAY(1000);
	read_wval = ADV_INW(adv, ADV_EEPROM_DATA);
	DELAY(1000);
	return (read_wval);
}

static u_int16_t
adv_write_eeprom_16(struct adv_softc *adv, u_int8_t addr, u_int16_t value)
{
	u_int16_t	read_value;

	read_value = adv_read_eeprom_16(adv, addr);
	if (read_value != value) {
		adv_write_eeprom_cmd_reg(adv, ADV_EEPROM_CMD_WRITE_ENABLE);
		DELAY(1000);
		
		ADV_OUTW(adv, ADV_EEPROM_DATA, value);
		DELAY(1000);

		adv_write_eeprom_cmd_reg(adv, ADV_EEPROM_CMD_WRITE | addr);
		DELAY(20 * 1000);

		adv_write_eeprom_cmd_reg(adv, ADV_EEPROM_CMD_WRITE_DISABLE);
		DELAY(1000);
		read_value = adv_read_eeprom_16(adv, addr);
	}
	return (read_value);
}

static int
adv_write_eeprom_cmd_reg(struct adv_softc *adv, u_int8_t cmd_reg)
{
	u_int8_t read_back;
	int	 retry;

	retry = 0;
	while (1) {
		ADV_OUTB(adv, ADV_EEPROM_CMD, cmd_reg);
		DELAY(1000);
		read_back = ADV_INB(adv, ADV_EEPROM_CMD);
		if (read_back == cmd_reg) {
			return (1);
		}
		if (retry++ > ADV_EEPROM_MAX_RETRY) {
			return (0);
		}
	}
}

static int
adv_set_eeprom_config_once(struct adv_softc *adv,
			   struct adv_eeprom_config *eeprom_config)
{
	int		n_error;
	u_int16_t	*wbuf;
	u_int16_t	sum;
	u_int8_t	s_addr;
	u_int8_t	cfg_beg;
	u_int8_t	cfg_end;

	wbuf = (u_int16_t *)eeprom_config;
	n_error = 0;
	sum = 0;
	for (s_addr = 0; s_addr < 2; s_addr++, wbuf++) {
		sum += *wbuf;
		if (*wbuf != adv_write_eeprom_16(adv, s_addr, *wbuf)) {
			n_error++;
		}
	}
	if (adv->type & ADV_VL) {
		cfg_beg = ADV_EEPROM_CFG_BEG_VL;
		cfg_end = ADV_EEPROM_MAX_ADDR_VL;
	} else {
		cfg_beg = ADV_EEPROM_CFG_BEG;
		cfg_end = ADV_EEPROM_MAX_ADDR;
	}

	for (s_addr = cfg_beg; s_addr <= (cfg_end - 1); s_addr++, wbuf++) {
		sum += *wbuf;
		if (*wbuf != adv_write_eeprom_16(adv, s_addr, *wbuf)) {
			n_error++;
		}
	}
	*wbuf = sum;
	if (sum != adv_write_eeprom_16(adv, s_addr, sum)) {
		n_error++;
	}
	wbuf = (u_int16_t *)eeprom_config;
	for (s_addr = 0; s_addr < 2; s_addr++, wbuf++) {
		if (*wbuf != adv_read_eeprom_16(adv, s_addr)) {
			n_error++;
		}
	}
	for (s_addr = cfg_beg; s_addr <= cfg_end; s_addr++, wbuf++) {
		if (*wbuf != adv_read_eeprom_16(adv, s_addr)) {
			n_error++;
		}
	}
	return (n_error);
}

static u_int32_t
adv_load_microcode(struct adv_softc *adv, u_int16_t s_addr,
		   u_int16_t *mcode_buf, u_int16_t mcode_size)
{
	u_int32_t chksum;
	u_int16_t mcode_lram_size;
	u_int16_t mcode_chksum;

	mcode_lram_size = mcode_size >> 1;
	/* XXX Why zero the memory just before you write the whole thing?? */
	adv_mset_lram_16(adv, s_addr, 0, mcode_lram_size);
	adv_write_lram_16_multi(adv, s_addr, mcode_buf, mcode_lram_size);

	chksum = adv_msum_lram_16(adv, s_addr, mcode_lram_size);
	mcode_chksum = (u_int16_t)adv_msum_lram_16(adv, ADV_CODE_SEC_BEG,
						   ((mcode_size - s_addr
						     - ADV_CODE_SEC_BEG) >> 1));
	adv_write_lram_16(adv, ADVV_MCODE_CHKSUM_W, mcode_chksum);
	adv_write_lram_16(adv, ADVV_MCODE_SIZE_W, mcode_size);
	return (chksum);
}

static void
adv_reinit_lram(struct adv_softc *adv) {
	adv_init_lram(adv);
	adv_init_qlink_var(adv);
}

static void
adv_init_lram(struct adv_softc *adv)
{
	u_int8_t  i;
	u_int16_t s_addr;

	adv_mset_lram_16(adv, ADV_QADR_BEG, 0,
			 (((adv->max_openings + 2 + 1) * 64) >> 1));
	
	i = ADV_MIN_ACTIVE_QNO;
	s_addr = ADV_QADR_BEG + ADV_QBLK_SIZE;

	adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_FWD,	i + 1);
	adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_BWD, adv->max_openings);
	adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_QNO, i);
	i++;
	s_addr += ADV_QBLK_SIZE;
	for (; i < adv->max_openings; i++, s_addr += ADV_QBLK_SIZE) {
		adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_FWD, i + 1);
		adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_BWD, i - 1);
		adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_QNO, i);
	}

	adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_FWD, ADV_QLINK_END);
	adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_BWD, adv->max_openings - 1);
	adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_QNO, adv->max_openings);
	i++;
	s_addr += ADV_QBLK_SIZE;

	for (; i <= adv->max_openings + 3; i++, s_addr += ADV_QBLK_SIZE) {
		adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_FWD, i);
		adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_BWD, i);
		adv_write_lram_8(adv, s_addr + ADV_SCSIQ_B_QNO, i);
	}
}

static int
adv_init_microcode_var(struct adv_softc *adv)
{
	int	 i;

	for (i = 0; i <= ADV_MAX_TID; i++) {
		
		/* Start out async all around */
		adv_set_syncrate(adv, /*path*/NULL,
				 i, 0, 0,
				 ADV_TRANS_GOAL|ADV_TRANS_CUR);
	}

	adv_init_qlink_var(adv);

	adv_write_lram_8(adv, ADVV_DISC_ENABLE_B, adv->disc_enable);
	adv_write_lram_8(adv, ADVV_HOSTSCSI_ID_B, 0x01 << adv->scsi_id);

	adv_write_lram_32(adv, ADVV_OVERRUN_PADDR_D, adv->overrun_physbase);

	adv_write_lram_32(adv, ADVV_OVERRUN_BSIZE_D, ADV_OVERRUN_BSIZE);

	ADV_OUTW(adv, ADV_REG_PROG_COUNTER, ADV_MCODE_START_ADDR);
	if (ADV_INW(adv, ADV_REG_PROG_COUNTER) != ADV_MCODE_START_ADDR) {
		printf("adv%d: Unable to set program counter. Aborting.\n",
		       adv->unit);
		return (1);
	}
	return (0);
}

static void
adv_init_qlink_var(struct adv_softc *adv)
{
	int	  i;
	u_int16_t lram_addr;

	adv_write_lram_8(adv, ADVV_NEXTRDY_B, 1);
	adv_write_lram_8(adv, ADVV_DONENEXT_B, adv->max_openings);

	adv_write_lram_16(adv, ADVV_FREE_Q_HEAD_W, 1);
	adv_write_lram_16(adv, ADVV_DONE_Q_TAIL_W, adv->max_openings);

	adv_write_lram_8(adv, ADVV_BUSY_QHEAD_B,
			 (u_int8_t)((int) adv->max_openings + 1));
	adv_write_lram_8(adv, ADVV_DISC1_QHEAD_B,
			 (u_int8_t)((int) adv->max_openings + 2));

	adv_write_lram_8(adv, ADVV_TOTAL_READY_Q_B, adv->max_openings);

	adv_write_lram_16(adv, ADVV_ASCDVC_ERR_CODE_W, 0);
	adv_write_lram_16(adv, ADVV_HALTCODE_W, 0);
	adv_write_lram_8(adv, ADVV_STOP_CODE_B, 0);
	adv_write_lram_8(adv, ADVV_SCSIBUSY_B, 0);
	adv_write_lram_8(adv, ADVV_WTM_FLAG_B, 0);
	adv_write_lram_8(adv, ADVV_Q_DONE_IN_PROGRESS_B, 0);

	lram_addr = ADV_QADR_BEG;
	for (i = 0; i < 32; i++, lram_addr += 2)
		adv_write_lram_16(adv, lram_addr, 0);
}

static void
adv_disable_interrupt(struct adv_softc *adv)
{
	u_int16_t cfg;

	cfg = ADV_INW(adv, ADV_CONFIG_LSW);
	ADV_OUTW(adv, ADV_CONFIG_LSW, cfg & ~ADV_CFG_LSW_HOST_INT_ON);
}

static void
adv_enable_interrupt(struct adv_softc *adv)
{
	u_int16_t cfg;

	cfg = ADV_INW(adv, ADV_CONFIG_LSW);
	ADV_OUTW(adv, ADV_CONFIG_LSW, cfg | ADV_CFG_LSW_HOST_INT_ON);
}

static void
adv_toggle_irq_act(struct adv_softc *adv)
{
	ADV_OUTW(adv, ADV_CHIP_STATUS, ADV_CIW_IRQ_ACT);
	ADV_OUTW(adv, ADV_CHIP_STATUS, 0);
}

void
adv_start_execution(struct adv_softc *adv)
{
	if (adv_read_lram_8(adv, ADV_STOP_CODE_B) != 0) {
		adv_write_lram_8(adv, ADV_STOP_CODE_B, 0);
	}
}

int
adv_stop_chip(struct adv_softc *adv)
{
	u_int8_t cc_val;

	cc_val = ADV_INB(adv, ADV_CHIP_CTRL)
		 & (~(ADV_CC_SINGLE_STEP | ADV_CC_TEST | ADV_CC_DIAG));
	ADV_OUTB(adv, ADV_CHIP_CTRL, cc_val | ADV_CC_HALT);
	adv_set_chip_ih(adv, ADV_INS_HALT);
	adv_set_chip_ih(adv, ADV_INS_RFLAG_WTM);
	if ((ADV_INW(adv, ADV_CHIP_STATUS) & ADV_CSW_HALTED) == 0) {
		return (0);
	}
	return (1);
}

static int
adv_host_req_chip_halt(struct adv_softc *adv)
{       
	int	 count;
	u_int8_t saved_stop_code;

	if (adv_is_chip_halted(adv))
		return (1);

	count = 0;
	saved_stop_code = adv_read_lram_8(adv, ADVV_STOP_CODE_B);
	adv_write_lram_8(adv, ADVV_STOP_CODE_B,
			 ADV_STOP_HOST_REQ_RISC_HALT | ADV_STOP_REQ_RISC_STOP);
	while (adv_is_chip_halted(adv) == 0
	    && count++ < 2000)
		;

	adv_write_lram_8(adv, ADVV_STOP_CODE_B, saved_stop_code);
	return (count < 2000); 
}

static void
adv_set_chip_ih(struct adv_softc *adv, u_int16_t ins_code)
{
	adv_set_bank(adv, 1);
	ADV_OUTW(adv, ADV_REG_IH, ins_code);
	adv_set_bank(adv, 0);
}

#if UNUSED
static u_int8_t
adv_get_chip_scsi_ctrl(struct adv_softc *adv)
{
	u_int8_t scsi_ctrl;

	adv_set_bank(adv, 1);
	scsi_ctrl = ADV_INB(adv, ADV_REG_SC);
	adv_set_bank(adv, 0);
	return (scsi_ctrl);
}
#endif

/*
 * XXX Looks like more padding issues in this routine as well.
 *     There has to be a way to turn this into an insw.
 */
static void
adv_get_q_info(struct adv_softc *adv, u_int16_t s_addr,
	       u_int16_t *inbuf, int words)
{
	int	i;

	ADV_OUTW(adv, ADV_LRAM_ADDR, s_addr);
	for (i = 0; i < words; i++, inbuf++) {
		if (i == 5) {
			continue;
		}
		*inbuf = ADV_INW(adv, ADV_LRAM_DATA);
	}
}

static u_int
adv_get_num_free_queues(struct adv_softc *adv, u_int8_t n_qs)
{
	u_int	  cur_used_qs;
	u_int	  cur_free_qs;

	cur_used_qs = adv->cur_active + ADV_MIN_FREE_Q;

	if ((cur_used_qs + n_qs) <= adv->max_openings) {
		cur_free_qs = adv->max_openings - cur_used_qs;
		return (cur_free_qs);
	}
	adv->openings_needed = n_qs;
	return (0);
}

static u_int8_t
adv_alloc_free_queues(struct adv_softc *adv, u_int8_t free_q_head,
		      u_int8_t n_free_q)
{
	int i;

	for (i = 0; i < n_free_q; i++) {
		free_q_head = adv_alloc_free_queue(adv, free_q_head);
		if (free_q_head == ADV_QLINK_END)
			break;
	}
	return (free_q_head);
}

static u_int8_t
adv_alloc_free_queue(struct adv_softc *adv, u_int8_t free_q_head)
{
	u_int16_t	q_addr;
	u_int8_t	next_qp;
	u_int8_t	q_status;

	next_qp = ADV_QLINK_END;
	q_addr = ADV_QNO_TO_QADDR(free_q_head);
	q_status = adv_read_lram_8(adv,	q_addr + ADV_SCSIQ_B_STATUS);
	
	if ((q_status & QS_READY) == 0)
		next_qp = adv_read_lram_8(adv, q_addr + ADV_SCSIQ_B_FWD);

	return (next_qp);
}

static int
adv_send_scsi_queue(struct adv_softc *adv, struct adv_scsi_q *scsiq,
		    u_int8_t n_q_required)
{
	u_int8_t	free_q_head;
	u_int8_t	next_qp;
	u_int8_t	tid_no;
	u_int8_t	target_ix;
	int		retval;

	retval = 1;
	target_ix = scsiq->q2.target_ix;
	tid_no = ADV_TIX_TO_TID(target_ix);
	free_q_head = adv_read_lram_16(adv, ADVV_FREE_Q_HEAD_W) & 0xFF;
	if ((next_qp = adv_alloc_free_queues(adv, free_q_head, n_q_required))
	    != ADV_QLINK_END) {
		scsiq->q1.q_no = free_q_head;

		/*
		 * Now that we know our Q number, point our sense
		 * buffer pointer to a bus dma mapped area where
		 * we can dma the data to.
		 */
		scsiq->q1.sense_addr = adv->sense_physbase
		    + ((free_q_head - 1) * sizeof(struct scsi_sense_data));
		adv_put_ready_sg_list_queue(adv, scsiq, free_q_head);
		adv_write_lram_16(adv, ADVV_FREE_Q_HEAD_W, next_qp);
		adv->cur_active += n_q_required;
		retval = 0;
	}
	return (retval);
}


static void
adv_put_ready_sg_list_queue(struct adv_softc *adv, struct adv_scsi_q *scsiq,
			    u_int q_no)
{
	u_int8_t	sg_list_dwords;
	u_int8_t	sg_index, i;
	u_int8_t	sg_entry_cnt;
	u_int8_t	next_qp;
	u_int16_t	q_addr;
	struct		adv_sg_head *sg_head;
	struct		adv_sg_list_q scsi_sg_q;

	sg_head = scsiq->sg_head;

	if (sg_head) {
		sg_entry_cnt = sg_head->entry_cnt - 1;
#ifdef DIAGNOSTIC
		if (sg_entry_cnt == 0)
			panic("adv_put_ready_sg_list_queue: ScsiQ with "
			      "a SG list but only one element");
		if ((scsiq->q1.cntl & QC_SG_HEAD) == 0)
			panic("adv_put_ready_sg_list_queue: ScsiQ with "
			      "a SG list but QC_SG_HEAD not set");
#endif			
		q_addr = ADV_QNO_TO_QADDR(q_no);
		sg_index = 1;
		scsiq->q1.sg_queue_cnt = sg_head->queue_cnt;
		scsi_sg_q.sg_head_qp = q_no;
		scsi_sg_q.cntl = QCSG_SG_XFER_LIST;
		for (i = 0; i < sg_head->queue_cnt; i++) {
			u_int8_t segs_this_q;

			if (sg_entry_cnt > ADV_SG_LIST_PER_Q)
				segs_this_q = ADV_SG_LIST_PER_Q;
			else {
				/* This will be the last segment then */
				segs_this_q = sg_entry_cnt;
				scsi_sg_q.cntl |= QCSG_SG_XFER_END;
			}
			scsi_sg_q.seq_no = i + 1;
			sg_list_dwords = segs_this_q << 1;
			if (i == 0) {
				scsi_sg_q.sg_list_cnt = segs_this_q;
				scsi_sg_q.sg_cur_list_cnt = segs_this_q;
			} else {
				scsi_sg_q.sg_list_cnt = segs_this_q - 1;
				scsi_sg_q.sg_cur_list_cnt = segs_this_q - 1;
			}
			next_qp = adv_read_lram_8(adv, q_addr + ADV_SCSIQ_B_FWD);
			scsi_sg_q.q_no = next_qp;
			q_addr = ADV_QNO_TO_QADDR(next_qp);

			adv_write_lram_16_multi(adv,
						q_addr + ADV_SCSIQ_SGHD_CPY_BEG,
						(u_int16_t *)&scsi_sg_q,
						sizeof(scsi_sg_q) >> 1);
			adv_write_lram_32_multi(adv, q_addr + ADV_SGQ_LIST_BEG,
						(u_int32_t *)&sg_head->sg_list[sg_index],
						sg_list_dwords);
			sg_entry_cnt -= segs_this_q;
			sg_index += ADV_SG_LIST_PER_Q;
		}
	}
	adv_put_ready_queue(adv, scsiq, q_no);
}

static void
adv_put_ready_queue(struct adv_softc *adv, struct adv_scsi_q *scsiq,
		    u_int q_no)
{
	struct		adv_target_transinfo* tinfo;
	u_int		q_addr;
	u_int		tid_no;

	tid_no = ADV_TIX_TO_TID(scsiq->q2.target_ix);
	tinfo = &adv->tinfo[tid_no];
	if ((tinfo->current.period != tinfo->goal.period)
	 || (tinfo->current.offset != tinfo->goal.offset)) {

		adv_msgout_sdtr(adv, tinfo->goal.period, tinfo->goal.offset);
		scsiq->q1.cntl |= QC_MSG_OUT;
	}
	q_addr = ADV_QNO_TO_QADDR(q_no);

	scsiq->q1.status = QS_FREE;

	adv_write_lram_16_multi(adv, q_addr + ADV_SCSIQ_CDB_BEG,
				(u_int16_t *)scsiq->cdbptr,
				scsiq->q2.cdb_len >> 1);

#if BYTE_ORDER == BIG_ENDIAN
	adv_adj_scsiq_endian(scsiq);
#endif

	adv_put_scsiq(adv, q_addr + ADV_SCSIQ_CPY_BEG,
		      (u_int16_t *) &scsiq->q1.cntl,
		      ((sizeof(scsiq->q1) + sizeof(scsiq->q2)) / 2) - 1);

#if CC_WRITE_IO_COUNT
	adv_write_lram_16(adv, q_addr + ADV_SCSIQ_W_REQ_COUNT,
			  adv->req_count);
#endif

#if CC_CLEAR_DMA_REMAIN

	adv_write_lram_32(adv, q_addr + ADV_SCSIQ_DW_REMAIN_XFER_ADDR, 0);
	adv_write_lram_32(adv, q_addr + ADV_SCSIQ_DW_REMAIN_XFER_CNT, 0);
#endif

	adv_write_lram_16(adv, q_addr + ADV_SCSIQ_B_STATUS,
			  (scsiq->q1.q_no << 8) | QS_READY);
}

static void
adv_put_scsiq(struct adv_softc *adv, u_int16_t s_addr,
	      u_int16_t *buffer, int words)
{
	int	i;

	/*
	 * XXX This routine makes *gross* assumptions
	 * about padding in the data structures.
	 * Either the data structures should have explicit
	 * padding members added, or they should have padding
	 * turned off via compiler attributes depending on
	 * which yields better overall performance.  My hunch
	 * would be that turning off padding would be the
	 * faster approach as an outsw is much faster than
	 * this crude loop and accessing un-aligned data
	 * members isn't *that* expensive.  The other choice
	 * would be to modify the ASC script so that the
	 * the adv_scsiq_1 structure can be re-arranged so
	 * padding isn't required.
	 */
	ADV_OUTW(adv, ADV_LRAM_ADDR, s_addr);
	for (i = 0; i < words; i++, buffer++) {
		if (i == 2 || i == 10) {
			continue;
		}
		ADV_OUTW(adv, ADV_LRAM_DATA, *buffer);
	}
}

static void
adv_handle_extmsg_in(struct adv_softc *adv, u_int16_t halt_q_addr,
		     u_int8_t q_cntl, target_bit_vector target_mask,
		     int tid_no)
{
	struct	ext_msg ext_msg;

	adv_read_lram_16_multi(adv, ADVV_MSGIN_BEG, (u_int16_t *) &ext_msg,
			       sizeof(ext_msg) >> 1);
	if ((ext_msg.msg_type == MSG_EXTENDED)
	 && (ext_msg.msg_req == MSG_EXT_SDTR)
	 && (ext_msg.msg_len == MSG_EXT_SDTR_LEN)) {
		union	  ccb *ccb;
		struct	  adv_target_transinfo* tinfo;
		u_int32_t cinfo_index;
		u_int	 period;
		u_int	 offset;
		int	 sdtr_accept;
		u_int8_t orig_offset;

		cinfo_index =
		    adv_read_lram_32(adv, halt_q_addr + ADV_SCSIQ_D_CINFO_IDX);
		ccb = adv->ccb_infos[cinfo_index].ccb;
		tinfo = &adv->tinfo[tid_no];
		sdtr_accept = TRUE;

		orig_offset = ext_msg.req_ack_offset;
		if (ext_msg.xfer_period < tinfo->goal.period) {
                	sdtr_accept = FALSE;
			ext_msg.xfer_period = tinfo->goal.period;
		}

		/* Perform range checking */
		period = ext_msg.xfer_period;
		offset = ext_msg.req_ack_offset;
		adv_period_offset_to_sdtr(adv, &period,  &offset, tid_no);
		ext_msg.xfer_period = period;
		ext_msg.req_ack_offset = offset;
		
		/* Record our current sync settings */
		adv_set_syncrate(adv, ccb->ccb_h.path,
				 tid_no, ext_msg.xfer_period,
				 ext_msg.req_ack_offset,
				 ADV_TRANS_GOAL|ADV_TRANS_ACTIVE);

		/* Offset too high or large period forced async */
		if (orig_offset != ext_msg.req_ack_offset)
			sdtr_accept = FALSE;

		if (sdtr_accept && (q_cntl & QC_MSG_OUT)) {
			/* Valid response to our requested negotiation */
			q_cntl &= ~QC_MSG_OUT;
		} else {
			/* Must Respond */
			q_cntl |= QC_MSG_OUT;
			adv_msgout_sdtr(adv, ext_msg.xfer_period,
					ext_msg.req_ack_offset);
		}

	} else if (ext_msg.msg_type == MSG_EXTENDED
		&& ext_msg.msg_req == MSG_EXT_WDTR
		&& ext_msg.msg_len == MSG_EXT_WDTR_LEN) {

		ext_msg.wdtr_width = 0;
		adv_write_lram_16_multi(adv, ADVV_MSGOUT_BEG,
					(u_int16_t *)&ext_msg,
					sizeof(ext_msg) >> 1);
		q_cntl |= QC_MSG_OUT;
        } else {

		ext_msg.msg_type = MSG_MESSAGE_REJECT;
		adv_write_lram_16_multi(adv, ADVV_MSGOUT_BEG,
					(u_int16_t *)&ext_msg,
					sizeof(ext_msg) >> 1);
		q_cntl |= QC_MSG_OUT;
        }
	adv_write_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_CNTL, q_cntl);
}

static void
adv_msgout_sdtr(struct adv_softc *adv, u_int8_t sdtr_period,
		u_int8_t sdtr_offset)
{
	struct	 ext_msg sdtr_buf;

	sdtr_buf.msg_type = MSG_EXTENDED;
	sdtr_buf.msg_len = MSG_EXT_SDTR_LEN;
	sdtr_buf.msg_req = MSG_EXT_SDTR;
	sdtr_buf.xfer_period = sdtr_period;
	sdtr_offset &= ADV_SYN_MAX_OFFSET;
	sdtr_buf.req_ack_offset = sdtr_offset;
	adv_write_lram_16_multi(adv, ADVV_MSGOUT_BEG,
				(u_int16_t *) &sdtr_buf,
				sizeof(sdtr_buf) / 2);
}

int
adv_abort_ccb(struct adv_softc *adv, int target, int lun, union ccb *ccb,
	      u_int32_t status, int queued_only)
{
	u_int16_t q_addr;
	u_int8_t  q_no;
	struct adv_q_done_info scsiq_buf;
	struct adv_q_done_info *scsiq;
	u_int8_t  target_ix;
	int	  count;

	scsiq = &scsiq_buf;
	target_ix = ADV_TIDLUN_TO_IX(target, lun);
	count = 0;
	for (q_no = ADV_MIN_ACTIVE_QNO; q_no <= adv->max_openings; q_no++) {
		struct adv_ccb_info *ccb_info;
		q_addr = ADV_QNO_TO_QADDR(q_no);

		adv_copy_lram_doneq(adv, q_addr, scsiq, adv->max_dma_count);
		ccb_info = &adv->ccb_infos[scsiq->d2.ccb_index];
		if (((scsiq->q_status & QS_READY) != 0)
		 && ((scsiq->q_status & QS_ABORTED) == 0)
		 && ((scsiq->cntl & QCSG_SG_XFER_LIST) == 0)
		 && (scsiq->d2.target_ix == target_ix)
		 && (queued_only == 0
		  || !(scsiq->q_status & (QS_DISC1|QS_DISC2|QS_BUSY|QS_DONE)))
		 && (ccb == NULL || (ccb == ccb_info->ccb))) {
			union ccb *aborted_ccb;
			struct adv_ccb_info *cinfo;

			scsiq->q_status |= QS_ABORTED;
			adv_write_lram_8(adv, q_addr + ADV_SCSIQ_B_STATUS,
					 scsiq->q_status);
			aborted_ccb = ccb_info->ccb;
			/* Don't clobber earlier error codes */
			if ((aborted_ccb->ccb_h.status & CAM_STATUS_MASK)
			  == CAM_REQ_INPROG)
				aborted_ccb->ccb_h.status |= status;
			cinfo = (struct adv_ccb_info *)
			    aborted_ccb->ccb_h.ccb_cinfo_ptr;
			cinfo->state |= ACCB_ABORT_QUEUED;
			count++;
		}
	}
	return (count);
}

int
adv_reset_bus(struct adv_softc *adv, int initiate_bus_reset)
{
	int count; 
	int i;
	union ccb *ccb;

	i = 200;
	while ((ADV_INW(adv, ADV_CHIP_STATUS) & ADV_CSW_SCSI_RESET_ACTIVE) != 0
	    && i--)
		DELAY(1000);
	adv_reset_chip(adv, initiate_bus_reset);
	adv_reinit_lram(adv);
	for (i = 0; i <= ADV_MAX_TID; i++)
		adv_set_syncrate(adv, NULL, i, /*period*/0,
				 /*offset*/0, ADV_TRANS_CUR);
	ADV_OUTW(adv, ADV_REG_PROG_COUNTER, ADV_MCODE_START_ADDR);

	/* Tell the XPT layer that a bus reset occured */
	if (adv->path != NULL)
		xpt_async(AC_BUS_RESET, adv->path, NULL);

	count = 0;
	while ((ccb = (union ccb *)LIST_FIRST(&adv->pending_ccbs)) != NULL) {
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG)
			ccb->ccb_h.status |= CAM_SCSI_BUS_RESET;
		adv_done(adv, ccb, QD_ABORTED_BY_HOST, 0, 0, 0);
		count++;
	}

	adv_start_chip(adv);
	return (count);
}

static void
adv_set_sdtr_reg_at_id(struct adv_softc *adv, int tid, u_int8_t sdtr_data)
{
	int orig_id;

    	adv_set_bank(adv, 1);
    	orig_id = ffs(ADV_INB(adv, ADV_HOST_SCSIID)) - 1;
    	ADV_OUTB(adv, ADV_HOST_SCSIID, tid);
	if (ADV_INB(adv, ADV_HOST_SCSIID) == (0x01 << tid)) {
		adv_set_bank(adv, 0);
		ADV_OUTB(adv, ADV_SYN_OFFSET, sdtr_data);
	}
    	adv_set_bank(adv, 1);
    	ADV_OUTB(adv, ADV_HOST_SCSIID, orig_id);
	adv_set_bank(adv, 0);
}
