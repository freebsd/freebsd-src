/*
 * Low level routines for Second Generation
 * Advanced Systems Inc. SCSI controllers chips
 *
 * Copyright (c) 1998 Justin Gibbs.
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
 * Copyright (c) 1995-1998 Advanced System Products, Inc.
 * All Rights Reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/clock.h>

#include <cam/cam.h>
#include <cam/scsi/scsi_all.h>

#include <dev/advansys/adwlib.h>

struct adw_eeprom adw_default_eeprom = {
	ADW_EEPROM_BIOS_ENABLE,	/* cfg_lsw */
	0x0000,			/* cfg_msw */
	0xFFFF,			/* disc_enable */
	0xFFFF,			/* wdtr_able */
	0xFFFF,			/* sdtr_able */
	0xFFFF,			/* start_motor */
	0xFFFF,			/* tagqng_able */
	0xFFFF,			/* bios_scan */
	0,			/* scam_tolerant */
	7,			/* adapter_scsi_id */
	0,			/* bios_boot_delay */
	3,			/* scsi_reset_delay */
	0,			/* bios_id_lun */
	0,			/* termination */
	0,			/* reserved1 */
	{			/* Bios Ctrl */
	  1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1,
	},
	0xFFFF,			/* ultra_able */
	0,			/* reserved2 */
	ADW_DEF_MAX_HOST_QNG,	/* max_host_qng */
	ADW_DEF_MAX_DVC_QNG,	/* max_dvc_qng */
	0,			/* dvc_cntl */
	0,			/* bug_fix */
	{ 0, 0, 0 },		/* serial_number */
	0,			/* check_sum */
	{			/* oem_name[16] */
	  0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0
	},
	0,			/* dvc_err_code */
	0,			/* adv_err_code */
	0,			/* adv_err_addr */
	0,			/* saved_dvc_err_code */
	0,			/* saved_adv_err_code */
	0,			/* saved_adv_err_addr */
	0			/* num_of_err */
};

static u_int16_t	adw_eeprom_read_16(struct adw_softc *adw, int addr);
static void		adw_eeprom_write_16(struct adw_softc *adw, int addr,
					    u_int data);
static void		adw_eeprom_wait(struct adw_softc *adw);

int
adw_find_signature(bus_space_tag_t tag, bus_space_handle_t bsh)
{
	if (bus_space_read_1(tag, bsh, ADW_SIGNATURE_BYTE) == ADW_CHIP_ID_BYTE
	 && bus_space_read_2(tag, bsh, ADW_SIGNATURE_WORD) == ADW_CHIP_ID_WORD)
		return (1);
	return (0);
}

/*
 * Reset Chip.
 */
void
adw_reset_chip(struct adw_softc *adw)
{
	adw_outw(adw, ADW_CTRL_REG, ADW_CTRL_REG_CMD_RESET);
	DELAY(100);
	adw_outw(adw, ADW_CTRL_REG, ADW_CTRL_REG_CMD_WR_IO_REG);

	/*
	 * Initialize Chip registers.
	 */
	adw_outb(adw, ADW_MEM_CFG,
		 adw_inb(adw, ADW_MEM_CFG) | ADW_MEM_CFG_RAM_SZ_8KB);

	adw_outw(adw, ADW_SCSI_CFG1,
		 adw_inw(adw, ADW_SCSI_CFG1) & ~ADW_SCSI_CFG1_BIG_ENDIAN);

	/*
	 * Setting the START_CTL_EM_FU 3:2 bits sets a FIFO threshold
	 * of 128 bytes. This register is only accessible to the host.
	 */
	adw_outb(adw, ADW_DMA_CFG0,
		 ADW_DMA_CFG0_START_CTL_EM_FU|ADW_DMA_CFG0_READ_CMD_MRM);
}

/*
 * Read the specified EEPROM location
 */
static u_int16_t
adw_eeprom_read_16(struct adw_softc *adw, int addr)
{
	adw_outw(adw, ADW_EEP_CMD, ADW_EEP_CMD_READ | addr);
	adw_eeprom_wait(adw);
	return (adw_inw(adw, ADW_EEP_DATA));
}

static void
adw_eeprom_write_16(struct adw_softc *adw, int addr, u_int data)
{
	adw_outw(adw, ADW_EEP_DATA, data);
	adw_outw(adw, ADW_EEP_CMD, ADW_EEP_CMD_WRITE | addr);
	adw_eeprom_wait(adw);
}

/*
 * Wait for and EEPROM command to complete
 */
static void
adw_eeprom_wait(struct adw_softc *adw)
{
	int i;

	for (i = 0; i < ADW_EEP_DELAY_MS; i++) {
		if ((adw_inw(adw, ADW_EEP_CMD) & ADW_EEP_CMD_DONE) != 0)
            		break;
		DELAY(1000);
    	}
	if (i == ADW_EEP_DELAY_MS)
		panic("%s: Timedout Reading EEPROM", adw_name(adw));
}

/*
 * Read EEPROM configuration into the specified buffer.
 *
 * Return a checksum based on the EEPROM configuration read.
 */
u_int16_t
adw_eeprom_read(struct adw_softc *adw, struct adw_eeprom *eep_buf)
{
	u_int16_t *wbuf;
	u_int16_t  wval;
	u_int16_t  chksum;
	int	   eep_addr;

	wbuf = (u_int16_t *)eep_buf;
	chksum = 0;

	for (eep_addr = ADW_EEP_DVC_CFG_BEGIN;
	     eep_addr < ADW_EEP_DVC_CFG_END;
	     eep_addr++, wbuf++) {
		wval = adw_eeprom_read_16(adw, eep_addr);
		chksum += wval;
		*wbuf = wval;
	}

	/* checksum field is not counted in the checksum */
	*wbuf = adw_eeprom_read_16(adw, eep_addr);
	wbuf++;
	
	/* Driver seeprom variables are not included in the checksum */
	for (eep_addr = ADW_EEP_DVC_CTL_BEGIN;
	     eep_addr < ADW_EEP_MAX_WORD_ADDR;
	     eep_addr++, wbuf++)
		*wbuf = adw_eeprom_read_16(adw, eep_addr);

	return (chksum);
}

void
adw_eeprom_write(struct adw_softc *adw, struct adw_eeprom *eep_buf)
{
	u_int16_t *wbuf;
	u_int16_t  addr;
	u_int16_t  chksum;

	wbuf = (u_int16_t *)eep_buf;
	chksum = 0;

	adw_outw(adw, ADW_EEP_CMD, ADW_EEP_CMD_WRITE_ABLE);
	adw_eeprom_wait(adw);

	/*
	 * Write EEPROM until checksum.
	 */
	for (addr = ADW_EEP_DVC_CFG_BEGIN;
	     addr < ADW_EEP_DVC_CFG_END; addr++, wbuf++) {
		chksum += *wbuf;
		adw_eeprom_write_16(adw, addr, *wbuf);
	}

	/*
	 * Write calculated EEPROM checksum
	 */
	adw_eeprom_write_16(adw, addr, chksum);

	/* skip over buffer's checksum */
	wbuf++;

	/*
	 * Write the rest.
	 */
	for (addr = ADW_EEP_DVC_CTL_BEGIN;
	     addr < ADW_EEP_MAX_WORD_ADDR; addr++, wbuf++)
		adw_eeprom_write_16(adw, addr, *wbuf);

	adw_outw(adw, ADW_EEP_CMD, ADW_EEP_CMD_WRITE_DISABLE);
	adw_eeprom_wait(adw);
}

int
adw_init_chip(struct adw_softc *adw, u_int term_scsicfg1)
{
	u_int8_t   biosmem[ADW_MC_BIOSLEN];
	u_int16_t *mcodebuf;
	u_int	   addr;
	u_int	   end_addr;
	u_int	   checksum;
	u_int	   scsicfg1;
	u_int	   i;

	/*
	 * Save the RISC memory BIOS region before writing the microcode.
	 * The BIOS may already be loaded and using its RISC LRAM region
	 * so its region must be saved and restored.
	 */
	for (addr = 0; addr < ADW_MC_BIOSLEN; addr++)
		biosmem[addr] = adw_lram_read_8(adw, ADW_MC_BIOSMEM + addr);

	/*
	 * Load the Microcode.  Casting here was less work than
	 * reformatting the supplied microcode into an array of
	 * 16bit values...
	 */
	mcodebuf = (u_int16_t *)adw_mcode;
	adw_outw(adw, ADW_RAM_ADDR, 0);
	for (addr = 0; addr < adw_mcode_size/2; addr++)
		adw_outw(adw, ADW_RAM_DATA, mcodebuf[addr]);

	/*
	 * Clear the rest of LRAM.
	 */
	for (; addr < ADW_CONDOR_MEMSIZE/2; addr++)
		adw_outw(adw, ADW_RAM_DATA, 0);

	/*
	 * Verify the microcode checksum.
	 */
	checksum = 0;
	adw_outw(adw, ADW_RAM_ADDR, 0);
	for (addr = 0; addr < adw_mcode_size/2; addr++)
		checksum += adw_inw(adw, ADW_RAM_DATA);

	if (checksum != adw_mcode_chksum) {
		printf("%s: Firmware load failed!\n", adw_name(adw));
		return (-1);
	}

	/*
	 * Restore the RISC memory BIOS region.
	 */
	for (addr = 0; addr < ADW_MC_BIOSLEN; addr++)
		adw_lram_write_8(adw, addr + ADW_MC_BIOSLEN, biosmem[addr]);

	/*
	 * Calculate and write the microcode code checksum to
	 * the microcode code checksum location.
	 */
	addr = adw_lram_read_16(adw, ADW_MC_CODE_BEGIN_ADDR) / 2;
	end_addr = adw_lram_read_16(adw, ADW_MC_CODE_END_ADDR) / 2;
	checksum = 0;
	for (; addr < end_addr; addr++)
		checksum += mcodebuf[addr];
	adw_lram_write_16(adw, ADW_MC_CODE_CHK_SUM, checksum);

	/*
	 * Initialize microcode operating variables
	 */
	adw_lram_write_16(adw, ADW_MC_ADAPTER_SCSI_ID, adw->initiator_id);

	/*
	 * Leave WDTR and SDTR negotiation disabled until the XPT has
	 * informed us of device capabilities, but do set the ultra mask
	 * in case we receive an SDTR request from the target before we
	 * negotiate.  We turn on tagged queuing at the microcode level
	 * for all devices, and modulate this on a per command basis.
	 */
	adw_lram_write_16(adw, ADW_MC_ULTRA_ABLE, adw->user_ultra);
	adw_lram_write_16(adw, ADW_MC_DISC_ENABLE, adw->user_discenb);
	adw_lram_write_16(adw, ADW_MC_TAGQNG_ABLE, ~0);

	/*
	 * Set SCSI_CFG0 Microcode Default Value.
	 *
	 * The microcode will set the SCSI_CFG0 register using this value
	 * after it is started.
	 */
	adw_lram_write_16(adw, ADW_MC_DEFAULT_SCSI_CFG0,
			  ADW_SCSI_CFG0_PARITY_EN|ADW_SCSI_CFG0_SEL_TMO_LONG|
			  ADW_SCSI_CFG0_OUR_ID_EN|adw->initiator_id);
  
	/*
	 * Determine SCSI_CFG1 Microcode Default Value.
	 *
	 * The microcode will set the SCSI_CFG1 register using this value
	 * after it is started below.
	 */
	scsicfg1 = adw_inw(adw, ADW_SCSI_CFG1);

	/*
	 * If all three connectors are in use, return an error.
	 */
	if ((scsicfg1 & ADW_SCSI_CFG1_ILLEGAL_CABLE_CONF_A_MASK) == 0
	 || (scsicfg1 & ADW_SCSI_CFG1_ILLEGAL_CABLE_CONF_B_MASK) == 0) {
		printf("%s: Illegal Cable Config!\n", adw_name(adw));
		printf("%s: Only Two Ports may be used at a time!\n",
		       adw_name(adw));
		return (-1);
	}

	/*
	 * If the internal narrow cable is reversed all of the SCSI_CTRL
	 * register signals will be set. Check for and return an error if
	 * this condition is found.
	 */
	if ((adw_inw(adw, ADW_SCSI_CTRL) & 0x3F07) == 0x3F07) {
		printf("%s: Illegal Cable Config!\n", adw_name(adw));
		printf("%s: Internal cable is reversed!\n", adw_name(adw));
	        return (-1);
	}

	/*
	 * If this is a differential board and a single-ended device
	 * is attached to one of the connectors, return an error.
	 */
	if ((scsicfg1 & ADW_SCSI_CFG1_DIFF_MODE) != 0
	 && (scsicfg1 & ADW_SCSI_CFG1_DIFF_SENSE) == 0) {
		printf("%s: A Single Ended Device is attached to our "
		       "differential bus!\n", adw_name(adw));
	        return (-1);
	}

	/*
	 * Perform automatic termination control if desired.
	 */
	if (term_scsicfg1 == 0) {
        	switch(scsicfg1 & ADW_SCSI_CFG1_CABLE_DETECT) {
		case (ADW_SCSI_CFG1_INT16_MASK|ADW_SCSI_CFG1_INT8_MASK):
		case (ADW_SCSI_CFG1_INT16_MASK|
		      ADW_SCSI_CFG1_INT8_MASK|ADW_SCSI_CFG1_EXT8_MASK):
		case (ADW_SCSI_CFG1_INT16_MASK|
		      ADW_SCSI_CFG1_INT8_MASK|ADW_SCSI_CFG1_EXT16_MASK):
		case (ADW_SCSI_CFG1_INT16_MASK|
		      ADW_SCSI_CFG1_EXT8_MASK|ADW_SCSI_CFG1_EXT16_MASK):
		case (ADW_SCSI_CFG1_INT8_MASK|
		      ADW_SCSI_CFG1_EXT8_MASK|ADW_SCSI_CFG1_EXT16_MASK):
		case (ADW_SCSI_CFG1_INT16_MASK|ADW_SCSI_CFG1_INT8_MASK|
		      ADW_SCSI_CFG1_EXT8_MASK|ADW_SCSI_CFG1_EXT16_MASK):
			/* Two out of three cables missing.  Both on. */
			term_scsicfg1 |= ADW_SCSI_CFG1_TERM_CTL_L
				      |  ADW_SCSI_CFG1_TERM_CTL_H;
			break;
		case (ADW_SCSI_CFG1_INT16_MASK):
		case (ADW_SCSI_CFG1_INT16_MASK|ADW_SCSI_CFG1_EXT8_MASK):
		case (ADW_SCSI_CFG1_INT16_MASK|ADW_SCSI_CFG1_EXT16_MASK):
		case (ADW_SCSI_CFG1_INT8_MASK|ADW_SCSI_CFG1_EXT16_MASK):
		case (ADW_SCSI_CFG1_EXT8_MASK|ADW_SCSI_CFG1_EXT16_MASK):
			/* No two 16bit cables present.  High on. */
			term_scsicfg1 |= ADW_SCSI_CFG1_TERM_CTL_H;
			break;
		case (ADW_SCSI_CFG1_INT8_MASK):
		case (ADW_SCSI_CFG1_INT8_MASK|ADW_SCSI_CFG1_EXT8_MASK):
			/* Wide -> Wide or Narrow -> Wide. Both off */
			break;
		}
        }

	/* Tell the user about our decission */
	switch (term_scsicfg1 & ADW_SCSI_CFG1_TERM_CTL_MASK) {
	case ADW_SCSI_CFG1_TERM_CTL_MASK:
		printf("High & Low Termination Enabled, ");
		break;
	case ADW_SCSI_CFG1_TERM_CTL_H:
		printf("High Termination Enabled, ");
		break;
	case ADW_SCSI_CFG1_TERM_CTL_L:
		printf("Low Termination Enabled, ");
		break;
	default:
		break;
	}

	/*
	 * Invert the TERM_CTL_H and TERM_CTL_L bits and then
	 * set 'scsicfg1'. The TERM_POL bit does not need to be
	 * referenced, because the hardware internally inverts
	 * the Termination High and Low bits if TERM_POL is set.
	 */
	term_scsicfg1 = ~term_scsicfg1 & ADW_SCSI_CFG1_TERM_CTL_MASK;
	scsicfg1 &= ~ADW_SCSI_CFG1_TERM_CTL_MASK;
	scsicfg1 |= term_scsicfg1 | ADW_SCSI_CFG1_TERM_CTL_MANUAL;

	/*
	 * Set SCSI_CFG1 Microcode Default Value
	 *
	 * Set filter value and possibly modified termination control
	 * bits in the Microcode SCSI_CFG1 Register Value.
	 *
	 * The microcode will set the SCSI_CFG1 register using this value
	 * after it is started below.
	 */
	adw_lram_write_16(adw, ADW_MC_DEFAULT_SCSI_CFG1,
			  scsicfg1 | ADW_SCSI_CFG1_FLTR_11_TO_20NS);

	/*
	 * Only accept selections on our initiator target id.
	 * This may change in target mode scenarios...
	 */
	adw_lram_write_16(adw, ADW_MC_DEFAULT_SEL_MASK,
			  (0x01 << adw->initiator_id));

	/*
	 * Link all the RISC Queue Lists together in a doubly-linked
	 * NULL terminated list.
	 *
	 * Skip the NULL (0) queue which is not used.
	 */
	for (i = 1, addr = ADW_MC_RISC_Q_LIST_BASE + ADW_MC_RISC_Q_LIST_SIZE;
	     i < ADW_MC_RISC_Q_TOTAL_CNT;
	     i++, addr += ADW_MC_RISC_Q_LIST_SIZE) {

	        /*
		 * Set the current RISC Queue List's
		 * RQL_FWD and RQL_BWD pointers in a
		 * one word write and set the state
		 * (RQL_STATE) to free.
		 */
		adw_lram_write_16(adw, addr, ((i + 1) | ((i - 1) << 8)));
		adw_lram_write_8(adw, addr + RQL_STATE, ADW_MC_QS_FREE);
	}

	/*
	 * Set the Host and RISC Queue List pointers.
	 *
	 * Both sets of pointers are initialized with the same values:
	 * ADW_MC_RISC_Q_FIRST(0x01) and ADW_MC_RISC_Q_LAST (0xFF).
	 */
	adw_lram_write_8(adw, ADW_MC_HOST_NEXT_READY, ADW_MC_RISC_Q_FIRST);
	adw_lram_write_8(adw, ADW_MC_HOST_NEXT_DONE, ADW_MC_RISC_Q_LAST);

	adw_lram_write_8(adw, ADW_MC_RISC_NEXT_READY, ADW_MC_RISC_Q_FIRST);
	adw_lram_write_8(adw, ADW_MC_RISC_NEXT_DONE, ADW_MC_RISC_Q_LAST);

	/*
	 * Set up the last RISC Queue List (255) with a NULL forward pointer.
	 */
	adw_lram_write_16(adw, addr, (ADW_MC_NULL_Q + ((i - 1) << 8)));
	adw_lram_write_8(adw, addr + RQL_STATE, ADW_MC_QS_FREE);

	adw_outb(adw, ADW_INTR_ENABLES,
		 ADW_INTR_ENABLE_HOST_INTR|ADW_INTR_ENABLE_GLOBAL_INTR);

	adw_outw(adw, ADW_PC, adw_lram_read_16(adw, ADW_MC_CODE_BEGIN_ADDR));

	return (0);
}

/*
 * Send an idle command to the chip and optionally wait for completion.
 */
void
adw_idle_cmd_send(struct adw_softc *adw, adw_idle_cmd_t cmd, u_int parameter)
{
	int s;

	adw->idle_command_cmp = 0;

	s = splcam();	

	if (adw->idle_cmd != ADW_IDLE_CMD_COMPLETED)
		printf("%s: Warning! Overlapped Idle Commands Attempted\n",
		       adw_name(adw));
	adw->idle_cmd = cmd;
	adw->idle_cmd_param = parameter;

	/*
	 * Write the idle command value after the idle command parameter
	 * has been written to avoid a race condition. If the order is not
	 * followed, the microcode may process the idle command before the
	 * parameters have been written to LRAM.
	 */
	adw_lram_write_16(adw, ADW_MC_IDLE_PARA_STAT, parameter);
    	adw_lram_write_16(adw, ADW_MC_IDLE_CMD, cmd);
	splx(s);
}

/* Wait for an idle command to complete */
adw_idle_cmd_status_t
adw_idle_cmd_wait(struct adw_softc *adw)
{
	u_int		      timeout;
	adw_idle_cmd_status_t status;
	int		      s;

	/* Wait for up to 10 seconds for the command to complete */
	timeout = 10000;
	while (--timeout) {
       		if (adw->idle_command_cmp != 0)
			break;
		DELAY(1000);
	}

	if (timeout == 0)
		panic("%s: Idle Command Timed Out!\n", adw_name(adw));
	s = splcam();
	status = adw_lram_read_16(adw, ADW_MC_IDLE_PARA_STAT);
	splx(s);
	return (status);
}
