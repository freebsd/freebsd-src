/*
 * sound/pss.c
 * 
 * The low level driver for the Personal Sound System (ECHO ESC614).
 * 
 * Copyright by Hannu Savolainen 1993
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 */

#include <i386/isa/sound/sound_config.h>

#if defined(CONFIG_PSS) && defined(CONFIG_AUDIO)

/*
 * PSS registers.
 */
#define REG(x)	(devc->base+x)
#define	PSS_DATA	0
#define	PSS_STATUS	2
#define PSS_CONTROL	2
#define	PSS_ID		4
#define	PSS_IRQACK	4
#define	PSS_PIO		0x1a

/*
 * Config registers
 */
#define CONF_PSS	0x10
#define CONF_WSS	0x12
#define CONF_SB		0x13
#define CONF_CDROM	0x16
#define CONF_MIDI	0x18

/*
 * Status bits.
 */
#define PSS_FLAG3     0x0800
#define PSS_FLAG2     0x0400
#define PSS_FLAG1     0x1000
#define PSS_FLAG0     0x0800
#define PSS_WRITE_EMPTY  0x8000
#define PSS_READ_FULL    0x4000

#include "coproc.h"

#ifdef PSS_HAVE_LD
#include "synth-ld.h"
#else
static int      pss_synthLen = 0;
static u_char pss_synth[1] =
{0};

#endif

typedef struct pss_config {
	int             base;
	int             irq;
	int             dma;
	sound_os_info  *osp;
} pss_config;

static pss_config pss_data;
static pss_config *devc = &pss_data;

static int      pss_initialized = 0;
static int      nonstandard_microcode = 0;

int
probe_pss(struct address_info * hw_config)
{
	u_short  id;
	int             irq, dma;

	devc->base = hw_config->io_base;
	irq = devc->irq = hw_config->irq;
	dma = devc->dma = hw_config->dma;
	devc->osp = hw_config->osp;

	/* these are the possible addresses */
	if (devc->base != 0x220 && devc->base != 0x240 &&
	    devc->base != 0x230 && devc->base != 0x250)
			return 0;

	/* these are the possible irqs */
	if (irq != 3 && irq != 5 && irq != 7 && irq != 9 &&
	    irq != 10 && irq != 11 && irq != 12)
		return 0;

	/* and these are the possible dmas */
	if (dma != 5 && dma != 6 && dma != 7)
		return 0;

	id = inb(REG(PSS_ID));

	/* XXX the following test cannot possibly succeed! - lr970714 */
	if ((id >> 8) != 'E') {
		/*
		 * printf ("No PSS signature detected at 0x%x (0x%x)\n",
		 * devc->base, id);
		 */
		return 0;
	}
	return 1;
}

static int
set_irq(pss_config * devc, int dev, int irq)
{
	static u_short irq_bits[16] =
	{
		0x0000, 0x0000, 0x0000, 0x0008,
		0x0000, 0x0010, 0x0000, 0x0018,
		0x0000, 0x0020, 0x0028, 0x0030,
		0x0038, 0x0000, 0x0000, 0x0000
	};

	u_short  tmp, bits;

	if (irq < 0 || irq > 15)
		return 0;

	tmp = inb(REG(dev)) & ~0x38;	/* Load confreg, mask IRQ bits out */

	if ((bits = irq_bits[irq]) == 0 && irq != 0) {
		printf("PSS: Invalid IRQ %d\n", irq);
		return 0;
	}
	outw(REG(dev), tmp | bits);
	return 1;
}

static int
set_io_base(pss_config * devc, int dev, int base)
{
	u_short  tmp = inb(REG(dev)) & 0x003f;
	u_short  bits = (base & 0x0ffc) << 4;

	outw(REG(dev), bits | tmp);

	return 1;
}

static int
set_dma(pss_config * devc, int dev, int dma)
{
	static u_short dma_bits[8] =
	{
		0x0001, 0x0002, 0x0000, 0x0003,
		0x0000, 0x0005, 0x0006, 0x0007
	};

	u_short  tmp, bits;

	if (dma < 0 || dma > 7)
		return 0;

	tmp = inb(REG(dev)) & ~0x07;	/* Load confreg, mask DMA bits out */

	if ((bits = dma_bits[dma]) == 0 && dma != 4) {
		printf("PSS: Invalid DMA %d\n", dma);
		return 0;
	}
	outw(REG(dev), tmp | bits);
	return 1;
}

static int
pss_reset_dsp(pss_config * devc)
{
	u_long   i, limit = get_time() + 10;

	outw(REG(PSS_CONTROL), 0x2000);

	for (i = 0; i < 32768 && get_time() < limit; i++)
		inb(REG(PSS_CONTROL));

	outw(REG(PSS_CONTROL), 0x0000);

	return 1;
}

static int
pss_put_dspword(pss_config * devc, u_short word)
{
	int             i, val;

	for (i = 0; i < 327680; i++) {
		val = inb(REG(PSS_STATUS));
		if (val & PSS_WRITE_EMPTY) {
			outw(REG(PSS_DATA), word);
			return 1;
		}
	}
	return 0;
}

static int
pss_get_dspword(pss_config * devc, u_short *word)
{
	int             i, val;

	for (i = 0; i < 327680; i++) {
		val = inb(REG(PSS_STATUS));
		if (val & PSS_READ_FULL) {
			*word = inb(REG(PSS_DATA));
			return 1;
		}
	}

	return 0;
}

static int
pss_download_boot(pss_config * devc, u_char *block, int size, int flags)
{
	int             i, limit, val, count;

	if (flags & CPF_FIRST) {
		/* _____ Warn DSP software that a boot is coming */
		outw(REG(PSS_DATA), 0x00fe);

		limit = get_time() + 10;

		for (i = 0; i < 32768 && get_time() < limit; i++)
			if (inb(REG(PSS_DATA)) == 0x5500)
				break;

		outw(REG(PSS_DATA), *block++);

		pss_reset_dsp(devc);
	}
	count = 1;
	while (1) {
		int             j;

		for (j = 0; j < 327670; j++) {
			/* _____ Wait for BG to appear */
			if (inb(REG(PSS_STATUS)) & PSS_FLAG3)
				break;
		}

		if (j == 327670) {
			/* It's ok we timed out when the file was empty */
			if (count >= size && flags & CPF_LAST)
				break;
			else {
				printf("\nPSS: DownLoad timeout problems, byte %d=%d\n",
				       count, size);
				return 0;
			}
		}
		/* _____ Send the next byte */
		outw(REG(PSS_DATA), *block++);
		count++;
	}

	if (flags & CPF_LAST) {
		/* _____ Why */
		outw(REG(PSS_DATA), 0);

		limit = get_time() + 10;
		for (i = 0; i < 32768 && get_time() < limit; i++)
			val = inb(REG(PSS_STATUS));

		limit = get_time() + 10;
		for (i = 0; i < 32768 && get_time() < limit; i++) {
			val = inb(REG(PSS_STATUS));
			if (val & 0x4000)
				break;
		}

		/* now read the version */
		for (i = 0; i < 32000; i++) {
			val = inb(REG(PSS_STATUS));
			if (val & PSS_READ_FULL)
				break;
		}
		if (i == 32000)
			return 0;

		val = inb(REG(PSS_DATA));
		/*
		 * printf("<PSS: microcode version %d.%d loaded>", val/16,
		 * val % 16);
		 */
	}
	return 1;
}

void
attach_pss(struct address_info * hw_config)
{
	u_short  id;
	char            tmp[100];

	devc->base = hw_config->io_base;
	devc->irq = hw_config->irq;
	devc->dma = hw_config->dma;
	devc->osp = hw_config->osp;

	if (!probe_pss(hw_config))
		return;

	id = inb(REG(PSS_ID)) & 0x00ff;

	/*
	 * Disable all emulations. Will be enabled later (if required).
	 */
	outw(REG(CONF_PSS), 0x0000);
	outw(REG(CONF_WSS), 0x0000);
	outw(REG(CONF_SB), 0x0000);
	outw(REG(CONF_MIDI), 0x0000);
	outw(REG(CONF_CDROM), 0x0000);

#if YOU_REALLY_WANT_TO_ALLOCATE_THESE_RESOURCES
	if (0) {
		printf("pss.c: Can't allocate DMA channel\n");
		return;
	}
	if (!set_irq(devc, CONF_PSS, devc->irq)) {
		printf("PSS: IRQ error\n");
		return;
	}
	if (!set_dma(devc, CONF_PSS, devc->dma)) {
		printf("PSS: DRQ error\n");
		return;
	}
#endif

	pss_initialized = 1;
	sprintf(tmp, "ECHO-PSS  Rev. %d", id);
	conf_printf(tmp, hw_config);

	return;
}

int
probe_pss_mpu(struct address_info * hw_config)
{
	int             timeout;

	if (!pss_initialized)
		return 0;

	if (0) {
		printf("PSS: MPU I/O port conflict\n");
		return 0;
	}
	if (!set_io_base(devc, CONF_MIDI, hw_config->io_base)) {
		printf("PSS: MIDI base error.\n");
		return 0;
	}
	if (!set_irq(devc, CONF_MIDI, hw_config->irq)) {
		printf("PSS: MIDI IRQ error.\n");
		return 0;
	}
	if (!pss_synthLen) {
		printf("PSS: Can't enable MPU. MIDI synth microcode not available.\n");
		return 0;
	}
	if (!pss_download_boot(devc, pss_synth, pss_synthLen, CPF_FIRST | CPF_LAST)) {
		printf("PSS: Unable to load MIDI synth microcode to DSP.\n");
		return 0;
	}
	/*
	 * Finally wait until the DSP algorithm has initialized itself and
	 * deactivates receive interrupt.
	 */

	for (timeout = 900000; timeout > 0; timeout--) {
		if ((inb(hw_config->io_base + 1) & 0x80) == 0)	/* Input data avail */
			inb(hw_config->io_base);	/* Discard it */
		else
			break;	/* No more input */
	}

#if (defined(CONFIG_MPU401) || defined(CONFIG_MPU_EMU)) && defined(CONFIG_MIDI)
	return probe_mpu401(hw_config);
#else
	return 0
#endif
}

static int
pss_coproc_open(void *dev_info, int sub_device)
{
	switch (sub_device) {
		case COPR_MIDI:

		if (pss_synthLen == 0) {
			printf("PSS: MIDI synth microcode not available.\n");
			return -(EIO);
		}
		if (nonstandard_microcode)
			if (!pss_download_boot(devc, pss_synth, pss_synthLen, CPF_FIRST | CPF_LAST)) {
				printf("PSS: Unable to load MIDI synth microcode to DSP.\n");
				return -(EIO);
			}
		nonstandard_microcode = 0;
		break;

	default:;
	}
	return 0;
}

static void
pss_coproc_close(void *dev_info, int sub_device)
{
	return;
}

static void
pss_coproc_reset(void *dev_info)
{
	if (pss_synthLen)
		if (!pss_download_boot(devc, pss_synth, pss_synthLen, CPF_FIRST | CPF_LAST)) {
			printf("PSS: Unable to load MIDI synth microcode to DSP.\n");
		}
	nonstandard_microcode = 0;
}

static int
download_boot_block(void *dev_info, copr_buffer * buf)
{
	if (buf->len <= 0 || buf->len > sizeof(buf->data))
		return -(EINVAL);

	if (!pss_download_boot(devc, buf->data, buf->len, buf->flags)) {
		printf("PSS: Unable to load microcode block to DSP.\n");
		return -(EIO);
	}
	nonstandard_microcode = 1;	/* The MIDI microcode has been
					 * overwritten */

	return 0;
}

static int
pss_coproc_ioctl(void *dev_info, u_int cmd, ioctl_arg arg, int local)
{
	/* printf("PSS coproc ioctl %x %x %d\n", cmd, arg, local); */

	switch (cmd) {
		case SNDCTL_COPR_RESET:
		pss_coproc_reset(dev_info);
		return 0;
		break;

	case SNDCTL_COPR_LOAD:
		{
			copr_buffer    *buf;
			int             err;

			buf = (copr_buffer *) malloc(sizeof(copr_buffer), M_TEMP, M_WAITOK);
			if (buf == NULL)
				return -(ENOSPC);

			bcopy(&(((char *) arg)[0]), (char *) buf, sizeof(*buf));
			err = download_boot_block(dev_info, buf);
			free(buf, M_TEMP);
			return err;
		}
		break;

	case SNDCTL_COPR_RDATA:
		{
			copr_debug_buf  buf;
			u_long   flags;
			u_short  tmp;

			bcopy(&(((char *) arg)[0]), (char *) &buf, sizeof(buf));

			flags = splhigh();
			if (!pss_put_dspword(devc, 0x00d0)) {
				splx(flags);
				return -(EIO);
			}
			if (!pss_put_dspword(devc, (u_short) (buf.parm1 & 0xffff))) {
				splx(flags);
				return -(EIO);
			}
			if (!pss_get_dspword(devc, &tmp)) {
				splx(flags);
				return -(EIO);
			}
			buf.parm1 = tmp;
			splx(flags);

			bcopy(&buf, &(((char *) arg)[0]), sizeof(buf));
			return 0;
		}
		break;

	case SNDCTL_COPR_WDATA:
		{
			copr_debug_buf  buf;
			u_long   flags;
			u_short  tmp;

			bcopy(&(((char *) arg)[0]), (char *) &buf, sizeof(buf));

			flags = splhigh();
			if (!pss_put_dspword(devc, 0x00d1)) {
				splx(flags);
				return -(EIO);
			}
			if (!pss_put_dspword(devc, (u_short) (buf.parm1 & 0xffff))) {
				splx(flags);
				return -(EIO);
			}
			tmp = (u_int) buf.parm2 & 0xffff;
			if (!pss_put_dspword(devc, tmp)) {
				splx(flags);
				return -(EIO);
			}
			splx(flags);
			return 0;
		}
		break;

	case SNDCTL_COPR_WCODE:
		{
			copr_debug_buf  buf;
			u_long   flags;
			u_short  tmp;

			bcopy(&(((char *) arg)[0]), (char *) &buf, sizeof(buf));

			flags = splhigh();
			if (!pss_put_dspword(devc, 0x00d3)) {
				splx(flags);
				return -(EIO);
			}
			if (!pss_put_dspword(devc, (u_short) (buf.parm1 & 0xffff))) {
				splx(flags);
				return -(EIO);
			}
			tmp = ((u_int) buf.parm2 >> 8) & 0xffff;
			if (!pss_put_dspword(devc, tmp)) {
				splx(flags);
				return -(EIO);
			}
			tmp = (u_int) buf.parm2 & 0x00ff;
			if (!pss_put_dspword(devc, tmp)) {
				splx(flags);
				return -(EIO);
			}
			splx(flags);
			return 0;
		}
		break;

	case SNDCTL_COPR_RCODE:
		{
			copr_debug_buf  buf;
			u_long   flags;
			u_short  tmp;

			bcopy(&(((char *) arg)[0]), (char *) &buf, sizeof(buf));

			flags = splhigh();
			if (!pss_put_dspword(devc, 0x00d2)) {
				splx(flags);
				return -(EIO);
			}
			if (!pss_put_dspword(devc, (u_short) (buf.parm1 & 0xffff))) {
				splx(flags);
				return -(EIO);
			}
			if (!pss_get_dspword(devc, &tmp)) {	/* Read msb */
				splx(flags);
				return -(EIO);
			}
			buf.parm1 = tmp << 8;

			if (!pss_get_dspword(devc, &tmp)) {	/* Read lsb */
				splx(flags);
				return -(EIO);
			}
			buf.parm1 |= tmp & 0x00ff;

			splx(flags);

			bcopy(&buf, &(((char *) arg)[0]), sizeof(buf));
			return 0;
		}
		break;

	default:
		return -(EINVAL);
	}

	return -(EINVAL);
}

static coproc_operations pss_coproc_operations =
{
	"ADSP-2115",
	pss_coproc_open,
	pss_coproc_close,
	pss_coproc_ioctl,
	pss_coproc_reset,
	&pss_data
};

void
attach_pss_mpu(struct address_info * hw_config)
{
	int             prev_devs;

#if (defined(CONFIG_MPU401) || defined(CONFIG_MPU_EMU)) && defined(CONFIG_MIDI)
	prev_devs = num_midis;
	attach_mpu401(hw_config);

	if (num_midis == (prev_devs + 1))	/* The MPU driver installed
						 * itself */
		midi_devs[prev_devs]->coproc = &pss_coproc_operations;
#endif
}

int
probe_pss_mss(struct address_info * hw_config)
{
	int             timeout;

	if (!pss_initialized)
		return 0;

	if (0) {
		printf("PSS: WSS I/O port conflict\n");
		return 0;
	}
	if (!set_io_base(devc, CONF_WSS, hw_config->io_base)) {
		printf("PSS: WSS base error.\n");
		return 0;
	}
	if (!set_irq(devc, CONF_WSS, hw_config->irq)) {
		printf("PSS: WSS IRQ error.\n");
		return 0;
	}
	if (!set_dma(devc, CONF_WSS, hw_config->dma)) {
		printf("PSS: WSS DRQ error\n");
		return 0;
	}
	/*
	 * For some reason the card returns 0xff in the WSS status register
	 * immediately after boot. Propably MIDI+SB emulation algorithm
	 * downloaded to the ADSP2115 spends some time initializing the card.
	 * Let's try to wait until it finishes this task.
	 */
	for (timeout = 0;
	   timeout < 100000 && (inb(hw_config->io_base + 3) & 0x3f) != 0x04;
	     timeout++);

	return probe_mss(hw_config);
}

void
attach_pss_mss(struct address_info * hw_config)
{
	int             prev_devs;
	long            ret;

	prev_devs = num_audiodevs;
	attach_mss(hw_config);

	/* Check if The MSS driver installed itself */
	if (num_audiodevs == (prev_devs + 1))
		audio_devs[prev_devs]->coproc = &pss_coproc_operations;
}

#endif
