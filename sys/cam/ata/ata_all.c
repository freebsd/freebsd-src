/*-
 * Copyright (c) 2009 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#ifdef _KERNEL
#include <opt_scsi.h>

#include <sys/systm.h>
#include <sys/libkern.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#else
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#endif

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_queue.h>
#include <cam/cam_xpt.h>
#include <sys/ata.h>
#include <cam/ata/ata_all.h>
#include <sys/sbuf.h>
#include <sys/endian.h>

int
ata_version(int ver)
{
	int bit;

	if (ver == 0xffff)
		return 0;
	for (bit = 15; bit >= 0; bit--)
		if (ver & (1<<bit))
			return bit;
	return 0;
}

char *
ata_op_string(struct ata_cmd *cmd)
{

	switch (cmd->command) {
	case 0x00: return ("NOP");
	case 0x03: return ("CFA_REQUEST_EXTENDED_ERROR");
	case 0x08: return ("DEVICE_RESET");
	case 0x20: return ("READ");
	case 0x24: return ("READ48");
	case 0x25: return ("READ_DMA48");
	case 0x26: return ("READ_DMA_QUEUED48");
	case 0x27: return ("READ_NATIVE_MAX_ADDRESS48");
	case 0x29: return ("READ_MUL48");
	case 0x2a: return ("READ_STREAM_DMA48");
	case 0x2b: return ("READ_STREAM48");
	case 0x2f: return ("READ_LOG_EXT");
	case 0x30: return ("WRITE");
	case 0x34: return ("WRITE48");
	case 0x35: return ("WRITE_DMA48");
	case 0x36: return ("WRITE_DMA_QUEUED48");
	case 0x37: return ("SET_MAX_ADDRESS48");
	case 0x39: return ("WRITE_MUL48");
	case 0x3a: return ("WRITE_STREAM_DMA48");
	case 0x3b: return ("WRITE_STREAM48");
	case 0x3d: return ("WRITE_DMA_FUA");
	case 0x3e: return ("WRITE_DMA_FUA48");
	case 0x3f: return ("WRITE_LOG_EXT");
	case 0x40: return ("READ_VERIFY");
	case 0x42: return ("READ_VERIFY48");
	case 0x51: return ("CONFIGURE_STREAM");
	case 0x60: return ("READ_FPDMA_QUEUED");
	case 0x61: return ("WRITE_FPDMA_QUEUED");
	case 0x70: return ("SEEK");
	case 0x87: return ("CFA_TRANSLATE_SECTOR");
	case 0x90: return ("EXECUTE_DEVICE_DIAGNOSTIC");
	case 0x92: return ("DOWNLOAD_MICROCODE");
	case 0xa0: return ("PACKET");
	case 0xa1: return ("ATAPI_IDENTIFY");
	case 0xa2: return ("SERVICE");
	case 0xb0: return ("SMART");
	case 0xb1: return ("DEVICE CONFIGURATION");
	case 0xc0: return ("CFA_ERASE");
	case 0xc4: return ("READ_MUL");
	case 0xc5: return ("WRITE_MUL");
	case 0xc6: return ("SET_MULTI");
	case 0xc7: return ("READ_DMA_QUEUED");
	case 0xc8: return ("READ_DMA");
	case 0xca: return ("WRITE_DMA");
	case 0xcc: return ("WRITE_DMA_QUEUED");
	case 0xcd: return ("CFA_WRITE_MULTIPLE_WITHOUT_ERASE");
	case 0xce: return ("WRITE_MULTIPLE_FUA48");
	case 0xd1: return ("CHECK_MEDIA_CARD_TYPE");
	case 0xda: return ("GET_MEDIA_STATUS");
	case 0xde: return ("MEDIA_LOCK");
	case 0xdf: return ("MEDIA_UNLOCK");
	case 0xe0: return ("STANDBY_IMMEDIATE");
	case 0xe1: return ("IDLE_IMMEDIATE");
	case 0xe2: return ("STANDBY");
	case 0xe3: return ("IDLE");
	case 0xe4: return ("READ_BUFFER/PM");
	case 0xe5: return ("CHECK_POWER_MODE");
	case 0xe6: return ("SLEEP");
	case 0xe7: return ("FLUSHCACHE");
	case 0xe8: return ("WRITE_PM");
	case 0xea: return ("FLUSHCACHE48");
	case 0xec: return ("ATA_IDENTIFY");
	case 0xed: return ("MEDIA_EJECT");
	case 0xef:
		switch (cmd->features) {
	        case 0x03: return ("SETFEATURES SET TRANSFER MODE");
	        case 0x02: return ("SETFEATURES ENABLE WCACHE");
	        case 0x82: return ("SETFEATURES DISABLE WCACHE");
	        case 0xaa: return ("SETFEATURES ENABLE RCACHE");
	        case 0x55: return ("SETFEATURES DISABLE RCACHE");
	        }
	        return "SETFEATURES";
	case 0xf1: return ("SECURITY_SET_PASSWORD");
	case 0xf2: return ("SECURITY_UNLOCK");
	case 0xf3: return ("SECURITY_ERASE_PREPARE");
	case 0xf4: return ("SECURITY_ERASE_UNIT");
	case 0xf5: return ("SECURITY_FREE_LOCK");
	case 0xf6: return ("SECURITY DISABLE PASSWORD");
	case 0xf8: return ("READ_NATIVE_MAX_ADDRESS");
	case 0xf9: return ("SET_MAX_ADDRESS");
	}
	return "UNKNOWN";
}

char *
ata_cmd_string(struct ata_cmd *cmd, char *cmd_string, size_t len)
{

	snprintf(cmd_string, len, "%02x %02x %02x %02x "
	    "%02x %02x %02x %02x %02x %02x %02x %02x",
	    cmd->command, cmd->features,
	    cmd->lba_low, cmd->lba_mid, cmd->lba_high, cmd->device,
	    cmd->lba_low_exp, cmd->lba_mid_exp, cmd->lba_high_exp,
	    cmd->features_exp, cmd->sector_count, cmd->sector_count_exp);

	return(cmd_string);
}

char *
ata_res_string(struct ata_res *res, char *res_string, size_t len)
{

	snprintf(res_string, len, "%02x %02x %02x %02x "
	    "%02x %02x %02x %02x %02x %02x %02x",
	    res->status, res->error,
	    res->lba_low, res->lba_mid, res->lba_high, res->device,
	    res->lba_low_exp, res->lba_mid_exp, res->lba_high_exp,
	    res->sector_count, res->sector_count_exp);

	return(res_string);
}

/*
 * ata_command_sbuf() returns 0 for success and -1 for failure.
 */
int
ata_command_sbuf(struct ccb_ataio *ataio, struct sbuf *sb)
{
	char cmd_str[(12 * 3) + 1];

	sbuf_printf(sb, "CMD: %s: %s",
	    ata_op_string(&ataio->cmd),
	    ata_cmd_string(&ataio->cmd, cmd_str, sizeof(cmd_str)));

	return(0);
}

/*
 * ata_status_abuf() returns 0 for success and -1 for failure.
 */
int
ata_status_sbuf(struct ccb_ataio *ataio, struct sbuf *sb)
{

	sbuf_printf(sb, "ATA Status: %02x (%s%s%s%s%s%s%s%s)",
	    ataio->res.status,
	    (ataio->res.status & 0x80) ? "BSY " : "",
	    (ataio->res.status & 0x40) ? "DRDY " : "",
	    (ataio->res.status & 0x20) ? "DF " : "",
	    (ataio->res.status & 0x10) ? "SERV " : "",
	    (ataio->res.status & 0x08) ? "DRQ " : "",
	    (ataio->res.status & 0x04) ? "CORR " : "",
	    (ataio->res.status & 0x02) ? "IDX " : "",
	    (ataio->res.status & 0x01) ? "ERR" : "");
	if (ataio->res.status & 1) {
	    sbuf_printf(sb, ", Error: %02x (%s%s%s%s%s%s%s%s)",
		ataio->res.error,
		(ataio->res.error & 0x80) ? "ICRC " : "",
		(ataio->res.error & 0x40) ? "UNC " : "",
		(ataio->res.error & 0x20) ? "MC " : "",
		(ataio->res.error & 0x10) ? "IDNF " : "",
		(ataio->res.error & 0x08) ? "MCR " : "",
		(ataio->res.error & 0x04) ? "ABRT " : "",
		(ataio->res.error & 0x02) ? "NM " : "",
		(ataio->res.error & 0x01) ? "ILI" : "");
	}

	return(0);
}

/*
 * ata_res_sbuf() returns 0 for success and -1 for failure.
 */
int
ata_res_sbuf(struct ccb_ataio *ataio, struct sbuf *sb)
{
	char res_str[(11 * 3) + 1];

	sbuf_printf(sb, "RES: %s",
	    ata_res_string(&ataio->res, res_str, sizeof(res_str)));

	return(0);
}

void
ata_print_ident(struct ata_params *ident_data)
{
	char product[48], revision[16];

	cam_strvis(product, ident_data->model, sizeof(ident_data->model),
		   sizeof(product));
	cam_strvis(revision, ident_data->revision, sizeof(ident_data->revision),
		   sizeof(revision));
	printf("<%s %s> ATA/ATAPI-%d",
	    product, revision, ata_version(ident_data->version_major));
	if (ident_data->satacapabilities && ident_data->satacapabilities != 0xffff) {
		if (ident_data->satacapabilities & ATA_SATA_GEN3)
			printf(" SATA 3.x");
		else if (ident_data->satacapabilities & ATA_SATA_GEN2)
			printf(" SATA 2.x");
		else if (ident_data->satacapabilities & ATA_SATA_GEN1)
			printf(" SATA 1.x");
		else
			printf(" SATA");
	}
	printf(" device\n");
}

uint32_t
ata_logical_sector_size(struct ata_params *ident_data)
{
	if ((ident_data->pss & 0xc000) == 0x4000 &&
	    (ident_data->pss & ATA_PSS_LSSABOVE512)) {
		return ((u_int32_t)ident_data->lss_1 |
		    ((u_int32_t)ident_data->lss_2 << 16));
	}
	return (512);
}

uint64_t
ata_physical_sector_size(struct ata_params *ident_data)
{
	if ((ident_data->pss & 0xc000) == 0x4000 &&
	    (ident_data->pss & ATA_PSS_MULTLS)) {
		return ((uint64_t)ata_logical_sector_size(ident_data) *
		    (1 << (ident_data->pss & ATA_PSS_LSPPS)));
	}
	return (512);
}

uint64_t
ata_logical_sector_offset(struct ata_params *ident_data)
{
	if ((ident_data->lsalign & 0xc000) == 0x4000) {
		return ((uint64_t)ata_logical_sector_size(ident_data) *
		    (ident_data->lsalign & 0x3fff));
	}
	return (0);
}

void
ata_28bit_cmd(struct ccb_ataio *ataio, uint8_t cmd, uint8_t features,
    uint32_t lba, uint8_t sector_count)
{
	bzero(&ataio->cmd, sizeof(ataio->cmd));
	ataio->cmd.flags = 0;
	ataio->cmd.command = cmd;
	ataio->cmd.features = features;
	ataio->cmd.lba_low = lba;
	ataio->cmd.lba_mid = lba >> 8;
	ataio->cmd.lba_high = lba >> 16;
	ataio->cmd.device = 0x40 | ((lba >> 24) & 0x0f);
	ataio->cmd.sector_count = sector_count;
}

void
ata_48bit_cmd(struct ccb_ataio *ataio, uint8_t cmd, uint16_t features,
    uint64_t lba, uint16_t sector_count)
{
	bzero(&ataio->cmd, sizeof(ataio->cmd));
	ataio->cmd.flags = CAM_ATAIO_48BIT;
	ataio->cmd.command = cmd;
	ataio->cmd.features = features;
	ataio->cmd.lba_low = lba;
	ataio->cmd.lba_mid = lba >> 8;
	ataio->cmd.lba_high = lba >> 16;
	ataio->cmd.device = 0x40;
	ataio->cmd.lba_low_exp = lba >> 24;
	ataio->cmd.lba_mid_exp = lba >> 32;
	ataio->cmd.lba_high_exp = lba >> 40;
	ataio->cmd.features_exp = features >> 8;
	ataio->cmd.sector_count = sector_count;
	ataio->cmd.sector_count_exp = sector_count >> 8;
}

void
ata_ncq_cmd(struct ccb_ataio *ataio, uint8_t cmd,
    uint64_t lba, uint16_t sector_count)
{
	bzero(&ataio->cmd, sizeof(ataio->cmd));
	ataio->cmd.flags = CAM_ATAIO_48BIT | CAM_ATAIO_FPDMA;
	ataio->cmd.command = cmd;
	ataio->cmd.features = sector_count;
	ataio->cmd.lba_low = lba;
	ataio->cmd.lba_mid = lba >> 8;
	ataio->cmd.lba_high = lba >> 16;
	ataio->cmd.device = 0x40;
	ataio->cmd.lba_low_exp = lba >> 24;
	ataio->cmd.lba_mid_exp = lba >> 32;
	ataio->cmd.lba_high_exp = lba >> 40;
	ataio->cmd.features_exp = sector_count >> 8;
}

void
ata_reset_cmd(struct ccb_ataio *ataio)
{
	bzero(&ataio->cmd, sizeof(ataio->cmd));
	ataio->cmd.flags = CAM_ATAIO_CONTROL | CAM_ATAIO_NEEDRESULT;
	ataio->cmd.control = 0x04;
}

void
ata_pm_read_cmd(struct ccb_ataio *ataio, int reg, int port)
{
	bzero(&ataio->cmd, sizeof(ataio->cmd));
	ataio->cmd.flags = CAM_ATAIO_NEEDRESULT;
	ataio->cmd.command = ATA_READ_PM;
	ataio->cmd.features = reg;
	ataio->cmd.device = port & 0x0f;
}

void
ata_pm_write_cmd(struct ccb_ataio *ataio, int reg, int port, uint32_t val)
{
	bzero(&ataio->cmd, sizeof(ataio->cmd));
	ataio->cmd.flags = 0;
	ataio->cmd.command = ATA_WRITE_PM;
	ataio->cmd.features = reg;
	ataio->cmd.sector_count = val;
	ataio->cmd.lba_low = val >> 8;
	ataio->cmd.lba_mid = val >> 16;
	ataio->cmd.lba_high = val >> 24;
	ataio->cmd.device = port & 0x0f;
}

void
ata_bswap(int8_t *buf, int len)
{
	u_int16_t *ptr = (u_int16_t*)(buf + len);

	while (--ptr >= (u_int16_t*)buf)
		*ptr = be16toh(*ptr);
}

void
ata_btrim(int8_t *buf, int len)
{
	int8_t *ptr;

	for (ptr = buf; ptr < buf+len; ++ptr)
		if (!*ptr || *ptr == '_')
			*ptr = ' ';
	for (ptr = buf + len - 1; ptr >= buf && *ptr == ' '; --ptr)
		*ptr = 0;
}

void
ata_bpack(int8_t *src, int8_t *dst, int len)
{
	int i, j, blank;

	for (i = j = blank = 0 ; i < len; i++) {
		if (blank && src[i] == ' ') continue;
		if (blank && src[i] != ' ') {
			dst[j++] = src[i];
			blank = 0;
			continue;
		}
		if (src[i] == ' ') {
			blank = 1;
			if (i == 0)
			continue;
		}
		dst[j++] = src[i];
	}
	while (j < len)
		dst[j++] = 0x00;
}

int
ata_max_pmode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_64_70) {
	if (ap->apiomodes & 0x02)
	    return ATA_PIO4;
	if (ap->apiomodes & 0x01)
	    return ATA_PIO3;
    }
    if (ap->mwdmamodes & 0x04)
	return ATA_PIO4;
    if (ap->mwdmamodes & 0x02)
	return ATA_PIO3;
    if (ap->mwdmamodes & 0x01)
	return ATA_PIO2;
    if ((ap->retired_piomode & ATA_RETIRED_PIO_MASK) == 0x200)
	return ATA_PIO2;
    if ((ap->retired_piomode & ATA_RETIRED_PIO_MASK) == 0x100)
	return ATA_PIO1;
    if ((ap->retired_piomode & ATA_RETIRED_PIO_MASK) == 0x000)
	return ATA_PIO0;
    return ATA_PIO0;
}

int
ata_max_wmode(struct ata_params *ap)
{
    if (ap->mwdmamodes & 0x04)
	return ATA_WDMA2;
    if (ap->mwdmamodes & 0x02)
	return ATA_WDMA1;
    if (ap->mwdmamodes & 0x01)
	return ATA_WDMA0;
    return -1;
}

int
ata_max_umode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_88) {
	if (ap->udmamodes & 0x40)
	    return ATA_UDMA6;
	if (ap->udmamodes & 0x20)
	    return ATA_UDMA5;
	if (ap->udmamodes & 0x10)
	    return ATA_UDMA4;
	if (ap->udmamodes & 0x08)
	    return ATA_UDMA3;
	if (ap->udmamodes & 0x04)
	    return ATA_UDMA2;
	if (ap->udmamodes & 0x02)
	    return ATA_UDMA1;
	if (ap->udmamodes & 0x01)
	    return ATA_UDMA0;
    }
    return -1;
}

int
ata_max_mode(struct ata_params *ap, int mode, int maxmode)
{

    if (maxmode && mode > maxmode)
	mode = maxmode;

    if (mode >= ATA_UDMA0 && ata_max_umode(ap) > 0)
	return (min(mode, ata_max_umode(ap)));

    if (mode >= ATA_WDMA0 && ata_max_wmode(ap) > 0)
	return (min(mode, ata_max_wmode(ap)));

    if (mode > ata_max_pmode(ap))
	return (min(mode, ata_max_pmode(ap)));

    return (mode);
}

