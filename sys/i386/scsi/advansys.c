/*
 * Generic driver for the Advanced Systems Inc. SCSI controllers
 * Product specific probe and attach routines can be found in:
 * 
 * i386/isa/adv_isa.c	ABP5140, ABP542, ABP5150, ABP842, ABP852
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
 *      $Id: advansys.c,v 1.1.1.1 1996/10/07 02:07:07 gibbs Exp $
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
#include <sys/malloc.h>
#include <sys/buf.h>

#include <machine/clock.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <i386/scsi/advansys.h>

static void	adv_scsi_cmd __P((struct scsi_xfer *xs));
static void	advminphys __P((struct buf *bp));
static timeout_t
                adv_timeout;
static int	adv_qdone __P((struct adv_softc *adv));
static void	adv_done __P((struct adv_softc *adv,
			      struct adv_q_done_info *qdonep));
static int	adv_poll __P((struct adv_softc *ahc, struct scsi_xfer *xs));

struct adv_softc *advsoftcs[NADV];   /* XXX Config should handle this */

static struct scsi_adapter adv_switch =
{
        adv_scsi_cmd,
        advminphys,
	NULL,
	NULL,
        "adv"
};

static void
adv_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct adv_softc *adv;
	struct adv_scsi_q scsiq;
	struct adv_sg_head sghead;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("adv_scsi_cmd\n"));	

	adv = (struct adv_softc *)xs->sc_link->scsibus->adpt_link.adpt_softc;

	/*
	 * Build up the request
	 */
	scsiq.q1.cntl = 0;
	scsiq.q1.sg_queue_cnt = 0;
	scsiq.q1.status = 0;
	scsiq.q1.q_no = 0;
	scsiq.q1.target_id = ADV_TID_TO_TARGET_ID(xs->sc_link->target);
	scsiq.q1.target_lun = xs->sc_link->lun;
	scsiq.q1.sense_addr = (u_int32_t)vtophys(&xs->sense);
	scsiq.q1.sense_len = sizeof(xs->sense);
	scsiq.q1.data_cnt = 0;
	scsiq.q1.data_addr = 0;	
	scsiq.q1.user_def = 0;
	scsiq.q2.xs_ptr = (u_int32_t)xs;
	scsiq.q2.target_ix = ADV_TIDLUN_TO_IX(xs->sc_link->target, xs->sc_link->lun);
	scsiq.q2.flag = 0;
	scsiq.q2.cdb_len = xs->cmdlen;
	scsiq.q2.tag_code = xs->tag_type;
	scsiq.q2.vm_id = 0;
	scsiq.sg_head = NULL;
	scsiq.cdbptr = &xs->cmd;

	if (xs->datalen) {
		/*
		 * Determin the number of segments needed for this
		 * transfer.  We should only use SG if we need more
		 * than one.
		 */
		int		seg;
		u_int32_t	datalen;
		vm_offset_t	vaddr;
		u_int32_t	paddr;
		u_int32_t	nextpaddr;
		struct		adv_sg_entry *sg;

		seg = 0;
		datalen = xs->datalen;
		vaddr = (vm_offset_t)xs->data;
		paddr = vtophys(vaddr);
		sg = &sghead.sg_list[0];

		while ((datalen > 0) && (seg < ADV_MAX_SG_LIST)) {
			/* put in the base address and length */
			sg->addr = paddr;
			sg->bytes = 0;
			
			/* do it at least once */
			nextpaddr = paddr;
			
			while ((datalen > 0) && (paddr == nextpaddr)) {
				u_int32_t	size;
				/*
				 * This page is contiguous (physically)
				 * with the the last, just extend the
				 * length
				 */
				/* how far to the end of the page */
				nextpaddr = (paddr & (~PAGE_MASK)) + PAGE_SIZE;

				/*
				 * Compute the maximum size
				 */
				size = nextpaddr - paddr;
				if (size > datalen)
					size = datalen;

				sg->bytes += size;
				vaddr	  += size;
				datalen   -= size;
				if (datalen > 0)
					paddr = vtophys(vaddr);
			}
			/*
			 * next page isn't contiguous, finish the seg
			 */
			seg++;
			sg++;
		}
		if (seg > 1) {
			scsiq.q1.cntl |= QC_SG_HEAD;
			scsiq.sg_head = &sghead;
			sghead.entry_cnt = sghead.entry_to_copy = seg;
			sghead.res = 0;
		}
		scsiq.q1.data_addr = sghead.sg_list[0].addr;
		scsiq.q1.data_cnt = sghead.sg_list[0].bytes;
	}
	
	if (adv_execute_scsi_queue(adv, &scsiq) != 0) {
		xs->error = XS_QUEUE_RESOURCE_SHORTAGE;
		scsi_done(xs);
	} else if ((xs->flags & SCSI_POLL) != 0) {
		/*
		 * If we can't use interrupts, poll for completion
		 */		
		int s;
		
		s = splbio();
		if (adv_poll(adv, xs)) {
			if (!(xs->flags & SCSI_SILENT))
				printf("cmd fail\n");
			adv_timeout(xs);
		}
		splx(s);
	} 
}


static void
advminphys(bp)
	struct	buf *bp;
{
	if (bp->b_bcount > ((ADV_MAX_SG_LIST - 1) * PAGE_SIZE))
		bp->b_bcount = ((ADV_MAX_SG_LIST - 1) * PAGE_SIZE);
}

static void
adv_timeout(arg)
	void *arg;
{
	printf("adv: Ooops. Had a timeout\n");
}

struct adv_softc *
adv_alloc(unit, iobase)
	int	unit;
	u_long	iobase;
{
        struct  adv_softc *adv;
	int	i;
   
	if (unit >= NADV) {
		printf("adv: unit number (%d) too high\n", unit);
		return NULL;
	}

	/*
	 * Allocate a storage area for us
	 */
	if (advsoftcs[unit]) {
		printf("adv%d: memory already allocated\n", unit);
		return NULL;
	}

	adv = malloc(sizeof(struct adv_softc), M_DEVBUF, M_NOWAIT);
	if (!adv) {
		printf("adv%d: cannot malloc!\n", unit);
		return NULL;
	}
	bzero(adv, sizeof(struct adv_softc));
	advsoftcs[unit] = adv;
	adv->unit = unit;
	adv->iobase = iobase;
	
	/* Set reasonable defaults incase we can't read the EEPROM */
	adv->max_openings = ADV_DEF_MAX_TOTAL_QNG;
        adv->start_motor = TARGET_BIT_VECTOR_SET;
        adv->disc_enable = TARGET_BIT_VECTOR_SET;
	adv->cmd_qng_enabled = TARGET_BIT_VECTOR_SET;
        adv->scsi_id = 7;

	for (i = 0; i <= ADV_MAX_TID; i++)
		adv->sdtr_data[i] = ADV_DEF_SDTR_OFFSET | (ADV_DEF_SDTR_INDEX << 4);

	return(adv);
}

void
adv_free(adv)
	struct adv_softc *adv;
{
	if (adv->sense_buffers != NULL)
		free(adv->sense_buffers, M_DEVBUF);
	free(adv, M_DEVBUF);
}

int
adv_init(adv)
	struct adv_softc *adv;
{
	struct	  adv_eeprom_config eeprom_config;
	int	  checksum, i;
	u_int16_t config_lsw;
	u_int16_t config_msw;

	adv_get_board_type(adv);
	
	/*
	 * Stop script execution.
	 */
	adv_write_lram_16(adv, ADV_HALTCODE_W, 0x00FE);
	adv_stop_execution(adv);
	adv_reset_chip_and_scsi_bus(adv);
	/*
	 * The generic SCSI code does a minimum delay for us
	 * already.
	 */
	/* DELAY(3 * 1000 * 1000);*/	/* 3 Second Delay */
	if (adv_is_chip_halted(adv) == 0) {
		printf("adv%d: Unable to halt adapter. Initialization"
		       "failed\n", adv->unit);
		return (1);
	}
	ADV_OUTW(adv, ADV_REG_PROG_COUNTER, ADV_MCODE_START_ADDR);
	if (ADV_INW(adv, ADV_REG_PROG_COUNTER) != ADV_MCODE_START_ADDR) {
		printf("adv%d: Unable to set program counter. Initialization"
		       "failed\n", adv->unit);
		return (1);
	}

	config_lsw = ADV_INW(adv, ADV_CONFIG_LSW);
	config_msw = ADV_INW(adv, ADV_CONFIG_MSW);

#if 0
	/* XXX Move to PCI probe code */
	if (adv->type & ADV_PCI) {
#if CC_DISABLE_PCI_PARITY_INT
		config_msw &= 0xFFC0;
		ADV_OUTW(adv, ADV_CONFIG_MSW, config_msw);
#endif

		if (asc_dvc->cfg->pci_device_id == ASC_PCI_DEVICE_ID_REV_A) {
			asc_dvc->bug_fix_cntl |= ASC_BUG_FIX_ADD_ONE_BYTE;
		}
	}
#endif		
	if ((config_msw & ADV_CFG_MSW_CLR_MASK) != 0) {
		config_msw &= (~(ADV_CFG_MSW_CLR_MASK));
		/*
		 * XXX The Linux code flags this as an error,
		 * but what should we report to the user???
		 * It seems that clearing the config register
		 * makes this error recoverable.
		 */
		ADV_OUTW(adv, ADV_CONFIG_MSW, config_msw);
	}

	/* Suck in the configuration from the EEProm */
	checksum = adv_get_eeprom_config(adv, &eeprom_config);

	eeprom_config.cfg_msw &= (~(ADV_CFG_MSW_CLR_MASK));

	if (ADV_INW(adv, ADV_CHIP_STATUS) & ADV_CSW_AUTO_CONFIG) {
		/*
		 * XXX The Linux code sets a warning level for this
		 * condition, yet nothing of meaning is printed to
		 * the user.  What does this mean???
		 */
		if (adv->chip_version == 3) {
			if (eeprom_config.cfg_lsw != config_lsw) {
				/* XXX Yet another supposed Warning */
				eeprom_config.cfg_lsw =
						ADV_INW(adv, ADV_CONFIG_LSW);
			}
			if (eeprom_config.cfg_msw != config_msw) {
				/* XXX Yet another supposed Warning */
				eeprom_config.cfg_msw =
						ADV_INW(adv, ADV_CONFIG_MSW);
			}
		}
	}
	eeprom_config.cfg_lsw |= ADV_CFG_LSW_HOST_INT_ON;
	if (checksum == eeprom_config.chksum) {
		if (adv_test_external_lram(adv) == 0) {
			if (adv->type & ADV_PCI) {
				eeprom_config.cfg_msw |= 0x0800;
				config_msw |= 0x0800;
				ADV_OUTW(adv, ADV_CONFIG_MSW, config_msw);
				eeprom_config.max_total_qng = ADV_MAX_PCI_INRAM_TOTAL_QNG;
				eeprom_config.max_tag_qng = ADV_MAX_INRAM_TAG_QNG;
			}
		}
		/* XXX What about wide bussed cards?? */
		for (i = 0; i <= 7; i++)
			adv->sdtr_data[i] = eeprom_config.sdtr_data[i];
		
		/* Range/Sanity checking */
		if (eeprom_config.max_total_qng < ADV_MIN_TOTAL_QNG) {
			eeprom_config.max_total_qng = ADV_MIN_TOTAL_QNG;
		}
		if (eeprom_config.max_total_qng > ADV_MAX_TOTAL_QNG) {
			eeprom_config.max_total_qng = ADV_MAX_TOTAL_QNG;
		}
		if (eeprom_config.max_tag_qng > eeprom_config.max_total_qng) {
			eeprom_config.max_tag_qng = eeprom_config.max_total_qng;
		}
		if (eeprom_config.max_tag_qng < ADV_MIN_TAG_Q_PER_DVC) {
			eeprom_config.max_tag_qng = ADV_MIN_TAG_Q_PER_DVC;
		}
		adv->max_openings = eeprom_config.max_total_qng;

		if ((eeprom_config.use_cmd_qng & eeprom_config.disc_enable) !=
		    eeprom_config.use_cmd_qng) {
			eeprom_config.disc_enable |= eeprom_config.use_cmd_qng;
			printf("adv:%d: WARNING! One or more targets with tagged "
			       "queuing enabled have the disconnection priveledge "
			       "disabled.\n"
			       "adv:%d: Overriding disconnection settings to "
			       "allow tagged queueing devices to disconnect.\n ",
			       adv->unit, adv->unit);
		}
#if 0
		/*
		 * XXX We should range check our target ID
		 * based on the width of our bus
		 */
		EEPROM_SET_SCSIID(eeprom_config,
				  EEPROM_SCSIID(eeprom_config) & ADV_MAX_TID);
#endif		
		adv->initiate_sdtr = eeprom_config.init_sdtr;
		adv->disc_enable = eeprom_config.disc_enable;
		adv->cmd_qng_enabled = eeprom_config.use_cmd_qng;
		adv->isa_dma_speed = EEPROM_DMA_SPEED(eeprom_config);
		adv->scsi_id = EEPROM_SCSIID(eeprom_config);
		adv->start_motor = eeprom_config.start_motor;
		adv->control = eeprom_config.cntl;
		adv->no_scam = eeprom_config.no_scam;
	} else {
		/*
		 * Use the defaults that adv was initialized with.
		 */
		/*
		 * XXX Fixup EEPROM with default values???
		 */
		printf("adv%d: Warning EEPROM Checksum mismatch. "
		       "Using default device parameters\n", adv->unit);
	}

#if 0
	/* XXX Do this in the PCI probe */
	if ((adv->btype & ADV_PCI) &&
		!(asc_dvc->dvc_cntl & ASC_CNTL_NO_PCI_FIX_ASYN_XFER)) {
		if ((asc_dvc->cfg->pci_device_id == ASC_PCI_DEVICE_ID_REV_A) ||
			(asc_dvc->cfg->pci_device_id == ASC_PCI_DEVICE_ID_REV_B)) {
			asc_dvc->pci_fix_asyn_xfer = ASC_ALL_DEVICE_BIT_SET;
		}
	}
#endif
	if (adv_set_eeprom_config(adv, &eeprom_config) != 0)
		printf("adv:%d: WARNING! Failure writing to EEPROM.\n");

	/* Allocate space for our sense buffers */
	/* XXX this should really be done by the generic SCSI layer by ensuring
	 * that all scsi_xfer structs are allocated below 16M if any controller
	 * needs to bounce.
	 */
	if (adv->type & ADV_ISA) {
		adv->sense_buffers = (struct scsi_sense_data *)contigmalloc(sizeof(struct scsi_sense_data) * adv->max_openings,
									 M_DEVBUF, M_NOWAIT, 0ul, 0xfffffful, 1ul,
									 0x10000ul);
		if (adv->sense_buffers == NULL) {
			printf("adv%d: Unable to allocate sense buffer space.\n");
			return (1);
		}

	}
	
	if (adv_init_lram_and_mcode(adv))
		return (1);

	return (0);
}

void
adv_intr(arg)
	void *arg;
{
	struct	  adv_softc *adv;
	u_int16_t chipstat;
	u_int16_t saved_ram_addr;
	u_int8_t  ctrl_reg;
	u_int8_t  saved_ctrl_reg;
	int	  status;
	u_int8_t  host_flag;

	adv = (struct adv_softc *)arg;

	ctrl_reg = ADV_INB(adv, ADV_CHIP_CTRL);
	saved_ctrl_reg = ctrl_reg & (~(ADV_CC_SCSI_RESET | ADV_CC_CHIP_RESET |
				       ADV_CC_SINGLE_STEP | ADV_CC_DIAG | ADV_CC_TEST));


	if ((chipstat = ADV_INW(adv, ADV_CHIP_STATUS)) & ADV_CSW_INT_PENDING) {
		
		adv_ack_interrupt(adv);

		host_flag = adv_read_lram_8(adv, ADVV_HOST_FLAG_B);
		adv_write_lram_8(adv, ADVV_HOST_FLAG_B,
				 host_flag | ADV_HOST_FLAG_IN_ISR);
		saved_ram_addr = ADV_INW(adv, ADV_LRAM_ADDR);

		if ((chipstat & ADV_CSW_HALTED)
		    && (ctrl_reg & ADV_CC_SINGLE_STEP)) {
			adv_isr_chip_halted(adv);
			saved_ctrl_reg &= ~ADV_CC_HALT;
		} else {
			if ((adv->control & ADV_CNTL_INT_MULTI_Q) != 0) {
				while (((status = adv_qdone(adv)) & 0x01) != 0)
					;
			} else {
				do {
					status = adv_qdone(adv);
				} while (status == 0x11);
			}
		}
		ADV_OUTW(adv, ADV_LRAM_ADDR, saved_ram_addr);
#ifdef DIAGNOSTIC	
		if (ADV_INW(adv, ADV_LRAM_ADDR) != saved_ram_addr)
			panic("adv_intr: Unable to set LRAM addr");
#endif	
		adv_write_lram_8(adv, ADVV_HOST_FLAG_B, host_flag);
	}
	
	ADV_OUTB(adv, ADV_CHIP_CTRL, saved_ctrl_reg);
}

int
adv_qdone(adv)
	struct adv_softc *adv;
{
	u_int8_t	  next_qp;
	u_int8_t	  i;
	u_int8_t	  n_q_used;
	u_int8_t	  sg_list_qp;
	u_int8_t	  sg_queue_cnt;
	u_int8_t	  done_q_tail;
	u_int8_t	  tid_no;
	target_bit_vector target_id;
	u_int16_t	  q_addr;
	u_int16_t	  sg_q_addr;
	struct adv_q_done_info scsiq_buf;
	struct adv_q_done_info *scsiq;
	int               false_overrun;
	u_int8_t	  tag_code;

	n_q_used = 1;
	scsiq = &scsiq_buf;
	done_q_tail = adv_read_lram_16(adv, ADVV_DONE_Q_TAIL_W) & 0xFF;
	q_addr = ADV_QNO_TO_QADDR(done_q_tail);
	next_qp = adv_read_lram_8(adv, q_addr + ADV_SCSIQ_B_FWD);
	if (next_qp != ADV_QLINK_END) {
		adv_write_lram_16(adv, ADVV_DONE_Q_TAIL_W, next_qp);
		q_addr = ADV_QNO_TO_QADDR(next_qp);

		sg_queue_cnt = adv_copy_lram_doneq(adv, q_addr, scsiq, adv->max_dma_count);

		adv_write_lram_8(adv, q_addr + ADV_SCSIQ_B_STATUS,
				 scsiq->q_status & ~(QS_READY | QS_ABORTED));
		tid_no = ADV_TIX_TO_TID(scsiq->d2.target_ix);
		target_id = ADV_TIX_TO_TARGET_ID(scsiq->d2.target_ix);
		if ((scsiq->cntl & QC_SG_HEAD) != 0) {
			sg_q_addr = q_addr;
			sg_list_qp = next_qp;
			for (i = 0; i < sg_queue_cnt; i++) {
				sg_list_qp = adv_read_lram_8(adv,
							     sg_q_addr + ADV_SCSIQ_B_FWD);
				sg_q_addr = ADV_QNO_TO_QADDR(sg_list_qp);
#ifdef DIAGNOSTIC				
				if (sg_list_qp == ASC_QLINK_END) {
					panic("adv_qdone: Corrupted SG list encountered");
				}
#endif				
				adv_write_lram_8(adv, sg_q_addr + ADV_SCSIQ_B_STATUS,
						 QS_FREE);
			}

			n_q_used = sg_queue_cnt + 1;
			adv_write_lram_16(adv, ADVV_DONE_Q_TAIL_W, sg_list_qp);
		}
#if 0
		/* XXX Fix later */
		if (adv->queue_full_or_busy & target_id) {
			cur_target_qng = adv_read_lram_8(adv,
							 ADV_QADR_BEG + scsiq->d2.target_ix);
			if (cur_target_qng < adv->max_dvc_qng[tid_no]) {
				scsi_busy = adv_read_lram_8(adv, ADVV_SCSIBUSY_B);
				scsi_busy &= ~target_id;
				adv_write_lram_8(adv, ADVV_SCSIBUSY_B, scsi_busy);
				adv->queue_full_or_busy &= ~target_id;
			}
		}
#endif
#ifdef DIAGNOSTIC
		if (adv->cur_total_qng < n_q_used)
			panic("adv_qdone: Attempting to free more queues than are active");
#endif		
		adv->cur_active -= n_q_used;

		if ((scsiq->d2.xs_ptr == 0) ||
		    ((scsiq->q_status & QS_ABORTED) != 0))
			return (0x11);
		else if (scsiq->q_status == QS_DONE) {

			false_overrun = FALSE;

			if (adv->bug_fix_control & ADV_BUG_FIX_ADD_ONE_BYTE) {
				tag_code = adv_read_lram_8(adv, q_addr + ADV_SCSIQ_B_TAG_CODE);
				if (tag_code & ADV_TAG_FLAG_ADD_ONE_BYTE) {
					if (scsiq->remain_bytes != 0) {
						scsiq->remain_bytes--;
						if (scsiq->remain_bytes == 0)
							false_overrun = TRUE;
					}
				}
			}
			if ((scsiq->d3.done_stat == QD_WITH_ERROR) &&
			    (scsiq->d3.host_stat == QHSTA_M_DATA_OVER_RUN)) {
				if ((scsiq->cntl & (QC_DATA_IN | QC_DATA_OUT)) == 0) {
					scsiq->d3.done_stat = QD_NO_ERROR;
					scsiq->d3.host_stat = QHSTA_NO_ERROR;
				} else if (false_overrun) {
					scsiq->d3.done_stat = QD_NO_ERROR;
					scsiq->d3.host_stat = QHSTA_NO_ERROR;
				}
			}

			if ((scsiq->cntl & QC_NO_CALLBACK) == 0)
				adv_done(adv, scsiq);
			else {
				if ((adv_read_lram_8(adv, q_addr + ADV_SCSIQ_CDB_BEG) ==
				     START_STOP)) {
					adv->unit_not_ready &= ~target_id;
					if (scsiq->d3.done_stat != QD_NO_ERROR)
						adv->start_motor &= ~target_id;
				}
			}
			return (1);
		} else {
			panic("adv_qdone: completed scsiq with unknown status");
#if 0
			/*
			 * XXX Doesn't this simply indicate a software bug?
			 *     What does setting the lram error code do for
			 *     you.  Would we even recover?
			 */
			AscSetLibErrorCode(asc_dvc, ASCQ_ERR_Q_STATUS);

		  FATAL_ERR_QDONE:
			if ((scsiq->cntl & QC_NO_CALLBACK) == 0) {
				(*asc_isr_callback) (asc_dvc, scsiq);
			}
			return (0x80);
#endif
		}
	}
	return (0);
}


void
adv_done(adv, qdonep)
	struct adv_softc *adv;
	struct adv_q_done_info *qdonep;
{
	struct	scsi_xfer *xs;

	xs = (struct scsi_xfer *)qdonep->d2.xs_ptr;

	xs->status = qdonep->d3.scsi_stat;
	/*
	 * 'qdonep' contains the command's ending status.
	 */
	switch (qdonep->d3.done_stat) {
	case QD_NO_ERROR:
		switch (qdonep->d3.host_stat) {
		case QHSTA_NO_ERROR:
			break;
		case QHSTA_M_SEL_TIMEOUT:
			xs->error = XS_SELTIMEOUT;
			break;
		default:
			/* QHSTA error occurred */
#if 0
			/* XXX Can I get more explicit information here? */
			xs->error = XS_DRIVER_STUFFUP;
#endif
			break;
		}
		break;

	case QD_WITH_ERROR:
		switch (qdonep->d3.host_stat) {
		case QHSTA_NO_ERROR:
			if ((qdonep->d3.scsi_stat == STATUS_CHECK_CONDITION)
			 || (qdonep->d3.scsi_stat == STATUS_COMMAND_TERMINATED)) {
				/* We have valid sense information to return */
				xs->error = XS_SENSE;
				if (adv->sense_buffers != NULL)
					/* Structure copy */
					xs->sense = adv->sense_buffers[qdonep->q_no];
			}
			break;
		case QHSTA_M_SEL_TIMEOUT:
			xs->error = XS_SELTIMEOUT;
			break;
		default:
#if 0
			/* XXX Can I get more explicit information here? */
			xs->error = XS_DRIVER_STUFFUP;
#endif
			break;
		}
		break;

	case QD_ABORTED_BY_HOST:
		/* XXX Should have an explicit ABORTED error code */
		xs->error = XS_ABORTED;
		break;

	default:
#if 0
		printf("adv_done: Unknown done status 0x%x\n",
			qdonep->d3.done_stat);
		xs->error = XS_DRIVER_STUFFUP;
#endif
		break;
	}
	xs->flags |= SCSI_ITSDONE;
	scsi_done(xs);
	return;
}

/*
 * Function to poll for command completion when
 * interrupts are disabled (crash dumps)
 */
static int
adv_poll(adv, xs)
	struct	adv_softc *adv;
	struct	scsi_xfer *xs;
{
	int	wait;

	wait = xs->timeout;
	do {
		DELAY(1000);
		adv_intr((void *)adv);
	} while (--wait && ((xs->flags & SCSI_ITSDONE) == 0));
	if (wait == 0) {
		printf("adv%d: board is not responding\n", adv->unit);
		return (EIO);
	}
	return (0);
}

/*
 * Attach all the sub-devices we can find
 */
int
adv_attach(adv)
	struct adv_softc *adv;
{
	struct scsi_bus *scbus;
	struct scsi_queue *scsiq;

	scsiq = scsi_alloc_queue(adv->max_openings);
	if (scsiq == NULL)
		return 0;

	/*
	 * Prepare the scsi_bus area for the upperlevel scsi code.
	 */
	scbus = scsi_alloc_bus(&adv_switch, adv, adv->unit, scsiq);
	if (scbus == NULL) {
		scsi_free_queue(scsiq);
		return 0;
	}

	/* Override defaults */
	if ((adv->type & ADV_ISA) != 0)
		scbus->adpt_link.adpt_flags |= SADPT_BOUNCE;
	scbus->adpt_link.adpt_target = adv->scsi_id;
	scbus->adpt_link.adpt_openings = 2;  /* XXX Is this correct for these cards? */
	scbus->adpt_link.adpt_tagged_openings = adv->max_openings;

	/*
	 * ask the adapter what subunits are present
	 */
	if(bootverbose)
		printf("adv%d: Probing SCSI bus\n", adv->unit);

	scsi_attachdevs(scbus);

	return 1;
}
