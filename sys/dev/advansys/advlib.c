/*
 * Low level routines for the Advanced Systems Inc. SCSI controllers chips
 *
 * Copyright (c) 1996 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
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
 *      $Id: advlib.c,v 1.1.1.1 1996/10/07 02:07:06 gibbs Exp $
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
#include <sys/systm.h>

#include <machine/clock.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsi_disk.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/advansys/advlib.h>
#include <dev/advansys/advmcode.h>

/*
 * Allowable periods in ns
 */
u_int8_t adv_sdtr_period_tbl[] =
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

struct sdtr_xmsg {
	u_int8_t	msg_type;
	u_int8_t	msg_len;
	u_int8_t	msg_req;
	u_int8_t	xfer_period;
	u_int8_t	req_ack_offset;
	u_int8_t	res;
};

/*
 * Some of the early PCI adapters have problems with
 * async transfers.  Instead try to use an offset of
 * 1.
 */
#define ASYN_SDTR_DATA_FIX 0x41

/* LRAM routines */
static void	 adv_read_lram_16_multi __P((struct adv_softc *adv, u_int16_t s_addr,
					     u_int16_t *buffer, int count));
static void	 adv_write_lram_16_multi __P((struct adv_softc *adv,
					      u_int16_t s_addr, u_int16_t *buffer,
					      int count));
static void	 adv_mset_lram_16 __P((struct adv_softc *adv,
					u_int16_t s_addr, u_int16_t set_value,
				       int count));
static u_int32_t adv_msum_lram_16 __P((struct adv_softc *adv, u_int16_t s_addr, int count));

static int	 adv_write_and_verify_lram_16 __P((struct adv_softc *adv,
						   u_int16_t addr, u_int16_t value));
static u_int32_t adv_read_lram_32 __P((struct adv_softc *adv, u_int16_t addr));


static void	 adv_write_lram_32 __P((struct adv_softc *adv, u_int16_t addr,
					u_int32_t value));
static void	 adv_write_lram_32_multi __P((struct adv_softc *adv, u_int16_t s_addr,
					      u_int32_t *buffer, int count));

/* EEPROM routines */
static u_int16_t adv_read_eeprom_16 __P((struct adv_softc *adv, u_int8_t addr));
static u_int16_t adv_write_eeprom_16 __P((struct adv_softc *adv, u_int8_t addr, u_int16_t value));
static int	 adv_write_eeprom_cmd_reg __P((struct adv_softc *adv, 	u_int8_t cmd_reg));
static int	 adv_set_eeprom_config_once __P((struct adv_softc *adv,
						 struct adv_eeprom_config *eeprom_config));

/* Initialization */
static u_int32_t adv_load_microcode __P((struct adv_softc *adv,
					 u_int16_t s_addr, u_int16_t *mcode_buf,					 u_int16_t mcode_size));
static void	 adv_init_lram __P((struct adv_softc *adv));
static int	 adv_init_microcode_var __P((struct adv_softc *adv));
static void	 adv_init_qlink_var __P((struct adv_softc *adv));

/* Interrupts */
static void	 adv_disable_interrupt __P((struct adv_softc *adv));
static void	 adv_enable_interrupt __P((struct adv_softc *adv));
static void	 adv_toggle_irq_act __P((struct adv_softc *adv));

/* Chip Control */
#if UNUSED
static void	 adv_start_execution __P((struct adv_softc *adv));
#endif
static int	 adv_start_chip __P((struct adv_softc *adv));
static int	 adv_stop_chip __P((struct adv_softc *adv));
static void	 adv_set_chip_ih __P((struct adv_softc *adv,
				      u_int16_t ins_code));
static void	 adv_set_bank __P((struct adv_softc *adv, u_int8_t bank));
#if UNUSED
static u_int8_t  adv_get_chip_scsi_ctrl __P((struct adv_softc *adv));
#endif

/* Queue handling and execution */
static int	 adv_sgcount_to_qcount __P((int sgcount));
static void	 adv_get_q_info __P((struct adv_softc *adv, u_int16_t s_addr, 	u_int16_t *inbuf,
				     int words));
static u_int	 adv_get_num_free_queues __P((struct adv_softc *adv,
					      u_int8_t n_qs));
static u_int8_t  adv_alloc_free_queues __P((struct adv_softc *adv,
					    u_int8_t free_q_head,
					    u_int8_t n_free_q));
static u_int8_t  adv_alloc_free_queue __P((struct adv_softc *adv,
					   u_int8_t free_q_head));
static int	 adv_send_scsi_queue __P((struct adv_softc *adv,
					  struct adv_scsi_q *scsiq,
					  u_int8_t n_q_required));
static void	 adv_put_ready_sg_list_queue __P((struct adv_softc *adv,
						  struct adv_scsi_q *scsiq,
						  u_int8_t q_no));
static void	 adv_put_ready_queue __P((struct adv_softc *adv,
					  struct adv_scsi_q *scsiq,
					  u_int8_t q_no));
static void	 adv_put_scsiq __P((struct adv_softc *adv, u_int16_t s_addr,
				    u_int16_t *buffer, int words));

/* SDTR */
static u_int8_t  adv_msgout_sdtr __P((struct adv_softc *adv,
				      u_int8_t sdtr_period,
				      u_int8_t sdtr_offset));
static u_int8_t  adv_get_card_sync_setting __P((u_int8_t period,
						u_int8_t offset));
static void	 adv_set_chip_sdtr __P((struct adv_softc *adv,
					u_int8_t sdtr_data,
					u_int8_t tid_no));


/* Exported functions first */

u_int8_t
adv_read_lram_8(adv, addr)
	struct adv_softc *adv;
	u_int16_t addr;
	
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
adv_write_lram_8(adv, addr, value)
	struct adv_softc *adv;
	u_int16_t addr;
	u_int8_t value;
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
adv_read_lram_16(adv, addr)
	struct adv_softc *adv;
	u_int16_t addr;
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, addr);
	return (ADV_INW(adv, ADV_LRAM_DATA));
}

void
adv_write_lram_16(adv, addr, value)
	struct adv_softc *adv;
	u_int16_t addr;
	u_int16_t value;
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, addr);
	ADV_OUTW(adv, ADV_LRAM_DATA, value);
}


/*
 * Return the fully qualified board type for the adapter.
 * The chip_revision must be set before this function is called.
 */
void
adv_get_board_type(adv)
	struct adv_softc *adv;
{
	if ((adv->chip_version >= ADV_CHIP_MIN_VER_VL) &&
	    (adv->chip_version <= ADV_CHIP_MAX_VER_VL)) {
		if (((adv->iobase & 0x0C30) == 0x0C30) ||
			((adv->iobase & 0x0C50) == 0x0C50)) {
			adv->type = ADV_EISA;
		} else
			adv->type = ADV_VL;
	} else if ((adv->chip_version >= ADV_CHIP_MIN_VER_ISA) &&
		   (adv->chip_version <= ADV_CHIP_MAX_VER_ISA)) {
		if (adv->chip_version >= ADV_CHIP_MIN_VER_ISA_PNP) {
			adv->type = ADV_ISAPNP;
		} else
			adv->type = ADV_ISA;
	} else if ((adv->chip_version >= ADV_CHIP_MIN_VER_PCI) &&
		   (adv->chip_version <= ADV_CHIP_MAX_VER_PCI)) {
		adv->type = ADV_PCI;
	} else
		panic("adv_get_board_type: Unknown board type encountered");
}

u_int16_t
adv_get_eeprom_config(adv, eeprom_config)
	struct adv_softc *adv;
	struct	  adv_eeprom_config  *eeprom_config;
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
adv_set_eeprom_config(adv, eeprom_config)
	struct adv_softc *adv;
	struct adv_eeprom_config *eeprom_config;
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
adv_reset_chip_and_scsi_bus(adv)
	struct adv_softc *adv;
{
	adv_stop_chip(adv);
	ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_CHIP_RESET | ADV_CC_SCSI_RESET | ADV_CC_HALT);
	DELAY(200 * 1000);

	adv_set_chip_ih(adv, ADV_INS_RFLAG_WTM);
	adv_set_chip_ih(adv, ADV_INS_HALT);

	ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_CHIP_RESET | ADV_CC_HALT);
	ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_HALT);
	DELAY(200 * 1000);
	return (adv_is_chip_halted(adv));
}

int
adv_test_external_lram(adv)
	struct adv_softc* adv;
{
	u_int16_t	q_addr;
	u_int16_t	saved_value;
	int		success;

	success = 0;

	/* XXX Why 241? */
	q_addr = ADV_QNO_TO_QADDR(241);
	saved_value = adv_read_lram_16(adv, q_addr);
	if (adv_write_and_verify_lram_16(adv, q_addr, 0x55AA) == 0) {
		success = 1;
		adv_write_lram_16(adv, q_addr, saved_value);
	}
	return (success);
}


int
adv_init_lram_and_mcode(adv)
	struct adv_softc *adv;
{
	u_int32_t	retval;
	adv_disable_interrupt(adv);

	adv_init_lram(adv);

	retval = adv_load_microcode(adv, 0, (u_int16_t *)adv_mcode, adv_mcode_size);
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
adv_get_chip_irq(adv)
	struct adv_softc *adv;
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
adv_set_chip_irq(adv, irq_no)
	struct adv_softc *adv;
	u_int8_t irq_no;
{
	u_int16_t	cfg_lsw;

	if ((adv->type & ADV_VL) != 0) {
		if (irq_no != 0) {
			if ((irq_no < ADV_MIN_IRQ_NO) || (irq_no > ADV_MAX_IRQ_NO)) {
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

int
adv_execute_scsi_queue(adv, scsiq)
	struct adv_softc *adv;
	struct adv_scsi_q *scsiq;
{
	int		retval;
	u_int		n_q_required;
	int		s;
	u_int32_t	addr;
	u_int8_t	sg_entry_cnt;
	u_int8_t	target_ix;
	u_int8_t	sg_entry_cnt_minus_one;
	u_int8_t	tid_no;
	u_int8_t	sdtr_data;
	u_int32_t	*p_data_addr;
	u_int32_t	*p_data_bcount;

	scsiq->q1.q_no = 0;
	retval = 1;  /* Default to error case */
	target_ix = scsiq->q2.target_ix;
	tid_no = ADV_TIX_TO_TID(target_ix);

	n_q_required = 1;
	
	s = splbio();
	if (scsiq->cdbptr->opcode == REQUEST_SENSE) {
		if (((adv->initiate_sdtr & scsiq->q1.target_id) != 0)
		    && ((adv->sdtr_done & scsiq->q1.target_id) != 0)) {
			int sdtr_index;
			
			sdtr_data = adv_read_lram_8(adv, ADVV_SDTR_DATA_BEG + tid_no);
			sdtr_index = (sdtr_data >> 4);
			adv_msgout_sdtr(adv, adv_sdtr_period_tbl[sdtr_index],
					 (sdtr_data & ADV_SYN_MAX_OFFSET));
			scsiq->q1.cntl |= (QC_MSG_OUT | QC_URGENT);
		}
	}

	if ((scsiq->q1.cntl & QC_SG_HEAD) != 0) {
		sg_entry_cnt = scsiq->sg_head->entry_cnt;
		sg_entry_cnt_minus_one = sg_entry_cnt - 1;

#ifdef DIAGNOSTIC
		if (sg_entry_cnt <= 1) 
			panic("adv_execute_scsi_queue: Queue with QC_SG_HEAD set but %d segs.", sg_entry_cnt);

		if (sg_entry_cnt > ADV_MAX_SG_LIST)
			panic("adv_execute_scsi_queue: Queue with too many segs.");

		if (adv->type & (ADV_ISA | ADV_VL | ADV_EISA)) {
			for (i = 0; i < sg_entry_cnt_minus_one; i++) {
				addr = scsiq->sg_head->sg_list[i].addr +
				       scsiq->sg_head->sg_list[i].bytes;

				if ((addr & 0x0003) != 0)
					panic("adv_execute_scsi_queue: SG with odd address or byte count");
			}
		}
#endif
		p_data_addr = &scsiq->sg_head->sg_list[sg_entry_cnt_minus_one].addr;
		p_data_bcount = &scsiq->sg_head->sg_list[sg_entry_cnt_minus_one].bytes;

		n_q_required = adv_sgcount_to_qcount(sg_entry_cnt);
		scsiq->sg_head->queue_cnt = n_q_required - 1;
	} else {
		p_data_addr = &scsiq->q1.data_addr;
		p_data_bcount = &scsiq->q1.data_cnt;
		n_q_required = 1;
	}

	if (adv->bug_fix_control & ADV_BUG_FIX_ADD_ONE_BYTE) {
		addr = *p_data_addr + *p_data_bcount;
		if ((addr & 0x0003) != 0) {
			/*
			 * XXX Is this extra test (the one on data_cnt) really only supposed to apply
			 * to the non SG case or was it a bug due to code duplication?
			 */
			if ((scsiq->q1.cntl & QC_SG_HEAD) != 0 || (scsiq->q1.data_cnt & 0x01FF) == 0) {
				if ((scsiq->cdbptr->opcode == READ_COMMAND) ||
				    (scsiq->cdbptr->opcode == READ_BIG)) {
					if ((scsiq->q2.tag_code & ADV_TAG_FLAG_ADD_ONE_BYTE) == 0) {
						(*p_data_bcount)++;
						scsiq->q2.tag_code |= ADV_TAG_FLAG_ADD_ONE_BYTE;
					}
				}
				
			}
		}
	}
	
	if ((adv_get_num_free_queues(adv, n_q_required) >= n_q_required)
	    || ((scsiq->q1.cntl & QC_URGENT) != 0))
		retval = adv_send_scsi_queue(adv, scsiq, n_q_required);
	
	splx(s);
	return (retval);
}


u_int8_t
adv_copy_lram_doneq(adv, q_addr, scsiq, max_dma_count)
	struct adv_softc *adv;
	u_int16_t q_addr;
	struct adv_q_done_info *scsiq;
	u_int32_t max_dma_count;
{
	u_int16_t	val;
	u_int8_t	sg_queue_cnt;

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
	scsiq->user_def = (val >> 8) & 0xFF;

	scsiq->remain_bytes = adv_read_lram_32(adv,
					       q_addr + ADV_SCSIQ_DW_REMAIN_XFER_CNT);
	/*
	 * XXX Is this just a safeguard or will the counter really
	 * have bogus upper bits?
	 */
	scsiq->remain_bytes &= max_dma_count;

	return (sg_queue_cnt);
}

int
adv_stop_execution(adv)
	struct	adv_softc *adv;
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
adv_is_chip_halted(adv)
	struct adv_softc *adv;
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
adv_ack_interrupt(adv)
	struct adv_softc *adv;
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
adv_isr_chip_halted(adv)
	struct adv_softc *adv;
{
	u_int16_t	  int_halt_code;
	u_int8_t	  halt_qp;
	u_int16_t	  halt_q_addr;
	u_int8_t	  target_ix;
	u_int8_t	  q_cntl;
	u_int8_t	  tid_no;
	target_bit_vector target_id;
	target_bit_vector scsi_busy;
	u_int8_t	  asyn_sdtr;
	u_int8_t	  sdtr_data;

	int_halt_code = adv_read_lram_16(adv, ADVV_HALTCODE_W);
	halt_qp = adv_read_lram_8(adv, ADVV_CURCDB_B);
	halt_q_addr = ADV_QNO_TO_QADDR(halt_qp);
	target_ix = adv_read_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_TARGET_IX);
	q_cntl = adv_read_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_CNTL);
	tid_no = ADV_TIX_TO_TID(target_ix);
	target_id = ADV_TID_TO_TARGET_ID(tid_no);
	if (adv->needs_async_bug_fix & target_id)
		asyn_sdtr = ASYN_SDTR_DATA_FIX;
	else
		asyn_sdtr = 0;
	if (int_halt_code == ADV_HALT_EXTMSG_IN) {
		struct	sdtr_xmsg sdtr_xmsg;
		int	sdtr_accept;

		adv_read_lram_16_multi(adv, ADVV_MSGIN_BEG,
					(u_int16_t *) &sdtr_xmsg,
					sizeof(sdtr_xmsg) >> 1);
		if ((sdtr_xmsg.msg_type == MSG_EXTENDED) &&
		    (sdtr_xmsg.msg_len == MSG_EXT_SDTR_LEN)) {
			sdtr_accept = TRUE;
			if (sdtr_xmsg.msg_req == MSG_EXT_SDTR) {
				if (sdtr_xmsg.req_ack_offset > ADV_SYN_MAX_OFFSET) {

					sdtr_accept = FALSE;
					sdtr_xmsg.req_ack_offset = ADV_SYN_MAX_OFFSET;
				}
				sdtr_data = adv_get_card_sync_setting(sdtr_xmsg.xfer_period,
								      sdtr_xmsg.req_ack_offset);
				if (sdtr_xmsg.req_ack_offset == 0) {
					q_cntl &= ~QC_MSG_OUT;
					adv->initiate_sdtr &= ~target_id;
					adv->sdtr_done &= ~target_id;
					adv_set_chip_sdtr(adv, asyn_sdtr, tid_no);
				} else if (sdtr_data == 0) {
					q_cntl |= QC_MSG_OUT;
					adv->initiate_sdtr &= ~target_id;
					adv->sdtr_done &= ~target_id;
					adv_set_chip_sdtr(adv, asyn_sdtr, tid_no);
				} else {
					if (sdtr_accept && (q_cntl & QC_MSG_OUT)) {
						q_cntl &= ~QC_MSG_OUT;
						adv->sdtr_done |= target_id;
						adv->initiate_sdtr |= target_id;
						adv->needs_async_bug_fix &= ~target_id;
						adv_set_chip_sdtr(adv, sdtr_data, tid_no);
					} else {

						q_cntl |= QC_MSG_OUT;

						adv_msgout_sdtr(adv,
								sdtr_xmsg.xfer_period,
								sdtr_xmsg.req_ack_offset);
						adv->needs_async_bug_fix &= ~target_id;
						adv_set_chip_sdtr(adv, sdtr_data, tid_no);
						adv->sdtr_done |= target_id;
						adv->initiate_sdtr |= target_id;
					}
				}

				adv_write_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_CNTL, q_cntl);
			}
		}
		/*
		 * XXX Hey, shouldn't we be rejecting any messages we don't understand?
		 *     The old code also did not un-halt the processor if it recieved
		 *     an extended message that it didn't understand.  That didn't
		 *     seem right, so I changed this routine to always un-halt the
		 *     processor at the end.
		 */
	} else if (int_halt_code == ADV_HALT_CHK_CONDITION) {
		u_int8_t	tag_code;
		u_int8_t	q_status;

		q_cntl |= QC_REQ_SENSE;
		if (((adv->initiate_sdtr & target_id) != 0) &&
			((adv->sdtr_done & target_id) != 0)) {

			sdtr_data = adv_read_lram_8(adv, ADVV_SDTR_DATA_BEG + tid_no);
			/* XXX Macrotize the extraction of the index from sdtr_data ??? */
			adv_msgout_sdtr(adv, adv_sdtr_period_tbl[(sdtr_data >> 4) & 0x0F],
					sdtr_data & ADV_SYN_MAX_OFFSET);
			q_cntl |= QC_MSG_OUT;
		}
		adv_write_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_CNTL, q_cntl);

		/* Don't tag request sense commands */
		tag_code = adv_read_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_TAG_CODE);
		tag_code &= ~(MSG_SIMPLE_Q_TAG|MSG_HEAD_OF_Q_TAG|MSG_ORDERED_Q_TAG);
		adv_write_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_TAG_CODE, tag_code);

		q_status = adv_read_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_STATUS);
		q_status |= (QS_READY | QS_BUSY);
		adv_write_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_STATUS, q_status);

		scsi_busy = adv_read_lram_8(adv, ADVV_SCSIBUSY_B);
		scsi_busy &= ~target_id;
		adv_write_lram_8(adv, ADVV_SCSIBUSY_B, scsi_busy);
	} else if (int_halt_code == ADV_HALT_SDTR_REJECTED) {
		struct	sdtr_xmsg out_msg;

		adv_read_lram_16_multi(adv, ADVV_MSGOUT_BEG,
				       (u_int16_t *) &out_msg,
				       sizeof(out_msg)/2);

		if ((out_msg.msg_type == MSG_EXTENDED) &&
			(out_msg.msg_len == MSG_EXT_SDTR_LEN) &&
			(out_msg.msg_req == MSG_EXT_SDTR)) {

			adv->initiate_sdtr &= ~target_id;
			adv->sdtr_done &= ~target_id;
			adv_set_chip_sdtr(adv, asyn_sdtr, tid_no);
		}
		q_cntl &= ~QC_MSG_OUT;
		adv_write_lram_8(adv, halt_q_addr + ADV_SCSIQ_B_CNTL, q_cntl);
	} else if (int_halt_code == ADV_HALT_SS_QUEUE_FULL) {
		u_int8_t	cur_dvc_qng;
		u_int8_t	scsi_status;

		/*
		 * XXX It would be nice if we could push the responsibility for handling
		 *     this situation onto the generic SCSI layer as other drivers do.
		 *     This would be done by completing the command with the status byte
		 *     set to QUEUE_FULL, whereupon it will request that any transactions
		 *     pending on the target that where scheduled after this one be aborted
		 *     (so as to maintain queue ordering) and the number of requests the
		 *     upper level will attempt to send this target will be reduced.
		 *
		 *     With this current strategy, am I guaranteed that once I unbusy the
		 *     target the queued up transactions will be sent in the order they
		 *     were queued?  If the ASC chip does a round-robin on all queued
		 *     transactions looking for queues to run, the order is not guaranteed.
		 */
		scsi_status = adv_read_lram_8(adv, halt_q_addr + ADV_SCSIQ_SCSI_STATUS);
		cur_dvc_qng = adv_read_lram_8(adv, ADV_QADR_BEG + target_ix);
		printf("adv%d: Queue full - target %d, active transactions %d\n", adv->unit,
		       tid_no, cur_dvc_qng);
#if 0
		/* XXX FIX LATER */
		if ((cur_dvc_qng > 0) && (adv->cur_dvc_qng[tid_no] > 0)) {
			scsi_busy = adv_read_lram_8(adv, ADVV_SCSIBUSY_B);
			scsi_busy |= target_id;
			adv_write_lram_8(adv, ADVV_SCSIBUSY_B, scsi_busy);
			asc_dvc->queue_full_or_busy |= target_id;

			if (scsi_status == SS_QUEUE_FULL) {
				if (cur_dvc_qng > ASC_MIN_TAGGED_CMD) {
					cur_dvc_qng -= 1;
					asc_dvc->max_dvc_qng[tid_no] = cur_dvc_qng;

					adv_write_lram_8(adv, ADVV_MAX_DVC_QNG_BEG + tid_no,
							 cur_dvc_qng);
				}
			}
		}
#endif
	}
	adv_write_lram_16(adv, ADVV_HALTCODE_W, 0);
}

/* Internal Routines */

static void
adv_read_lram_16_multi(adv, s_addr, buffer, count)
	struct adv_softc *adv;
	u_int16_t	 s_addr;
	u_int16_t	 *buffer;
	int		 count;
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, s_addr);
	ADV_INSW(adv, ADV_LRAM_DATA, buffer, count);
}

static void
adv_write_lram_16_multi(adv, s_addr, buffer, count)
	struct adv_softc *adv;
	u_int16_t	 s_addr;
	u_int16_t	 *buffer;
	int		 count;
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, s_addr);
	ADV_OUTSW(adv, ADV_LRAM_DATA, buffer, count);
}

static void
adv_mset_lram_16(adv, s_addr, set_value, count)
	struct adv_softc *adv;
	u_int16_t s_addr;
	u_int16_t set_value;
	int count;
{
	int	i;

	ADV_OUTW(adv, ADV_LRAM_ADDR, s_addr);
	for (i = 0; i < count; i++)
		ADV_OUTW(adv, ADV_LRAM_DATA, set_value);
}

static u_int32_t
adv_msum_lram_16(adv, s_addr, count)
	struct adv_softc *adv;
	u_int16_t	 s_addr;
	int		 count;
{
	u_int32_t	sum;
	int		i;

	sum = 0;
	for (i = 0; i < count; i++, s_addr += 2)
		sum += adv_read_lram_16(adv, s_addr);
	return (sum);
}

static int
adv_write_and_verify_lram_16(adv, addr, value)
	struct adv_softc *adv;
	u_int16_t addr;
	u_int16_t value;
{
	int	retval;

	retval = 0;
	ADV_OUTW(adv, ADV_LRAM_ADDR, addr);
	ADV_OUTW(adv, ADV_LRAM_DATA, value);
	ADV_OUTW(adv, ADV_LRAM_ADDR, addr);
	if (value != ADV_INW(adv, ADV_LRAM_DATA))
		retval = 1;
	return (retval);
}

static u_int32_t
adv_read_lram_32(adv, addr)
	struct adv_softc *adv;
	u_int16_t addr;
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
adv_write_lram_32(adv, addr, value)
	struct adv_softc *adv;
	u_int16_t addr;
	u_int32_t value;
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
adv_write_lram_32_multi(adv, s_addr, buffer, count)
	struct adv_softc *adv;
	u_int16_t s_addr;
	u_int32_t *buffer;
	int count;
{
	ADV_OUTW(adv, ADV_LRAM_ADDR, s_addr);
	ADV_OUTSW(adv, ADV_LRAM_DATA, buffer, count * 2);
}

static u_int16_t
adv_read_eeprom_16(adv, addr)
	struct adv_softc *adv;
	u_int8_t addr;
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
adv_write_eeprom_16(adv, addr, value)
	struct adv_softc *adv;
	u_int8_t addr;
	u_int16_t value;
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
adv_write_eeprom_cmd_reg(adv, cmd_reg)
	struct adv_softc *adv;
	u_int8_t cmd_reg;
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
adv_set_eeprom_config_once(adv, eeprom_config)
	struct adv_softc *adv;
	struct adv_eeprom_config *eeprom_config;
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
adv_load_microcode(adv, s_addr, mcode_buf, mcode_size)
	struct adv_softc *adv;
	u_int16_t	 s_addr;
	u_int16_t	 *mcode_buf;
	u_int16_t	 mcode_size;
{
	u_int32_t	chksum;
	u_int16_t	mcode_lram_size;
	u_int16_t	mcode_chksum;

	mcode_lram_size = mcode_size >> 1;
	/* XXX Why zero the memory just before you write the whole thing?? */
	/* adv_mset_lram_16(adv, s_addr, 0, mcode_lram_size);*/
	adv_write_lram_16_multi(adv, s_addr, mcode_buf, mcode_lram_size);

	chksum = adv_msum_lram_16(adv, s_addr, mcode_lram_size);
	mcode_chksum = (u_int16_t)adv_msum_lram_16(adv, ADV_CODE_SEC_BEG,
					  ((mcode_size - s_addr - ADV_CODE_SEC_BEG) >> 1));
	adv_write_lram_16(adv, ADVV_MCODE_CHKSUM_W, mcode_chksum);
	adv_write_lram_16(adv, ADVV_MCODE_SIZE_W, mcode_size);
	return (chksum);
}

static void
adv_init_lram(adv)
	struct adv_softc *adv;
{
	u_int8_t	i;
	u_int16_t	s_addr;

	adv_mset_lram_16(adv, ADV_QADR_BEG, 0,
			 (u_int16_t)((((int)adv->max_openings + 2 + 1) * 64) >> 1));
	
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
adv_init_microcode_var(adv)
	struct adv_softc *adv;
{
	int       i;

	for (i = 0; i <= ADV_MAX_TID; i++) {
		adv_write_lram_8(adv, ADVV_SDTR_DATA_BEG + i,
				 adv->sdtr_data[i]);
	}

	adv_init_qlink_var(adv);

	/* XXX Again, what about wide busses??? */
	adv_write_lram_8(adv, ADVV_DISC_ENABLE_B, adv->disc_enable);
	adv_write_lram_8(adv, ADVV_HOSTSCSI_ID_B, 0x01 << adv->scsi_id);

	/* What are the extra 8 bytes for?? */
	adv_write_lram_32(adv, ADVV_OVERRUN_PADDR_D, vtophys(&(adv->overrun_buf[0])) + 8);

	adv_write_lram_32(adv, ADVV_OVERRUN_BSIZE_D, ADV_OVERRUN_BSIZE - 8);

#if 0
	/* If we're going to print anything, RCS ids are more meaningful */
	mcode_date = adv_read_lram_16(adv, ADVV_MC_DATE_W);
	mcode_version = adv_read_lram_16(adv, ADVV_MC_VER_W);
#endif
	ADV_OUTW(adv, ADV_REG_PROG_COUNTER, ADV_MCODE_START_ADDR);
	if (ADV_INW(adv, ADV_REG_PROG_COUNTER) != ADV_MCODE_START_ADDR) {
		printf("adv%d: Unable to set program counter. Aborting.\n", adv->unit);
		return (1);
	}
	if (adv_start_chip(adv) != 1) {
		printf("adv%d: Unable to start on board processor. Aborting.\n",
		       adv->unit);
		return (1);
	}
	return (0);
}

static void
adv_init_qlink_var(adv)
	struct adv_softc *adv;
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

	adv_write_lram_8(adv, ADVV_CDBCNT_B, 0);

	lram_addr = ADV_QADR_BEG;
	for (i = 0; i < 32; i++, lram_addr += 2)
		adv_write_lram_16(adv, lram_addr, 0);
}
static void
adv_disable_interrupt(adv)
	struct adv_softc *adv;
{
	u_int16_t cfg;

	cfg = ADV_INW(adv, ADV_CONFIG_LSW);
	ADV_OUTW(adv, ADV_CONFIG_LSW, cfg & ~ADV_CFG_LSW_HOST_INT_ON);
}

static void
adv_enable_interrupt(adv)
	struct adv_softc *adv;
{
	u_int16_t cfg;

	cfg = ADV_INW(adv, ADV_CONFIG_LSW);
	ADV_OUTW(adv, ADV_CONFIG_LSW, cfg | ADV_CFG_LSW_HOST_INT_ON);
}

static void
adv_toggle_irq_act(adv)
	struct adv_softc *adv;
{
	ADV_OUTW(adv, ADV_CHIP_STATUS, ADV_CIW_IRQ_ACT);
	ADV_OUTW(adv, ADV_CHIP_STATUS, 0);
}

#if UNUSED
static void
adv_start_execution(adv)
	struct adv_softc *adv;
{
	if (adv_read_lram_8(adv, ADV_STOP_CODE_B) != 0) {
		adv_write_lram_8(adv, ADV_STOP_CODE_B, 0);
	}
}
#endif

static int
adv_start_chip(adv)
	struct adv_softc *adv;
{
	ADV_OUTB(adv, ADV_CHIP_CTRL, 0);
	if ((ADV_INW(adv, ADV_CHIP_STATUS) & ADV_CSW_HALTED) != 0)
		return (0);
	return (1);
}

static int
adv_stop_chip(adv)
	struct adv_softc *adv;
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

static void
adv_set_chip_ih(adv, ins_code)
	struct adv_softc *adv;
	u_int16_t ins_code;
{
	adv_set_bank(adv, 1);
	ADV_OUTW(adv, ADV_REG_IH, ins_code);
	adv_set_bank(adv, 0);
}

static void
adv_set_bank(adv, bank)
	struct adv_softc *adv;
	u_int8_t bank;
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

#if UNUSED
static u_int8_t
adv_get_chip_scsi_ctrl(adv)
	struct	adv_softc *adv;
{
	u_int8_t scsi_ctrl;

	adv_set_bank(adv, 1);
	scsi_ctrl = ADV_INB(adv, ADV_REG_SC);
	adv_set_bank(adv, 0);
	return (scsi_ctrl);
}
#endif

static int
adv_sgcount_to_qcount(sgcount)
	int sgcount;
{
	int	n_sg_list_qs;

	n_sg_list_qs = ((sgcount - 1) / ADV_SG_LIST_PER_Q);
	if (((sgcount - 1) % ADV_SG_LIST_PER_Q) != 0)
		n_sg_list_qs++;
	return (n_sg_list_qs + 1);
}

/*
 * XXX Looks like more padding issues in this routine as well.
 *     There has to be a way to turn this into an insw.
 */
static void
adv_get_q_info(adv, s_addr, inbuf, words)
	struct adv_softc *adv;
	u_int16_t s_addr;
	u_int16_t *inbuf;
	int words;
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
adv_get_num_free_queues(adv, n_qs)
	struct adv_softc *adv;
	u_int8_t n_qs;
{
	u_int	  cur_used_qs;
	u_int	  cur_free_qs;

	if (n_qs == 1)
		cur_used_qs = adv->cur_active +
			      adv->openings_needed +
			      ADV_MIN_FREE_Q;
	else
		cur_used_qs = adv->cur_active +
			      ADV_MIN_FREE_Q;

	if ((cur_used_qs + n_qs) <= adv->max_openings) {
		cur_free_qs = adv->max_openings - cur_used_qs;
		return (cur_free_qs);
	}
	if (n_qs > 1)
		if (n_qs > adv->openings_needed)
			adv->openings_needed = n_qs;
	return (0);
}

static u_int8_t
adv_alloc_free_queues(adv, free_q_head, n_free_q)
	struct adv_softc *adv;
	u_int8_t free_q_head;
	u_int8_t n_free_q;
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
adv_alloc_free_queue(adv, free_q_head)
	struct adv_softc *adv;
	u_int8_t free_q_head;
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
adv_send_scsi_queue(adv, scsiq, n_q_required)
	struct adv_softc *adv;
	struct adv_scsi_q *scsiq;
	u_int8_t n_q_required;
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
		if (n_q_required > 1) {
			/*
			 * Only reset the shortage value when processing
			 * a "normal" request and not error recovery or
			 * other requests that dip into our reserved queues.
			 * Generally speaking, a normal request will need more
			 * than one queue.
			 */
			adv->openings_needed = 0;
		}
		scsiq->q1.q_no = free_q_head;

		/*
		 * Now that we know our Q number, point our sense
		 * buffer pointer to an area below 16M if we are
		 * an ISA adapter.
		 */
		if (adv->sense_buffers != NULL)
			scsiq->q1.sense_addr = (u_int32_t)vtophys(&(adv->sense_buffers[free_q_head]));
		adv_put_ready_sg_list_queue(adv, scsiq, free_q_head);
		adv_write_lram_16(adv, ADVV_FREE_Q_HEAD_W, next_qp);
		adv->cur_active += n_q_required;
		retval = 0;
	}
	return (retval);
}


static void
adv_put_ready_sg_list_queue(adv, scsiq, q_no)
	struct	adv_softc *adv;
	struct 	adv_scsi_q *scsiq;
	u_int8_t q_no;
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
			panic("adv_put_ready_sg_list_queue: ScsiQ with a SG list but only one element");
		if ((scsiq->q1.cntl & QC_SG_HEAD) == 0)
			panic("adv_put_ready_sg_list_queue: ScsiQ with a SG list but QC_SG_HEAD not set");
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
			sg_list_dwords = segs_this_q * 2;
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

			adv_write_lram_16_multi(adv, q_addr + ADV_SCSIQ_SGHD_CPY_BEG,
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
adv_put_ready_queue(adv, scsiq, q_no)
	struct adv_softc *adv;
	struct adv_scsi_q *scsiq;
	u_int8_t q_no;
{
	u_int16_t	q_addr;
	u_int8_t	tid_no;
	u_int8_t	sdtr_data;
	u_int8_t	syn_period_ix;
	u_int8_t	syn_offset;

	if (((adv->initiate_sdtr & scsiq->q1.target_id) != 0) &&
	    ((adv->sdtr_done & scsiq->q1.target_id) == 0)) {

		tid_no = ADV_TIX_TO_TID(scsiq->q2.target_ix);

		sdtr_data = adv_read_lram_8(adv, ADVV_SDTR_DATA_BEG + tid_no);
		syn_period_ix = (sdtr_data >> 4) & (ADV_SYN_XFER_NO - 1);
		syn_offset = sdtr_data & ADV_SYN_MAX_OFFSET;
		adv_msgout_sdtr(adv, adv_sdtr_period_tbl[syn_period_ix],
				 syn_offset);

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
adv_put_scsiq(adv, s_addr, buffer, words)
	struct adv_softc *adv;
	u_int16_t s_addr;
	u_int16_t *buffer;
	int words;
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

static u_int8_t
adv_msgout_sdtr(adv, sdtr_period, sdtr_offset)
	struct adv_softc *adv;
	u_int8_t sdtr_period;
	u_int8_t sdtr_offset;
{
	struct	 sdtr_xmsg sdtr_buf;

	sdtr_buf.msg_type = MSG_EXTENDED;
	sdtr_buf.msg_len = MSG_EXT_SDTR_LEN;
	sdtr_buf.msg_req = MSG_EXT_SDTR;
	sdtr_buf.xfer_period = sdtr_period;
	sdtr_offset &= ADV_SYN_MAX_OFFSET;
	sdtr_buf.req_ack_offset = sdtr_offset;
	adv_write_lram_16_multi(adv, ADVV_MSGOUT_BEG,
				(u_int16_t *) &sdtr_buf,
				sizeof(sdtr_buf) / 2);

	return (adv_get_card_sync_setting(sdtr_period, sdtr_offset));
}

static u_int8_t
adv_get_card_sync_setting(period, offset)
	u_int8_t period;
	u_int8_t offset;
{
	u_int i;

	if (period >= adv_sdtr_period_tbl[0]) {
		for (i = 0; i < sizeof(adv_sdtr_period_tbl); i++) {
			if (period <= adv_sdtr_period_tbl[i])
				return ((adv_sdtr_period_tbl[i] << 4) | offset);
		}
	}
	return (0);
}

static void
adv_set_chip_sdtr(adv, sdtr_data, tid_no)
	struct adv_softc *adv;
	u_int8_t sdtr_data;
	u_int8_t tid_no;
{
	ADV_OUTB(adv, ADV_SYN_OFFSET, sdtr_data);
	adv_write_lram_8(adv, ADVV_SDTR_DONE_BEG + tid_no, sdtr_data);
}
