/*
 * sound/maui.c
 * 
 * The low level driver for Turtle Beach Maui and Tropez.
 * 
 * Copyright by Hannu Savolainen 1995
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

#define USE_SEQ_MACROS
#define USE_SIMPLE_MACROS

#include <i386/isa/sound/sound_config.h>


#if defined(CONFIG_MAUI)

static int      maui_base = 0x330;

static volatile int irq_ok = 0;
static sound_os_info *maui_osp;

#define HOST_DATA_PORT	(maui_base + 2)
#define HOST_STAT_PORT	(maui_base + 3)
#define HOST_CTRL_PORT	(maui_base + 3)

#define STAT_TX_INTR	0x40
#define STAT_TX_AVAIL	0x20
#define STAT_TX_IENA	0x10
#define STAT_RX_INTR	0x04
#define STAT_RX_AVAIL	0x02
#define STAT_RX_IENA	0x01

static int      (*orig_load_patch) (int dev, int format, snd_rw_buf * addr,
		                 int offs, int count, int pmgr_flag) = NULL;

static int
maui_read(void)
{
    int             timeout;

    for (timeout = 0; timeout < 1000000; timeout++)
	if (inb(HOST_STAT_PORT) & STAT_RX_AVAIL)
	    return inb(HOST_DATA_PORT);

    printf("Maui: Receive timeout\n");

    return -1;
}

static int
maui_write(u_char data)
{
    int             timeout;

    for (timeout = 0; timeout < 10000000; timeout++) {
	if (inb(HOST_STAT_PORT) & STAT_TX_AVAIL) {
	    outb(HOST_DATA_PORT, data);
	    return 1;
	}
    }

    printf("Maui: Write timeout\n");

    return 0;
}

void
mauiintr(int irq)
{
    irq_ok = 1;
}


int
maui_load_patch(int dev, int format, snd_rw_buf * addr,
		int offs, int count, int pmgr_flag)
{

    struct sysex_info header;
    u_long   left, src_offs;
    int             hdr_size = (u_long) &header.data[0] - (u_long) &header;
    int             i;

    if (format == SYSEX_PATCH)	/* Handled by midi_synth.c */
	return orig_load_patch(dev, format, addr, offs, count, pmgr_flag);

    if (format != MAUI_PATCH) {
	printf("Maui: Unknown patch format\n");
    }
    if (count < hdr_size) {
	printf("Maui error: Patch header too short\n");
	return -(EINVAL);
    }
    count -= hdr_size;

    /*
     * Copy the header from user space but ignore the first bytes which
     * have been transferred already.
     */

    if (uiomove(&((char *) &header)[offs], hdr_size - offs, addr)) {
	printf("sb: Bad copyin()!\n");
    };

    if (count < header.len) {
	printf("Maui warning: Host command record too short (%d<%d)\n",
	       count, (int) header.len);
	header.len = count;
    }
    left = header.len;
    src_offs = 0;

    for (i = 0; i < left; i++) {
	u_char   data;
	uiomove((char *) &(data), 1, addr);
	if (i == 0 && !(data & 0x80))
	    return -(EINVAL);

	if (maui_write(data) == -1)
	    return -(EIO);
    }

    if ((i = maui_read()) != 0x80) {
	if (i != -1)
	    printf("Maui: Error status %02x\n", i);

	return -(EIO);
    }
    return 0;
}

int
probe_maui(struct address_info * hw_config)
{
    int             i;
    int             tmp1, tmp2;

    maui_base = hw_config->io_base;
    maui_osp = hw_config->osp;

    if (snd_set_irq_handler(hw_config->irq, mauiintr, maui_osp) < 0)
	return 0;

    if (!maui_write(0xCF)) {/* Report hardware version */
	/* snd_release_irq(hw_config->irq); */
	return 0;
    }
    if ((tmp1 = maui_read()) == -1 || (tmp2 = maui_read()) == -1) {
	/* snd_release_irq(hw_config->irq); */
	return 0;
    }
    printf("WaveFront hardware version %d.%d\n", tmp1, tmp2);

    if (!maui_write(0x9F))	/* Report firmware version */
	return 0;
    if ((tmp1 = maui_read()) == -1 || (tmp2 = maui_read()) == -1)
	return 0;
    printf("WaveFront firmware version %d.%d\n", tmp1, tmp2);

    if (!maui_write(0x85))	/* Report free DRAM */
	return 0;
    tmp1 = 0;
    for (i = 0; i < 4; i++) {
	tmp1 |= maui_read() << (7 * i);
    }
    printf("Available DRAM %dk\n", tmp1 / 1024);

    for (i = 0; i < 1000; i++)
	if (probe_mpu401(hw_config))
	    break;

    return probe_mpu401(hw_config);
}

void
attach_maui(struct address_info * hw_config)
{
    int             this_dev = num_midis;

    conf_printf("Maui", hw_config);

    hw_config->irq *= -1;
    attach_mpu401(hw_config);

    if (num_midis > this_dev) {	/* The MPU401 driver installed itself */
	struct synth_operations *synth;

	/*
	 * Intercept patch loading calls so that they canbe handled
	 * by the Maui driver.
	 */

	synth = midi_devs[this_dev]->converter;

	if (synth != NULL) {
	    orig_load_patch = synth->load_patch;
	    synth->load_patch = &maui_load_patch;
	} else
	    printf("Maui: Can't install patch loader\n");
    }
    return;
}

#endif
