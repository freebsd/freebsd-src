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
	ataio->cmd.flags = CAM_ATAIO_48BIT | CAM_ATAIO_NEEDRESULT;
	ataio->cmd.command = ATA_READ_PM;
	ataio->cmd.features = reg;
	ataio->cmd.features_exp = reg >> 8;
	ataio->cmd.device = port & 0x0f;
}

void
ata_pm_write_cmd(struct ccb_ataio *ataio, int reg, int port, uint64_t val)
{
	bzero(&ataio->cmd, sizeof(ataio->cmd));
	ataio->cmd.flags = CAM_ATAIO_48BIT | CAM_ATAIO_NEEDRESULT;
	ataio->cmd.command = ATA_WRITE_PM;
	ataio->cmd.features = reg;
	ataio->cmd.lba_low = val >> 8;
	ataio->cmd.lba_mid = val >> 16;
	ataio->cmd.lba_high = val >> 24;
	ataio->cmd.device = port & 0x0f;
	ataio->cmd.lba_low_exp = val >> 40;
	ataio->cmd.lba_mid_exp = val >> 48;
	ataio->cmd.lba_high_exp = val >> 56;
	ataio->cmd.features_exp = reg >> 8;
	ataio->cmd.sector_count = val;
	ataio->cmd.sector_count_exp = val >> 32;
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

