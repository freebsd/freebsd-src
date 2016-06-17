/*
 * linux/drivers/pcmcia/sa1100_badge4.c
 *
 * BadgePAD 4 PCMCIA specific routines
 *
 *   Christopher Hoover <ch@hpl.hp.com>
 *
 * Copyright (C) 2002 Hewlett-Packard Company
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/arch/badge4.h>
#include <asm/hardware/sa1111.h>

#include "sa1100_generic.h"
#include "sa1111_generic.h"

/*
 * BadgePAD 4 Details
 *
 * PCM Vcc:
 *
 *  PCM Vcc on BadgePAD 4 can be jumpered for 3.3V (short pins 1 and 3
 *  on JP6) or 5V (short pins 3 and 5 on JP6).  N.B., 5V supply rail
 *  is enabled by the SA-1110's BADGE4_GPIO_PCMEN5V (GPIO 24).
 *
 * PCM Vpp:
 *
 *  PCM Vpp on BadgePAD 4 can be jumpered for 12V (short pins 2 and 4
 *  on JP6) or tied to PCM Vcc (short pins 4 and 6 on JP6).  N.B., 12V
 *  operation requires that the power supply actually supply 12V.
 *
 * CF Vcc:
 *
 *  CF Vcc on BadgePAD 4 can be jumpered either for 3.3V (short pins 1
 *  and 2 on JP10) or 5V (short pins 2 and 3 on JP10).  The note above
 *  about the 5V supply rail applies.
 *
 * There's no way programmatically to determine how a given board is
 * jumpered.  This code assumes a default jumpering: 5V PCM Vcc (pins
 * 3 and 5 shorted) and PCM Vpp = PCM Vcc (pins 4 and 6 shorted) and
 * no jumpering for CF Vcc.  If this isn't correct, Override these
 * defaults with a pcmv setup argument: pcmv=<pcm vcc>,<pcm vpp>,<cf
 * vcc>.  E.g. pcmv=33,120,50 indicates 3.3V PCM Vcc, 12.0V PCM Vpp,
 * and 5.0V CF Vcc.
 *
 */

static int badge4_pcmvcc = 50;
static int badge4_pcmvpp = 50;
static int badge4_cfvcc = 0;

static int badge4_pcmcia_init(struct pcmcia_init *init)
{
	printk(KERN_INFO __FUNCTION__
	       ": badge4_pcmvcc=%d, badge4_pcmvpp=%d, badge4_cfvcc=%d\n",
	       badge4_pcmvcc, badge4_pcmvpp, badge4_cfvcc);

	return sa1111_pcmcia_init(init);
}

static int badge4_pcmcia_shutdown(void)
{
	int rc = sa1111_pcmcia_shutdown();

	/* be sure to disable 5V use */
	badge4_set_5V(BADGE4_5V_PCMCIA_SOCK0, 0);
	badge4_set_5V(BADGE4_5V_PCMCIA_SOCK1, 0);

	return rc;
}

static void complain_about_jumpering(const char *whom,
				     const char *supply,
				     int given, int wanted)
{
	printk(KERN_ERR
	 "%s: %s %d.%dV wanted but board is jumpered for %s %d.%dV operation"
	 "; re-jumper the board and/or use pcmv=xx,xx,xx\n",
	       whom, supply,
	       wanted / 10, wanted % 10,
	       supply,
	       given / 10, given % 10);
}

static unsigned badge4_need_5V_bitmap = 0;

static int
badge4_pcmcia_configure_socket(const struct pcmcia_configure *conf)
{
	int ret;

	switch (conf->sock) {
	case 0:
		if ((conf->vcc != 0) &&
		    (conf->vcc != badge4_pcmvcc)) {
			complain_about_jumpering(__FUNCTION__, "pcmvcc",
						 badge4_pcmvcc, conf->vcc);
			return -1;
		}
		if ((conf->vpp != 0) &&
		    (conf->vpp != badge4_pcmvpp)) {
			complain_about_jumpering(__FUNCTION__, "pcmvpp",
						 badge4_pcmvpp, conf->vpp);
			return -1;
		}
		break;

	case 1:
		if ((conf->vcc != 0) &&
		    (conf->vcc != badge4_cfvcc)) {
			complain_about_jumpering(__FUNCTION__, "cfvcc",
						 badge4_cfvcc, conf->vcc);
			return -1;
		}
		break;

	default:
		return -1;
	}

	ret = sa1111_pcmcia_configure_socket(conf);
	if (ret == 0) {
		unsigned long flags;
		int need5V;

		local_irq_save(flags);

		need5V = ((conf->vcc == 50) || (conf->vpp == 50));

		badge4_set_5V(BADGE4_5V_PCMCIA_SOCK(conf->sock), need5V);

		local_irq_restore(flags);
	}

	return 0;
}

struct pcmcia_low_level badge4_pcmcia_ops = {
	init:			badge4_pcmcia_init,
	shutdown:		badge4_pcmcia_shutdown,
	socket_state:		sa1111_pcmcia_socket_state,
	get_irq_info:		sa1111_pcmcia_get_irq_info,
	configure_socket:	badge4_pcmcia_configure_socket,

	socket_init:		sa1111_pcmcia_socket_init,
	socket_suspend:		sa1111_pcmcia_socket_suspend,
};


static int __init pcmv_setup(char *s)
{
	int v[4];

	s = get_options(s, ARRAY_SIZE(v), v);

	if (v[0] >= 1) badge4_pcmvcc = v[1];
	if (v[0] >= 2) badge4_pcmvpp = v[2];
	if (v[0] >= 3) badge4_cfvcc = v[3];

	return 1;
}

__setup("pcmv=", pcmv_setup);
