/*-
 * Low level routines for Second Generation
 * Advanced Systems Inc. SCSI controllers chips
 *
 * Copyright (c) 1998, 1999, 2000 Justin Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
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
 */
/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/scsi/scsi_all.h>

#include <dev/advansys/adwlib.h>

const struct adw_eeprom adw_asc3550_default_eeprom =
{
	ADW_EEPROM_BIOS_ENABLE,		/* cfg_lsw */
	0x0000,				/* cfg_msw */
	0xFFFF,				/* disc_enable */
	0xFFFF,				/* wdtr_able */
	{ 0xFFFF },			/* sdtr_able */
	0xFFFF,				/* start_motor */
	0xFFFF,				/* tagqng_able */
	0xFFFF,				/* bios_scan */
	0,				/* scam_tolerant */
	7,				/* adapter_scsi_id */
	0,				/* bios_boot_delay */
	3,				/* scsi_reset_delay */
	0,				/* bios_id_lun */
	0,				/* termination */
	0,				/* reserved1 */
	0xFFE7,				/* bios_ctrl */
	{ 0xFFFF },			/* ultra_able */   
	{ 0 },				/* reserved2 */
	ADW_DEF_MAX_HOST_QNG,		/* max_host_qng */
	ADW_DEF_MAX_DVC_QNG,		/* max_dvc_qng */
	0,				/* dvc_cntl */
	{ 0 },				/* bug_fix */
	{ 0, 0, 0 },			/* serial_number */
	0,				/* check_sum */
	{				/* oem_name[16] */
	  0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0
	},
	0,				/* dvc_err_code */
	0,				/* adv_err_code */
	0,				/* adv_err_addr */
	0,				/* saved_dvc_err_code */
	0,				/* saved_adv_err_code */
	0				/* saved_adv_err_addr */
};

const struct adw_eeprom adw_asc38C0800_default_eeprom =
{
	ADW_EEPROM_BIOS_ENABLE,		/* 00 cfg_lsw */
	0x0000,				/* 01 cfg_msw */
	0xFFFF,				/* 02 disc_enable */
	0xFFFF,				/* 03 wdtr_able */
	{ 0x4444 },			/* 04 sdtr_speed1 */
	0xFFFF,				/* 05 start_motor */
	0xFFFF,				/* 06 tagqng_able */
	0xFFFF,				/* 07 bios_scan */
	0,				/* 08 scam_tolerant */
	7,				/* 09 adapter_scsi_id */
	0,				/*    bios_boot_delay */
	3,				/* 10 scsi_reset_delay */
	0,				/*    bios_id_lun */
	0,				/* 11 termination_se */
	0,				/*    termination_lvd */
	0xFFE7,				/* 12 bios_ctrl */
	{ 0x4444 },			/* 13 sdtr_speed2 */
	{ 0x4444 },			/* 14 sdtr_speed3 */
	ADW_DEF_MAX_HOST_QNG,		/* 15 max_host_qng */
	ADW_DEF_MAX_DVC_QNG,		/*    max_dvc_qng */
	0,				/* 16 dvc_cntl */
	{ 0x4444 } ,			/* 17 sdtr_speed4 */
	{ 0, 0, 0 },			/* 18-20 serial_number */
	0,				/* 21 check_sum */
	{				/* 22-29 oem_name[16] */
	  0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0
	},
	0,				/* 30 dvc_err_code */
	0,				/* 31 adv_err_code */
	0,				/* 32 adv_err_addr */
	0,				/* 33 saved_dvc_err_code */
	0,				/* 34 saved_adv_err_code */
	0,				/* 35 saved_adv_err_addr */
	{				/* 36 - 55 reserved */
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	},
	0,				/* 56 cisptr_lsw */
	0,				/* 57 cisprt_msw */
					/* 58-59 sub-id */
	(PCI_ID_ADVANSYS_38C0800_REV1 & PCI_ID_DEV_VENDOR_MASK) >> 32,
};

#define ADW_MC_SDTR_OFFSET_ULTRA2_DT	0
#define ADW_MC_SDTR_OFFSET_ULTRA2	1
#define ADW_MC_SDTR_OFFSET_ULTRA	2
const struct adw_syncrate adw_syncrates[] =
{
	/*   mc_sdtr		  period      rate */
	{ ADW_MC_SDTR_80,	    9,	     "80.0"  },
	{ ADW_MC_SDTR_40,	    10,	     "40.0"  },
	{ ADW_MC_SDTR_20,	    12,	     "20.0"  },
	{ ADW_MC_SDTR_10,	    25,	     "10.0"  },
	{ ADW_MC_SDTR_5,	    50,	     "5.0"   },
	{ ADW_MC_SDTR_ASYNC,	    0,	     "async" }
};

const int adw_num_syncrates = sizeof(adw_syncrates) / sizeof(adw_syncrates[0]);

static u_int16_t	adw_eeprom_read_16(struct adw_softc *adw, int addr);
static void		adw_eeprom_write_16(struct adw_softc *adw, int addr,
					    u_int data);
static void		adw_eeprom_wait(struct adw_softc *adw);

int
adw_find_signature(struct adw_softc *adw)
{
	if (adw_inb(adw, ADW_SIGNATURE_BYTE) == ADW_CHIP_ID_BYTE
	 && adw_inw(adw, ADW_SIGNATURE_WORD) == ADW_CHIP_ID_WORD)
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
	DELAY(1000 * 100);
	adw_outw(adw, ADW_CTRL_REG, ADW_CTRL_REG_CMD_WR_IO_REG);

	/*
	 * Initialize Chip registers.
	 */
	adw_outw(adw, ADW_SCSI_CFG1,
		 adw_inw(adw, ADW_SCSI_CFG1) & ~ADW_SCSI_CFG1_BIG_ENDIAN);
}

/*
 * Reset the SCSI bus.
 */
int
adw_reset_bus(struct adw_softc *adw)
{
	adw_idle_cmd_status_t status;

	status =
	    adw_idle_cmd_send(adw, ADW_IDLE_CMD_SCSI_RESET_START, /*param*/0);
	if (status != ADW_IDLE_CMD_SUCCESS) {
		xpt_print_path(adw->path);
		printf("Bus Reset start attempt failed\n");
		return (1);
	}
	DELAY(ADW_BUS_RESET_HOLD_DELAY_US);
	status =
	    adw_idle_cmd_send(adw, ADW_IDLE_CMD_SCSI_RESET_END, /*param*/0);
	if (status != ADW_IDLE_CMD_SUCCESS) {
		xpt_print_path(adw->path);
		printf("Bus Reset end attempt failed\n");
		return (1);
	}
	return (0);
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
	u_int8_t	    biosmem[ADW_MC_BIOSLEN];
	const u_int16_t    *word_table;
	const u_int8_t     *byte_codes;
	const u_int8_t     *byte_codes_end;
	u_int		    bios_sig;
	u_int		    bytes_downloaded;
	u_int		    addr;
	u_int		    end_addr;
	u_int		    checksum;
	u_int		    scsicfg1;
	u_int		    tid;

	/*
	 * Save the RISC memory BIOS region before writing the microcode.
	 * The BIOS may already be loaded and using its RISC LRAM region
	 * so its region must be saved and restored.
	 */
	for (addr = 0; addr < ADW_MC_BIOSLEN; addr++)
		biosmem[addr] = adw_lram_read_8(adw, ADW_MC_BIOSMEM + addr);

	/*
	 * Save current per TID negotiated values if the BIOS has been
	 * loaded (BIOS signature is present).  These will be used if
	 * we cannot get information from the EEPROM.
	 */
	addr = ADW_MC_BIOS_SIGNATURE - ADW_MC_BIOSMEM;
	bios_sig = biosmem[addr]
		 | (biosmem[addr + 1] << 8);
	if (bios_sig == 0x55AA
	 && (adw->flags & ADW_EEPROM_FAILED) != 0) {
		u_int major_ver;
		u_int minor_ver;
		u_int sdtr_able;

		addr = ADW_MC_BIOS_VERSION - ADW_MC_BIOSMEM;
		minor_ver = biosmem[addr + 1] & 0xF;
		major_ver = (biosmem[addr + 1] >> 4) & 0xF;
		if ((adw->chip == ADW_CHIP_ASC3550)
		 && (major_ver <= 3
		  || (major_ver == 3 && minor_ver <= 1))) {
			/*
			 * BIOS 3.1 and earlier location of
			 * 'wdtr_able' variable.
			 */
			adw->user_wdtr =
			    adw_lram_read_16(adw, ADW_MC_WDTR_ABLE_BIOS_31);
		} else {
			adw->user_wdtr =
			    adw_lram_read_16(adw, ADW_MC_WDTR_ABLE);
		}
		sdtr_able = adw_lram_read_16(adw, ADW_MC_SDTR_ABLE);
		for (tid = 0; tid < ADW_MAX_TID; tid++) {
			u_int tid_mask;
			u_int mc_sdtr;

			tid_mask = 0x1 << tid;
			if ((sdtr_able & tid_mask) == 0)
				mc_sdtr = ADW_MC_SDTR_ASYNC;
			else if ((adw->features & ADW_DT) != 0)
				mc_sdtr = ADW_MC_SDTR_80;
			else if ((adw->features & ADW_ULTRA2) != 0)
				mc_sdtr = ADW_MC_SDTR_40;
			else
				mc_sdtr = ADW_MC_SDTR_20;
			adw_set_user_sdtr(adw, tid, mc_sdtr);
		}
		adw->user_tagenb = adw_lram_read_16(adw, ADW_MC_TAGQNG_ABLE);
	}

	/*
	 * Load the Microcode.
	 *
	 * Assume the following compressed format of the microcode buffer:
	 *
	 *	253 word (506 byte) table indexed by byte code followed
	 *	by the following byte codes:
	 *
	 *	1-Byte Code:
	 *		00: Emit word 0 in table.
	 *		01: Emit word 1 in table.
	 *		.
	 *		FD: Emit word 253 in table.
	 *
	 *	Multi-Byte Code:
	 *		FD RESEVED
	 *
	 *		FE WW WW: (3 byte code)
	 *			Word to emit is the next word WW WW.
	 *		FF BB WW WW: (4 byte code)
	 *			Emit BB count times next word WW WW.
	 *
	 */
	bytes_downloaded = 0;
	word_table = (const u_int16_t *)adw->mcode_data->mcode_buf;
	byte_codes = (const u_int8_t *)&word_table[253];
	byte_codes_end = adw->mcode_data->mcode_buf
		       + adw->mcode_data->mcode_size;
	adw_outw(adw, ADW_RAM_ADDR, 0);
	while (byte_codes < byte_codes_end) {
		if (*byte_codes == 0xFF) {
			u_int16_t value;

			value = byte_codes[2]
			      | byte_codes[3] << 8;
			adw_set_multi_2(adw, ADW_RAM_DATA,
					value, byte_codes[1]);
			bytes_downloaded += byte_codes[1];
			byte_codes += 4;
		} else if (*byte_codes == 0xFE) {
			u_int16_t value;

			value = byte_codes[1]
			      | byte_codes[2] << 8;
			adw_outw(adw, ADW_RAM_DATA, value);
			bytes_downloaded++;
			byte_codes += 3;
		} else {
			adw_outw(adw, ADW_RAM_DATA, word_table[*byte_codes]);
			bytes_downloaded++;
			byte_codes++;
		}
	}
	/* Convert from words to bytes */
	bytes_downloaded *= 2;

	/*
	 * Clear the rest of LRAM.
	 */
	for (addr = bytes_downloaded; addr < adw->memsize; addr += 2)
		adw_outw(adw, ADW_RAM_DATA, 0);

	/*
	 * Verify the microcode checksum.
	 */
	checksum = 0;
	adw_outw(adw, ADW_RAM_ADDR, 0);
	for (addr = 0; addr < bytes_downloaded; addr += 2)
		checksum += adw_inw(adw, ADW_RAM_DATA);

	if (checksum != adw->mcode_data->mcode_chksum) {
		printf("%s: Firmware load failed!\n", adw_name(adw));
		return (EIO);
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
	addr = adw_lram_read_16(adw, ADW_MC_CODE_BEGIN_ADDR);
	end_addr = adw_lram_read_16(adw, ADW_MC_CODE_END_ADDR);
	checksum = 0;
	adw_outw(adw, ADW_RAM_ADDR, addr);
	for (; addr < end_addr; addr += 2)
		checksum += adw_inw(adw, ADW_RAM_DATA);
	adw_lram_write_16(adw, ADW_MC_CODE_CHK_SUM, checksum);

	/*
	 * Tell the microcode what kind of chip it's running on.
	 */
	adw_lram_write_16(adw, ADW_MC_CHIP_TYPE, adw->chip);

	/*
	 * Leave WDTR and SDTR negotiation disabled until the XPT has
	 * informed us of device capabilities, but do set the desired
	 * user rates in case we receive an SDTR request from the target
	 * before we negotiate.  We turn on tagged queuing at the microcode
	 * level for all devices, and modulate this on a per command basis.
	 */
	adw_lram_write_16(adw, ADW_MC_SDTR_SPEED1, adw->user_sdtr[0]);
	adw_lram_write_16(adw, ADW_MC_SDTR_SPEED2, adw->user_sdtr[1]);
	adw_lram_write_16(adw, ADW_MC_SDTR_SPEED3, adw->user_sdtr[2]);
	adw_lram_write_16(adw, ADW_MC_SDTR_SPEED4, adw->user_sdtr[3]);
	adw_lram_write_16(adw, ADW_MC_DISC_ENABLE, adw->user_discenb);
	for (tid = 0; tid < ADW_MAX_TID; tid++) {
		/* Cam limits the maximum number of commands for us */
		adw_lram_write_8(adw, ADW_MC_NUMBER_OF_MAX_CMD + tid,
				 adw->max_acbs);
	}
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
	 * Tell the MC about the memory size that
	 * was setup by the probe code.
	 */
	adw_lram_write_16(adw, ADW_MC_DEFAULT_MEM_CFG,
			  adw_inb(adw, ADW_MEM_CFG) & ADW_MEM_CFG_RAM_SZ_MASK);

	/*
	 * Determine SCSI_CFG1 Microcode Default Value.
	 *
	 * The microcode will set the SCSI_CFG1 register using this value
	 * after it is started below.
	 */
	scsicfg1 = adw_inw(adw, ADW_SCSI_CFG1);

	/*
	 * If the internal narrow cable is reversed all of the SCSI_CTRL
	 * register signals will be set. Check for and return an error if
	 * this condition is found.
	 */
	if ((adw_inw(adw, ADW_SCSI_CTRL) & 0x3F07) == 0x3F07) {
		printf("%s: Illegal Cable Config!\n", adw_name(adw));
		printf("%s: Internal cable is reversed!\n", adw_name(adw));
		return (EIO);
	}

	/*
	 * If this is a differential board and a single-ended device
	 * is attached to one of the connectors, return an error.
	 */
	if ((adw->features & ADW_ULTRA) != 0)  {
		if ((scsicfg1 & ADW_SCSI_CFG1_DIFF_MODE) != 0
		 && (scsicfg1 & ADW_SCSI_CFG1_DIFF_SENSE) == 0) {
			printf("%s: A Single Ended Device is attached to our "
			       "differential bus!\n", adw_name(adw));
		        return (EIO);
		}
	} else {
		if ((scsicfg1 & ADW2_SCSI_CFG1_DEV_DETECT_HVD) != 0) {
			printf("%s: A High Voltage Differential Device "
			       "is attached to this controller.\n",
			       adw_name(adw));
			printf("%s: HVD devices are not supported.\n",
			       adw_name(adw));
		        return (EIO);
		}
	}

	/*
	 * Perform automatic termination control if desired.
	 */
	if ((adw->features & ADW_ULTRA2) != 0) {
		u_int cable_det;

		/*
		 * Ultra2 Chips require termination disabled to
		 * detect cable presence.
		 */
		adw_outw(adw, ADW_SCSI_CFG1,
			 scsicfg1 | ADW2_SCSI_CFG1_DIS_TERM_DRV);
		cable_det = adw_inw(adw, ADW_SCSI_CFG1);
		adw_outw(adw, ADW_SCSI_CFG1, scsicfg1);

		/* SE Termination first if auto-term has been specified */
		if ((term_scsicfg1 & ADW_SCSI_CFG1_TERM_CTL_MASK) == 0) {

			/*
			 * For all SE cable configurations, high byte
			 * termination is enabled.
			 */
			term_scsicfg1 |= ADW_SCSI_CFG1_TERM_CTL_H;
			if ((cable_det & ADW_SCSI_CFG1_INT8_MASK) != 0
			 || (cable_det & ADW_SCSI_CFG1_INT16_MASK) != 0) {
				/*
				 * If either cable is not present, the
				 * low byte must be terminated as well.
				 */
				term_scsicfg1 |= ADW_SCSI_CFG1_TERM_CTL_L;
			}
		}

		/* LVD auto-term */
		if ((term_scsicfg1 & ADW2_SCSI_CFG1_TERM_CTL_LVD) == 0
		 && (term_scsicfg1 & ADW2_SCSI_CFG1_DIS_TERM_DRV) == 0) {
			/*
			 * If both cables are installed, termination
			 * is disabled.  Otherwise it is enabled.
			 */
			if ((cable_det & ADW2_SCSI_CFG1_EXTLVD_MASK) != 0
			 || (cable_det & ADW2_SCSI_CFG1_INTLVD_MASK) != 0) {

				term_scsicfg1 |= ADW2_SCSI_CFG1_TERM_CTL_LVD;
			}
		}
		term_scsicfg1 &= ~ADW2_SCSI_CFG1_DIS_TERM_DRV;
	} else {
		/* Ultra Controller Termination */
		if ((term_scsicfg1 & ADW_SCSI_CFG1_TERM_CTL_MASK) == 0) {
			int cable_count;
			int wide_cable_count;

			cable_count = 0;
			wide_cable_count = 0;
			if ((scsicfg1 & ADW_SCSI_CFG1_INT16_MASK) == 0) {
				cable_count++;
				wide_cable_count++;
			}
			if ((scsicfg1 & ADW_SCSI_CFG1_INT8_MASK) == 0)
				cable_count++;

			/* There is only one external port */
			if ((scsicfg1 & ADW_SCSI_CFG1_EXT16_MASK) == 0) {
				cable_count++;
				wide_cable_count++;
			} else if ((scsicfg1 & ADW_SCSI_CFG1_EXT8_MASK) == 0)
				cable_count++;

			if (cable_count == 3) {
				printf("%s: Illegal Cable Config!\n",
				       adw_name(adw));
				printf("%s: Only Two Ports may be used at "
				       "a time!\n", adw_name(adw));
			} else if (cable_count <= 1) {
				/*
				 * At least two out of three cables missing.
				 * Terminate both bytes.
				 */
				term_scsicfg1 |= ADW_SCSI_CFG1_TERM_CTL_H
					      |  ADW_SCSI_CFG1_TERM_CTL_L;
			} else if (wide_cable_count <= 1) {
				/* No two 16bit cables present.  High on. */
				term_scsicfg1 |= ADW_SCSI_CFG1_TERM_CTL_H;
			}
		}
        }

	/* Tell the user about our decission */
	switch (term_scsicfg1 & ADW_SCSI_CFG1_TERM_CTL_MASK) {
	case ADW_SCSI_CFG1_TERM_CTL_MASK:
		printf("High & Low SE Term Enabled, ");
		break;
	case ADW_SCSI_CFG1_TERM_CTL_H:
		printf("High SE Termination Enabled, ");
		break;
	case ADW_SCSI_CFG1_TERM_CTL_L:
		printf("Low SE Term Enabled, ");
		break;
	default:
		break;
	}

	if ((adw->features & ADW_ULTRA2) != 0
	 && (term_scsicfg1 & ADW2_SCSI_CFG1_TERM_CTL_LVD) != 0)
		printf("LVD Term Enabled, ");

	/*
	 * Invert the TERM_CTL_H and TERM_CTL_L bits and then
	 * set 'scsicfg1'. The TERM_POL bit does not need to be
	 * referenced, because the hardware internally inverts
	 * the Termination High and Low bits if TERM_POL is set.
	 */
	if ((adw->features & ADW_ULTRA2) != 0) {
		term_scsicfg1 = ~term_scsicfg1;
		term_scsicfg1 &= ADW_SCSI_CFG1_TERM_CTL_MASK
			      |  ADW2_SCSI_CFG1_TERM_CTL_LVD;
		scsicfg1 &= ~(ADW_SCSI_CFG1_TERM_CTL_MASK
			     |ADW2_SCSI_CFG1_TERM_CTL_LVD
			     |ADW_SCSI_CFG1_BIG_ENDIAN
			     |ADW_SCSI_CFG1_TERM_POL
			     |ADW2_SCSI_CFG1_DEV_DETECT);
		scsicfg1 |= term_scsicfg1;
	} else {
		term_scsicfg1 = ~term_scsicfg1 & ADW_SCSI_CFG1_TERM_CTL_MASK;
		scsicfg1 &= ~ADW_SCSI_CFG1_TERM_CTL_MASK;
		scsicfg1 |= term_scsicfg1 | ADW_SCSI_CFG1_TERM_CTL_MANUAL;
		scsicfg1 |= ADW_SCSI_CFG1_FLTR_DISABLE;
	}

	/*
	 * Set SCSI_CFG1 Microcode Default Value
	 *
	 * The microcode will set the SCSI_CFG1 register using this value
	 * after it is started below.
	 */
	adw_lram_write_16(adw, ADW_MC_DEFAULT_SCSI_CFG1, scsicfg1);

	/*
	 * Only accept selections on our initiator target id.
	 * This may change in target mode scenarios...
	 */
	adw_lram_write_16(adw, ADW_MC_DEFAULT_SEL_MASK,
			  (0x01 << adw->initiator_id));

	/*
	 * Tell the microcode where it can find our
	 * Initiator Command Queue (ICQ).  It is
	 * currently empty hence the "stopper" address.
	 */
	adw->commandq = adw->free_carriers;
	adw->free_carriers = carrierbotov(adw, adw->commandq->next_ba);
	adw->commandq->next_ba = ADW_CQ_STOPPER;
	adw_lram_write_32(adw, ADW_MC_ICQ, adw->commandq->carr_ba);

	/*
	 * Tell the microcode where it can find our
	 * Initiator Response Queue (IRQ).  It too
	 * is currently empty.
	 */
	adw->responseq = adw->free_carriers;
	adw->free_carriers = carrierbotov(adw, adw->responseq->next_ba);
	adw->responseq->next_ba = ADW_CQ_STOPPER;
	adw_lram_write_32(adw, ADW_MC_IRQ, adw->responseq->carr_ba);

	adw_outb(adw, ADW_INTR_ENABLES,
		 ADW_INTR_ENABLE_HOST_INTR|ADW_INTR_ENABLE_GLOBAL_INTR);

	adw_outw(adw, ADW_PC, adw_lram_read_16(adw, ADW_MC_CODE_BEGIN_ADDR));

	return (0);
}

void
adw_set_user_sdtr(struct adw_softc *adw, u_int tid, u_int mc_sdtr)
{
	adw->user_sdtr[ADW_TARGET_GROUP(tid)] &= ~ADW_TARGET_GROUP_MASK(tid);
	adw->user_sdtr[ADW_TARGET_GROUP(tid)] |=
	    mc_sdtr << ADW_TARGET_GROUP_SHIFT(tid);
}

u_int
adw_get_user_sdtr(struct adw_softc *adw, u_int tid)
{
	u_int mc_sdtr;

	mc_sdtr = adw->user_sdtr[ADW_TARGET_GROUP(tid)];
	mc_sdtr &= ADW_TARGET_GROUP_MASK(tid);
	mc_sdtr >>= ADW_TARGET_GROUP_SHIFT(tid);
	return (mc_sdtr);
}

void
adw_set_chip_sdtr(struct adw_softc *adw, u_int tid, u_int sdtr)
{
	u_int mc_sdtr_offset;
	u_int mc_sdtr;

	mc_sdtr_offset = ADW_MC_SDTR_SPEED1;
	mc_sdtr_offset += ADW_TARGET_GROUP(tid) * 2;
	mc_sdtr = adw_lram_read_16(adw, mc_sdtr_offset);
	mc_sdtr &= ~ADW_TARGET_GROUP_MASK(tid);
	mc_sdtr |= sdtr << ADW_TARGET_GROUP_SHIFT(tid);
	adw_lram_write_16(adw, mc_sdtr_offset, mc_sdtr);
}

u_int
adw_get_chip_sdtr(struct adw_softc *adw, u_int tid)
{
	u_int mc_sdtr_offset;
	u_int mc_sdtr;

	mc_sdtr_offset = ADW_MC_SDTR_SPEED1;
	mc_sdtr_offset += ADW_TARGET_GROUP(tid) * 2;
	mc_sdtr = adw_lram_read_16(adw, mc_sdtr_offset);
	mc_sdtr &= ADW_TARGET_GROUP_MASK(tid);
	mc_sdtr >>= ADW_TARGET_GROUP_SHIFT(tid);
	return (mc_sdtr);
}

u_int
adw_find_sdtr(struct adw_softc *adw, u_int period)
{
	int i;

	i = 0;
	if ((adw->features & ADW_DT) == 0)
		i = ADW_MC_SDTR_OFFSET_ULTRA2;
	if ((adw->features & ADW_ULTRA2) == 0)
		i = ADW_MC_SDTR_OFFSET_ULTRA;
	if (period == 0)
		return ADW_MC_SDTR_ASYNC;

	for (; i < adw_num_syncrates; i++) {
		if (period <= adw_syncrates[i].period)
			return (adw_syncrates[i].mc_sdtr);
	}	
	return ADW_MC_SDTR_ASYNC;
}

u_int
adw_find_period(struct adw_softc *adw, u_int mc_sdtr)
{
	int i;

	for (i = 0; i < adw_num_syncrates; i++) {
		if (mc_sdtr == adw_syncrates[i].mc_sdtr)
			break;
	}	
	return (adw_syncrates[i].period);
}

u_int
adw_hshk_cfg_period_factor(u_int tinfo)
{
	tinfo &= ADW_HSHK_CFG_RATE_MASK;
	tinfo >>= ADW_HSHK_CFG_RATE_SHIFT;
	if (tinfo == 0x11)
		/* 80MHz/DT */
		return (9);
	else if (tinfo == 0x10)
		/* 40MHz */
		return (10);
	else
		return (((tinfo * 25) + 50) / 4);
}

/*
 * Send an idle command to the chip and wait for completion.
 */
adw_idle_cmd_status_t
adw_idle_cmd_send(struct adw_softc *adw, adw_idle_cmd_t cmd, u_int parameter)
{
	u_int		      timeout;
	adw_idle_cmd_status_t status;
	int		      s;

	s = splcam();	

	/*
	 * Clear the idle command status which is set by the microcode
	 * to a non-zero value to indicate when the command is completed.
	 */
	adw_lram_write_16(adw, ADW_MC_IDLE_CMD_STATUS, 0);

	/*
	 * Write the idle command value after the idle command parameter
	 * has been written to avoid a race condition. If the order is not
	 * followed, the microcode may process the idle command before the
	 * parameters have been written to LRAM.
	 */
	adw_lram_write_32(adw, ADW_MC_IDLE_CMD_PARAMETER, parameter);
    	adw_lram_write_16(adw, ADW_MC_IDLE_CMD, cmd);

	/*
	 * Tickle the RISC to tell it to process the idle command.
	 */
	adw_tickle_risc(adw, ADW_TICKLE_B);

	/* Wait for up to 10 seconds for the command to complete */
	timeout = 5000000;
	while (--timeout) {
		status = adw_lram_read_16(adw, ADW_MC_IDLE_CMD_STATUS);
       		if (status != 0)
			break;
		DELAY(20);
	}

	if (timeout == 0)
		panic("%s: Idle Command Timed Out!\n", adw_name(adw));
	splx(s);
	return (status);
}
