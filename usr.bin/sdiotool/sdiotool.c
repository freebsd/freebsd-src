/*-
 * Copyright (c) 2016-2017 Ilya Bakulin
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/ioctl.h>
#include <sys/stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/endian.h>
#include <sys/sbuf.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <limits.h>
#include <fcntl.h>
#include <ctype.h>
#include <err.h>
#include <libutil.h>
#include <unistd.h>

#include <cam/cam.h>
#include <cam/cam_debug.h>
#include <cam/cam_ccb.h>
#include <cam/mmc/mmc_all.h>
#include <camlib.h>

struct cis_info {
	uint16_t man_id;
	uint16_t prod_id;
	uint16_t max_block_size;
};

static int sdio_rw_direct(struct cam_device *dev,
			  uint8_t func_number,
			  uint32_t addr,
			  uint8_t is_write,
			  uint8_t *data,
			  uint8_t *resp);
static uint8_t sdio_read_1(struct cam_device *dev, uint8_t func_number, uint32_t addr);
static void sdio_write_1(struct cam_device *dev, uint8_t func_number, uint32_t addr, uint8_t val);
static int sdio_is_func_ready(struct cam_device *dev, uint8_t func_number, uint8_t *is_enab);
static int sdio_is_func_enabled(struct cam_device *dev, uint8_t func_number, uint8_t *is_enab);
static int sdio_func_enable(struct cam_device *dev, uint8_t func_number, int enable);
static int sdio_is_func_intr_enabled(struct cam_device *dev, uint8_t func_number, uint8_t *is_enab);
static int sdio_func_intr_enable(struct cam_device *dev, uint8_t func_number, int enable);
static void sdio_card_reset(struct cam_device *dev);
static uint32_t sdio_get_common_cis_addr(struct cam_device *dev);
static void probe_bcrm(struct cam_device *dev);

/* Use CMD52 to read or write a single byte */
int
sdio_rw_direct(struct cam_device *dev,
	       uint8_t func_number,
	       uint32_t addr,
	       uint8_t is_write,
	       uint8_t *data, uint8_t *resp) {
	union ccb *ccb;
	uint32_t flags;
	uint32_t arg;
	int retval = 0;

	ccb = cam_getccb(dev);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		return (1);
	}
	bzero(&(&ccb->ccb_h)[1],
	      sizeof(union ccb) - sizeof(struct ccb_hdr));

	flags = MMC_RSP_R5 | MMC_CMD_AC;
	arg = SD_IO_RW_FUNC(func_number) | SD_IO_RW_ADR(addr);
	if (is_write)
		arg |= SD_IO_RW_WR | SD_IO_RW_RAW | SD_IO_RW_DAT(*data);

	cam_fill_mmcio(&ccb->mmcio,
		       /*retries*/ 0,
		       /*cbfcnp*/ NULL,
		       /*flags*/ CAM_DIR_NONE,
		       /*mmc_opcode*/ SD_IO_RW_DIRECT,
		       /*mmc_arg*/ arg,
		       /*mmc_flags*/ flags,
		       /*mmc_data*/ 0,
		       /*timeout*/ 5000);

	if (((retval = cam_send_ccb(dev, ccb)) < 0)
	    || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		const char warnstr[] = "error sending command";

		if (retval < 0)
			warn(warnstr);
		else
			warnx(warnstr);
		return (-1);
	}

	*resp = ccb->mmcio.cmd.resp[0] & 0xFF;
	cam_freeccb(ccb);
	return (retval);
}

#if 0
/*
 * CMD53 -- IO_RW_EXTENDED
 * Use to read or write memory blocks
 *
 * is_increment=1: FIFO mode
 * blk_count > 0: block mode
 */
int
sdio_rw_extended(struct cam_device *dev,
		 uint8_t func_number,
		 uint32_t addr,
		 uint8_t is_write,
		 uint8_t *data, size_t datalen,
		 uint8_t is_increment,
		 uint16_t blk_count) {
	union ccb *ccb;
	uint32_t flags;
	uint32_t arg;
	int retval = 0;

	if (blk_count != 0) {
		warnx("%s: block mode is not supported yet", __func__);
		return (1);
	}

	ccb = cam_getccb(dev);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		return (1);
	}
	bzero(&(&ccb->ccb_h)[1],
	      sizeof(union ccb) - sizeof(struct ccb_hdr));

	flags = MMC_RSP_R5 | MMC_CMD_AC;
	arg = SD_IO_RW_FUNC(func_number) | SD_IO_RW_ADR(addr);
	if (is_write)
		arg |= SD_IO_RW_WR;

	cam_fill_mmcio(&ccb->mmcio,
		       /*retries*/ 0,
		       /*cbfcnp*/ NULL,
		       /*flags*/ CAM_DIR_NONE,
		       /*mmc_opcode*/ SD_IO_RW_DIRECT,
		       /*mmc_arg*/ arg,
		       /*mmc_flags*/ flags,
		       /*mmc_data*/ 0,
		       /*timeout*/ 5000);

	if (((retval = cam_send_ccb(dev, ccb)) < 0)
	    || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		const char warnstr[] = "error sending command";

		if (retval < 0)
			warn(warnstr);
		else
			warnx(warnstr);
		return (-1);
	}

	*resp = ccb->mmcio.cmd.resp[0] & 0xFF;
	cam_freeccb(ccb);
	return (retval);
}
#endif

static int
sdio_read_bool_for_func(struct cam_device *dev, uint32_t addr, uint8_t func_number, uint8_t *is_enab) {
	uint8_t resp;
	int ret;

	ret = sdio_rw_direct(dev, 0, addr, 0, NULL, &resp);
	if (ret < 0)
		return ret;

	*is_enab = (resp & (1 << func_number)) > 0 ? 1 : 0;

	return (0);
}

static int
sdio_set_bool_for_func(struct cam_device *dev, uint32_t addr, uint8_t func_number, int enable) {
	uint8_t resp;
	int ret;
	uint8_t is_enabled;

	ret = sdio_rw_direct(dev, 0, addr, 0, NULL, &resp);
	if (ret != 0)
		return ret;

	is_enabled = resp & (1 << func_number);
	if ((is_enabled !=0 && enable == 1) || (is_enabled == 0 && enable == 0))
		return 0;

	if (enable)
		resp |= 1 << func_number;
	else
		resp &= ~ (1 << func_number);

	ret = sdio_rw_direct(dev, 0, addr, 1, &resp, &resp);

	return ret;
}

static uint8_t
sdio_read_1(struct cam_device *dev, uint8_t func_number, uint32_t addr) {
	uint8_t val;
	sdio_rw_direct(dev, func_number, addr, 0, NULL, &val);
	return val;
}

__unused static void
sdio_write_1(struct cam_device *dev, uint8_t func_number, uint32_t addr, uint8_t val) {
	uint8_t _val;
	sdio_rw_direct(dev, func_number, addr, 0, &val, &_val);
}

static int
sdio_is_func_ready(struct cam_device *dev, uint8_t func_number, uint8_t *is_enab) {
	return sdio_read_bool_for_func(dev, SD_IO_CCCR_FN_READY, func_number, is_enab);
}

static int
sdio_is_func_enabled(struct cam_device *dev, uint8_t func_number, uint8_t *is_enab) {
	return sdio_read_bool_for_func(dev, SD_IO_CCCR_FN_ENABLE, func_number, is_enab);
}

static int
sdio_func_enable(struct cam_device *dev, uint8_t func_number, int enable) {
	return sdio_set_bool_for_func(dev, SD_IO_CCCR_FN_ENABLE, func_number, enable);
}

static int
sdio_is_func_intr_enabled(struct cam_device *dev, uint8_t func_number, uint8_t *is_enab) {
	return sdio_read_bool_for_func(dev, SD_IO_CCCR_INT_ENABLE, func_number, is_enab);
}

static int
sdio_func_intr_enable(struct cam_device *dev, uint8_t func_number, int enable) {
	return sdio_set_bool_for_func(dev, SD_IO_CCCR_INT_ENABLE, func_number, enable);
}

static int
sdio_card_set_bus_width(struct cam_device *dev, enum mmc_bus_width bw) {
	int ret;
	uint8_t ctl_val;
	ret = sdio_rw_direct(dev, 0, SD_IO_CCCR_BUS_WIDTH, 0, NULL, &ctl_val);
	if (ret < 0) {
		warn("Error getting CCCR_BUS_WIDTH value");
		return ret;
	}
	ctl_val &= ~0x3;
	switch (bw) {
	case bus_width_1:
		/* Already set to 1-bit */
		break;
	case bus_width_4:
		ctl_val |= CCCR_BUS_WIDTH_4;
		break;
	case bus_width_8:
		warn("Cannot do 8-bit on SDIO yet");
		return -1;
		break;
	}
	ret = sdio_rw_direct(dev, 0, SD_IO_CCCR_BUS_WIDTH, 1, &ctl_val, &ctl_val);
	if (ret < 0) {
		warn("Error setting CCCR_BUS_WIDTH value");
		return ret;
	}
	return ret;
}

static int
sdio_func_read_cis(struct cam_device *dev, uint8_t func_number,
		   uint32_t cis_addr, struct cis_info *info) {
	uint8_t tuple_id, tuple_len, tuple_count;
	uint32_t addr;

	char *cis1_info[4];
	int start, i, ch, count;
	char cis1_info_buf[256];

	tuple_count = 0; /* Use to prevent infinite loop in case of parse errors */
	memset(cis1_info_buf, 0, 256);
	do {
		addr = cis_addr;
		tuple_id = sdio_read_1(dev, 0, addr++);
		if (tuple_id == SD_IO_CISTPL_END)
			break;
		if (tuple_id == 0) {
			cis_addr++;
			continue;
		}
		tuple_len = sdio_read_1(dev, 0, addr++);
		if (tuple_len == 0 && tuple_id != 0x00) {
			warn("Parse error: 0-length tuple %02X\n", tuple_id);
			return -1;
		}

		switch (tuple_id) {
		case SD_IO_CISTPL_VERS_1:
			addr += 2;
			for (count = 0, start = 0, i = 0;
			     (count < 4) && ((i + 4) < 256); i++) {
				ch = sdio_read_1(dev, 0, addr + i);
				printf("count=%d, start=%d, i=%d, Got %c (0x%02x)\n", count, start, i, ch, ch);
				if (ch == 0xff)
					break;
				cis1_info_buf[i] = ch;
				if (ch == 0) {
					cis1_info[count] =
						cis1_info_buf + start;
					start = i + 1;
					count++;
				}
			}
			printf("Card info:");
			for (i=0; i<4; i++)
				if (cis1_info[i])
					printf(" %s", cis1_info[i]);
			printf("\n");
			break;
		case SD_IO_CISTPL_MANFID:
			info->man_id =  sdio_read_1(dev, 0, addr++);
			info->man_id |= sdio_read_1(dev, 0, addr++) << 8;

			info->prod_id =  sdio_read_1(dev, 0, addr++);
			info->prod_id |= sdio_read_1(dev, 0, addr++) << 8;
			break;
		case SD_IO_CISTPL_FUNCID:
			/* not sure if we need to parse it? */
			break;
		case SD_IO_CISTPL_FUNCE:
			if (tuple_len < 4) {
				printf("FUNCE is too short: %d\n", tuple_len);
				break;
			}
			if (func_number == 0) {
				/* skip extended_data */
				addr++;
				info->max_block_size  = sdio_read_1(dev, 0, addr++);
				info->max_block_size |= sdio_read_1(dev, 0, addr++) << 8;
			} else {
				info->max_block_size  = sdio_read_1(dev, 0, addr + 0xC);
				info->max_block_size |= sdio_read_1(dev, 0, addr + 0xD) << 8;
			}
			break;
		default:
			printf("Skipping tuple ID %02X len %02X\n", tuple_id, tuple_len);
		}
		cis_addr += tuple_len + 2;
		tuple_count++;
	} while (tuple_count < 20);

	return 0;
}

static uint32_t
sdio_get_common_cis_addr(struct cam_device *dev) {
	uint32_t addr;

	addr =  sdio_read_1(dev, 0, SD_IO_CCCR_CISPTR);
	addr |= sdio_read_1(dev, 0, SD_IO_CCCR_CISPTR + 1) << 8;
	addr |= sdio_read_1(dev, 0, SD_IO_CCCR_CISPTR + 2) << 16;

	if (addr < SD_IO_CIS_START || addr > SD_IO_CIS_START + SD_IO_CIS_SIZE) {
		warn("Bad CIS address: %04X\n", addr);
		addr = 0;
	}

	return addr;
}

static void sdio_card_reset(struct cam_device *dev) {
	int ret;
	uint8_t ctl_val;
	ret = sdio_rw_direct(dev, 0, SD_IO_CCCR_CTL, 0, NULL, &ctl_val);
	if (ret < 0)
		errx(1, "Error getting CCCR_CTL value");
	ctl_val |= CCCR_CTL_RES;
	ret = sdio_rw_direct(dev, 0, SD_IO_CCCR_CTL, 1, &ctl_val, &ctl_val);
	if (ret < 0)
		errx(1, "Error setting CCCR_CTL value");
}

/*
 * How Linux driver works
 *
 * The probing begins by calling brcmf_ops_sdio_probe() which is defined as probe function in struct sdio_driver. http://lxr.free-electrons.com/source/drivers/net/wireless/broadcom/brcm80211/brcmfmac/bcmsdh.c#L1126
 *
 * The driver does black magic by copying func struct for F2 and setting func number to zero there, to create an F0 func structure :)
 * Driver state changes to BRCMF_SDIOD_DOWN.
 * ops_sdio_probe() then calls brcmf_sdio_probe() -- at this point it has filled in sdiodev struct with the pointers to all three functions (F0, F1, F2).
 *
 * brcmf_sdiod_probe() sets block sizes for F1 and F2. It sets F1 block size to 64 and F2 to 512, not consulting the values stored in SDIO CCCR  / FBR registers!
 * Then it increases timeout for F2 (what is this?!)
 * Then it enables F1
 * Then it attaches "freezer" (without PM this is NOP)
 * Finally it calls brcmf_sdio_probe() http://lxr.free-electrons.com/source/drivers/net/wireless/broadcom/brcm80211/brcmfmac/sdio.c#L4082
 *
 * Here high-level workqueues and sg tables are allocated.
 * It then calls brcmf_sdio_probe_attach()
 *
 * Here at the beginning there is a pr_debug() call with brcmf_sdiod_regrl() inside to addr #define SI_ENUM_BASE            0x18000000.
 * Return value is 0x16044330.
 * Then turns off PLL:  byte-write BRCMF_INIT_CLKCTL1 (0x28) ->  SBSDIO_FUNC1_CHIPCLKCSR (0x1000E)
 * Then it reads value back, should be 0xe8.
 * Then calls brcmf_chip_attach()
 *
 * http://lxr.free-electrons.com/source/drivers/net/wireless/broadcom/brcm80211/brcmfmac/chip.c#L1054
 * This func enumerates and resets all the cores on the dongle.
 *  - brcmf_sdio_buscoreprep(): force clock to ALPAvail req only:
 *    SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ -> SBSDIO_FUNC1_CHIPCLKCSR
 * Wait up to 15ms to !SBSDIO_ALPAV(clkval) of the value from CLKCSR.
 * Force ALP:
 *    SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_FORCE_ALP (0x21)-> SBSDIO_FUNC1_CHIPCLKCSR
 * Disaable SDIO pullups:
 * byte 0 -> SBSDIO_FUNC1_SDIOPULLUP (0x0001000f)
 *
 *  Calls brcmf_chip_recognition()
 * http://lxr.free-electrons.com/source/drivers/net/wireless/broadcom/brcm80211/brcmfmac/chip.c#L908
 * Read 0x18000000. Get 0x16044330: chip 4330 rev 4
 * AXI chip, call  brcmf_chip_dmp_erom_scan() to get info about all cores.
 * Then  brcmf_chip_cores_check() to check that CPU and RAM are found,
 *
 * Setting cores to passive: not clear which of CR4/CA7/CM3 our chip has.
 *  Quite a few r/w calls to different parts of the chip to reset cores....
 * Finally get_raminfo() called to fill in RAM info:
 * brcmf_chip_get_raminfo: RAM: base=0x0 size=294912 (0x48000) sr=0 (0x0)
 * http://lxr.free-electrons.com/source/drivers/net/wireless/broadcom/brcm80211/brcmfmac/chip.c#L700
 *
 * Then brcmf_chip_setup() is called, this prints and fills in chipcommon rev and PMU caps:
 *   brcmf_chip_setup: ccrev=39, pmurev=12, pmucaps=0x19583c0c
 * http://lxr.free-electrons.com/source/drivers/net/wireless/broadcom/brcm80211/brcmfmac/chip.c#L1015
 *  Bus-specific setup code is NOP for SDIO.
 *
 * brcmf_sdio_kso_init() is called.
 * Here it first reads 0x1 from SBSDIO_FUNC1_SLEEPCSR 0x18000650 and then writes it back... WTF?
 *
 * brcmf_sdio_drivestrengthinit() is called
 * http://lxr.free-electrons.com/source/drivers/net/wireless/broadcom/brcm80211/brcmfmac/sdio.c#L3630
 *
 * Set card control so an SDIO card reset does a WLAN backplane reset
 * set PMUControl so a backplane reset does PMU state reload
 * === end of brcmf_sdio_probe_attach ===

 **** Finished reading at http://lxr.free-electrons.com/source/drivers/net/wireless/broadcom/brcm80211/brcmfmac/sdio.c#L4152, line 2025 in the dump

 * === How register reading works ===
 * http://lxr.free-electrons.com/source/drivers/net/wireless/broadcom/brcm80211/brcmfmac/bcmsdh.c#L357
 * The address to read from is written to three byte-sized registers of F1:
 *  - SBSDIO_FUNC1_SBADDRLOW  0x1000A
 *  - SBSDIO_FUNC1_SBADDRMID  0x1000B
 *  - SBSDIO_FUNC1_SBADDRHIGH 0x1000C
 * If this is 32-bit read , a flag is set. The address is ANDed with SBSDIO_SB_OFT_ADDR_MASK which is 0x07FFF.
 * Then brcmf_sdiod_regrw_helper() is called to read the reply.
 * http://lxr.free-electrons.com/source/drivers/net/wireless/broadcom/brcm80211/brcmfmac/bcmsdh.c#L306
 * Based on the address it figures out where to read it from (CCCR / FBR in F0, or somewhere in F1).
 * Reads are retried three times.
 * 1-byte IO is done with CMD52, more is read with CMD53 with address increment (not FIFO mode).
 * http://lxr.free-electrons.com/source/drivers/mmc/core/sdio_io.c#L458
 * ==================================
 *
 *
 */
__unused
static void
probe_bcrm(struct cam_device *dev) {
	uint32_t cis_addr;
	struct cis_info info;

	sdio_card_set_bus_width(dev, bus_width_4);
	cis_addr = sdio_get_common_cis_addr(dev);
	printf("CIS address: %04X\n", cis_addr);

	memset(&info, 0, sizeof(info));
	sdio_func_read_cis(dev, 0, cis_addr, &info);
	printf("Vendor 0x%04X product 0x%04X\n", info.man_id, info.prod_id);
}
__unused
static uint8_t *
mmap_fw() {
	const char fw_path[] = "/home/kibab/repos/fbsd-bbb/brcm-firmware/brcmfmac4330-sdio.bin";
	struct stat sb;
	uint8_t *fw_ptr;

	int fd = open(fw_path, O_RDONLY);
	if (fd < 0)
		errx(1, "Cannot open firmware file");
	if (fstat(fd, &sb) < 0)
		errx(1, "Cannot get file stat");
	fw_ptr = mmap(NULL, sb.st_size, PROT_READ, 0, fd, 0);
	if (fw_ptr == MAP_FAILED)
		errx(1, "Cannot map the file");

	return fw_ptr;
}

static void
usage() {
	printf("sdiotool -u <pass_dev_unit>\n");
	exit(0);
}

static void
get_sdio_card_info(struct cam_device *dev) {
	uint32_t cis_addr;
	uint32_t fbr_addr;
	struct cis_info info;

	cis_addr = sdio_get_common_cis_addr(dev);

	memset(&info, 0, sizeof(info));
	sdio_func_read_cis(dev, 0, cis_addr, &info);
	printf("F0: Vendor 0x%04X product 0x%04X max block size %d bytes\n",
	       info.man_id, info.prod_id, info.max_block_size);
	for (int i = 1; i <= 2; i++) {
		fbr_addr = SD_IO_FBR_START * i + 0x9;
		cis_addr =  sdio_read_1(dev, 0, fbr_addr++);
		cis_addr |= sdio_read_1(dev, 0, fbr_addr++) << 8;
		cis_addr |= sdio_read_1(dev, 0, fbr_addr++) << 16;
		memset(&info, 0, sizeof(info));
		sdio_func_read_cis(dev, i, cis_addr, &info);
		printf("F%d: Vendor 0x%04X product 0x%04X max block size %d bytes\n",
		       i, info.man_id, info.prod_id, info.max_block_size);
	}
}

/* Test interrupt delivery when select() */
__unused static int
sdio_signal_intr(struct cam_device *dev) {
	uint8_t resp;
	int ret;

	ret = sdio_rw_direct(dev, 0, 0x666, 0, NULL, &resp);
	if (ret < 0)
		return ret;
	return (0);
}

static void
do_intr_test(__unused struct cam_device *dev) {
}

int
main(int argc, char **argv) {
	char device[] = "pass";
	int unit = 0;
	int func = 0;
	uint8_t resp;
	uint8_t is_enab;
	__unused uint8_t *fw_ptr;
	int ch;
	struct cam_device *cam_dev;
	int is_intr_test = 0;

	//fw_ptr = mmap_fw();

	while ((ch = getopt(argc, argv, "Iu:")) != -1) {
		switch (ch) {
		case 'u':
			unit = (int) strtol(optarg, NULL, 10);
			break;
		case 'f':
			func = (int) strtol(optarg, NULL, 10);
		case 'I':
			is_intr_test = 1;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if ((cam_dev = cam_open_spec_device(device, unit, O_RDWR, NULL)) == NULL)
		errx(1, "Cannot open device");

	get_sdio_card_info(cam_dev);
	if (is_intr_test > 0)
		do_intr_test(cam_dev);
	exit(0);
	sdio_card_reset(cam_dev);

	/* Read Addr 7 of func 0 */
	int ret = sdio_rw_direct(cam_dev, 0, 7, 0, NULL, &resp);
	if (ret < 0)
		errx(1, "Error sending CAM command");
	printf("Result: %02x\n", resp);

	/* Check if func 1 is enabled */
	ret = sdio_is_func_enabled(cam_dev, 1, &is_enab);
	if (ret < 0)
		errx(1, "Cannot check if func is enabled");
	printf("F1 enabled: %d\n", is_enab);
	ret = sdio_func_enable(cam_dev, 1, 1 - is_enab);
	if (ret < 0)
		errx(1, "Cannot enable/disable func");
	printf("F1 en/dis result: %d\n", ret);

	/* Check if func 1 is ready */
	ret = sdio_is_func_ready(cam_dev, 1, &is_enab);
	if (ret < 0)
		errx(1, "Cannot check if func is ready");
	printf("F1 ready: %d\n", is_enab);

	/* Check if interrupts are enabled */
	ret = sdio_is_func_intr_enabled(cam_dev, 1, &is_enab);
	if (ret < 0)
		errx(1, "Cannot check if func intr is enabled");
	printf("F1 intr enabled: %d\n", is_enab);
	ret = sdio_func_intr_enable(cam_dev, 1, 1 - is_enab);
	if (ret < 0)
		errx(1, "Cannot enable/disable func intr");
	printf("F1 intr en/dis result: %d\n", ret);

	cam_close_spec_device(cam_dev);
}
