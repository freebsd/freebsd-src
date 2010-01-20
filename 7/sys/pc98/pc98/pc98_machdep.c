/*-
 * Copyright (c) KATO Takenori, 1996, 1997.
 *
 * All rights reserved.  Unpublished rights reserved under the copyright
 * laws of Japan.
 *
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $FreeBSD$
 */

#include "opt_pc98.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <geom/geom_disk.h>
#include <machine/md_var.h>
#include <pc98/pc98/pc98_machdep.h>

static	int	ad_geom_method = AD_GEOM_ADJUST_COMPATIDE;

TUNABLE_INT("machdep.ad_geom_method", &ad_geom_method);
SYSCTL_INT(_machdep, OID_AUTO, ad_geom_method, CTLFLAG_RW, &ad_geom_method, 0,
    "IDE disk geometry conversion method");

/*
 * Initialize DMA controller
 */
void
pc98_init_dmac(void)
{
	outb(0x439, (inb(0x439) & 0xfb));	/* DMA Accsess Control over 1MB */
	outb(0x29, (0x0c | 0));				/* Bank Mode Reg. 16M mode */
	outb(0x29, (0x0c | 1));				/* Bank Mode Reg. 16M mode */
	outb(0x29, (0x0c | 2));				/* Bank Mode Reg. 16M mode */
	outb(0x29, (0x0c | 3));				/* Bank Mode Reg. 16M mode */
	outb(0x11, 0x50);
}

#ifdef EPSON_MEMWIN
/*
 * Disconnect phisical memory in 15-16MB region.
 *
 * EPSON PC-486GR, P, SR, SE, HX, HG and HA only.  Other system support
 * this feature with software DIP switch.
 */
static void
init_epson_memwin(void)
{
	/* Disable 15MB-16MB caching. */
	switch (epson_machine_id) {
	case EPSON_PC486_HX:
	case EPSON_PC486_HG:
	case EPSON_PC486_HA:
		/* Cache control start. */
		outb(0x43f, 0x42);
		outw(0xc40, 0x0033);

		/* Disable 0xF00000-0xFFFFFF. */
		outb(0xc48, 0x49);
		outb(0xc4c, 0x00);
		outb(0xc48, 0x48);
		outb(0xc4c, 0xf0);
		outb(0xc48, 0x4d);
		outb(0xc4c, 0x00);
		outb(0xc48, 0x4c);
		outb(0xc4c, 0xff);
		outb(0xc48, 0x4f);
		outb(0xc4c, 0x00);

		/* Cache control end. */
		outb(0x43f, 0x40);
		break;

	case EPSON_PC486_GR:
	case EPSON_PC486_P:
	case EPSON_PC486_GR_SUPER:
	case EPSON_PC486_GR_PLUS:
	case EPSON_PC486_SE:
	case EPSON_PC486_SR:
		/* Disable 0xF00000-0xFFFFFF. */
		outb(0x43f, 0x42);
		outb(0x467, 0xe0);
		outb(0x567, 0xd8);

		outb(0x43f, 0x40);
		outb(0x467, 0xe0);
		outb(0x567, 0xe0);
		break;
	}

	/* Disable 15MB-16MB RAM and enable memory window. */
	outb(0x43b, inb(0x43b) & 0xfd);	/* Clear bit1. */
}
#endif

/*
 * Get physical memory size
 */
unsigned int
pc98_getmemsize(unsigned int *base, unsigned int *ext)
{
	unsigned int under16, over16;

	/* available conventional memory size */
	*base = ((PC98_SYSTEM_PARAMETER(0x501) & 7) + 1) * 128;

	/* available protected memory size under 16MB */
	under16 = PC98_SYSTEM_PARAMETER(0x401) * 128 + 1024;
#ifdef EPSON_MEMWIN
	if (pc98_machine_type & M_EPSON_PC98) {
		if (under16 > (15 * 1024))
			/* chop under16 memory to 15MB */
			under16 = 15 * 1024;
		init_epson_memwin();
	}
#endif

	/* available protected memory size over 16MB / 1MB */
	over16  = PC98_SYSTEM_PARAMETER(0x594);
	over16 += PC98_SYSTEM_PARAMETER(0x595) * 256;

	if (over16 > 0)
		*ext = (16 + over16) * 1024 - 1024;
	else
		*ext = under16 - 1024;

	return (under16);
}

/*
 * Read a geometry information of SCSI HDD from BIOS work area.
 *
 * XXX - Before reading BIOS work area, we should check whether
 * host adapter support it.
 */
int
scsi_da_bios_params(struct ccb_calc_geometry *ccg)
{
	u_char *tmp;
	int	target;

	target = ccg->ccb_h.target_id;
	tmp = (u_char *)&PC98_SYSTEM_PARAMETER(0x460 + target*4);
	if ((PC98_SYSTEM_PARAMETER(0x482) & ((1 << target)&0xff)) != 0) {
		ccg->secs_per_track = *tmp;
		ccg->cylinders = ((*(tmp+3)<<8)|*(tmp+2))&0xfff;
#if 0
		switch (*(tmp + 3) & 0x30) {
		case 0x00:
			disk_parms->secsiz = 256;
			printf("Warning!: not supported.\n");
			break;
		case 0x10:
			disk_parms->secsiz = 512;
			break;
		case 0x20:
			disk_parms->secsiz = 1024;
			break;
		default:
			disk_parms->secsiz = 512;
			printf("Warning!: not supported. But force to 512\n");
			break;
		}
#endif
		if (*(tmp+3) & 0x40) {
			ccg->cylinders += (*(tmp+1)&0xf0)<<8;
			ccg->heads = *(tmp+1)&0x0f;
		} else {
			ccg->heads = *(tmp+1);
		}
		return (1);
	}

	return (0);
}

/*
 * Adjust the geometry of the IDE HDD.
 */

/* IDE BIOS compatible mode. */
static	void
pc98_ad_geom_adjust_idebios(struct disk *disk)
{

	if (disk->d_mediasize < MEDIASIZE_4_3G) {
		disk->d_fwsectors = 17;
		disk->d_fwheads = 8;
	} else if (disk->d_mediasize < MEDIASIZE_29_5G) {
		disk->d_fwsectors = 63;
		if (disk->d_fwheads != 15)	/* Allow 15H63S. */
			disk->d_fwheads = 16;
	} else if (disk->d_mediasize < MEDIASIZE_31_5G) {
		disk->d_fwsectors = 63;
		disk->d_fwheads = 16;
	} else if (disk->d_mediasize < MEDIASIZE_127G) {
		disk->d_fwsectors = 255;
		disk->d_fwheads = 16;
	} else {
		/* XXX */
		disk->d_fwsectors = 255;
		disk->d_fwheads = 255;
	}
}

/* SCSI BIOS compatible mode. */
static	void
pc98_ad_geom_adjust_scsibios(struct disk *disk)
{

	if (disk->d_mediasize < MEDIASIZE_8G) {
		disk->d_fwsectors = 32;
		disk->d_fwheads = 8;
	} else if (disk->d_mediasize < MEDIASIZE_32G) {
		disk->d_fwsectors = 128;
		disk->d_fwheads = 8;
	} else if (disk->d_mediasize < MEDIASIZE_60G) {
		/* Compatible with IFC-USP 1.2. */
		disk->d_fwsectors = 128;
		disk->d_fwheads = 15;
	} else if (disk->d_mediasize < MEDIASIZE_120G) {
		disk->d_fwsectors = 255;
		disk->d_fwheads = 15;
	} else {
		/* XXX */
		disk->d_fwsectors = 255;
		disk->d_fwheads = 255;
	}
}

/* Compatible with the revision 1.28. */
static	void
pc98_ad_geom_adjust_cyl16bit(struct disk *disk)
{
	off_t totsec = disk->d_mediasize / disk->d_sectorsize;
	off_t cyl = totsec / disk->d_fwsectors / disk->d_fwheads;
    
	/*
	 * It is impossible to have more than 65535 cylendars, so if
	 * we have more then try to adjust.  This is lame, but it is
	 * only POC.
	 */
	if (cyl > 65355) {
		if (totsec < 17*8*65535) {
			disk->d_fwsectors = 17;
			disk->d_fwheads = 8;
		} else if (totsec < 63*16*65535) {
			disk->d_fwsectors = 63;
			disk->d_fwheads = 16;
		} else if (totsec < 255*16*65535) {
			disk->d_fwsectors = 255;
			disk->d_fwheads = 16;
		} else {
			disk->d_fwsectors = 255;
			disk->d_fwheads = 255;
		}
	}
}

void
pc98_ad_firmware_geom_adjust(device_t dev, struct disk *disk)
{
	u_int	oldsectors, oldheads;

	oldsectors = disk->d_fwsectors;
	oldheads = disk->d_fwheads;

	switch (ad_geom_method) {
	case AD_GEOM_ADJUST_COMPATIDE:
		pc98_ad_geom_adjust_idebios(disk);
		break;
	case AD_GEOM_ADJUST_COMPATSCSI:
		pc98_ad_geom_adjust_scsibios(disk);
		break;
	case AD_GEOM_ADJUST_COMPATCYL16:
		pc98_ad_geom_adjust_cyl16bit(disk);
		break;
	default:
		/* Do nothing. */
		break;
	}

	if (bootverbose &&
	    (oldsectors != disk->d_fwsectors || oldheads != disk->d_fwheads))
		device_printf(dev,
		    "geometry adjusted from [%dH/%dS] to [%dH/%dS]\n",
		    oldheads, oldsectors,
		    disk->d_fwheads, disk->d_fwsectors);

}
