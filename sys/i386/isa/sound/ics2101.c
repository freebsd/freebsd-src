/*
 * sound/ics2101.c
 * 
 * Driver for the ICS2101 mixer of GUS v3.7.
 * 
 * Copyright by Hannu Savolainen 1994
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

#if defined(CONFIG_GUS)

#include <i386/isa/sound/ultrasound.h>
#include <i386/isa/sound/gus_hw.h>

#define MIX_DEVS	(SOUND_MASK_MIC|SOUND_MASK_LINE| \
			 SOUND_MASK_SYNTH| \
  			 SOUND_MASK_CD | SOUND_MASK_VOLUME)

extern sound_os_info *gus_osp;
extern int      gus_base;
static int      volumes[ICS_MIXDEVS];
static int      left_fix[ICS_MIXDEVS] = {1, 1, 1, 2, 1, 2};
static int      right_fix[ICS_MIXDEVS] = {2, 2, 2, 1, 2, 1};

static int
scale_vol(int vol)
{
    /*
     * Experimental volume scaling by Risto Kankkunen. This should give
     * smoother volume response than just a plain multiplication.
     */
    int             e;

    RANGE(vol, 0, 100);
    vol = (31 * vol + 50) / 100;
    e = 0;
    if (vol) {
	while (vol < 16) {
	    vol <<= 1;
	    e--;
	}
	vol -= 16;
	e += 7;
    }
    return ((e << 4) + vol);
}

static void
write_mix(int dev, int chn, int vol)
{
    int            *selector;
    unsigned long   flags;
    int             ctrl_addr = dev << 3;
    int             attn_addr = dev << 3;

    vol = scale_vol(vol);

    if (chn == CHN_LEFT) {
	selector = left_fix;
	ctrl_addr |= 0x00;
	attn_addr |= 0x02;
    } else {
	selector = right_fix;
	ctrl_addr |= 0x01;
	attn_addr |= 0x03;
    }

    flags = splhigh();
    outb(u_MixSelect, ctrl_addr);
    outb(u_MixData, selector[dev]);
    outb(u_MixSelect, attn_addr);
    outb(u_MixData, (unsigned char) vol);
    splx(flags);
}

static int
set_volumes(int dev, int vol)
{
    int             left = vol & 0x00ff;
    int             right = (vol >> 8) & 0x00ff;

    RANGE (left, 0, 100);
    RANGE (right, 0, 100);

    write_mix(dev, CHN_LEFT, left);
    write_mix(dev, CHN_RIGHT, right);

    vol = left + (right << 8);
    volumes[dev] = vol;
    return vol;
}

static int
ics2101_mixer_ioctl(int dev, unsigned int cmd, ioctl_arg arg)
{
    if (((cmd >> 8) & 0xff) == 'M') {
	if (cmd & IOC_IN)
	    switch (cmd & 0xff) {
	    case SOUND_MIXER_RECSRC:
		return gus_default_mixer_ioctl(dev, cmd, arg);
		break;

	    case SOUND_MIXER_MIC:
		return *(int *) arg = set_volumes(DEV_MIC, (*(int *) arg));
		break;

	    case SOUND_MIXER_CD:
		return *(int *) arg = set_volumes(DEV_CD, (*(int *) arg));
		break;

	    case SOUND_MIXER_LINE:
		return *(int *) arg = set_volumes(DEV_LINE, (*(int *) arg));
		break;

	    case SOUND_MIXER_SYNTH:
		return *(int *) arg = set_volumes(DEV_GF1, (*(int *) arg));
		break;

	    case SOUND_MIXER_VOLUME:
		return *(int *) arg = set_volumes(DEV_VOL, (*(int *) arg));
		break;

	    default:
		return -(EINVAL);
	    }
	else
	    switch (cmd & 0xff) {	/* Return parameters */

	    case SOUND_MIXER_RECSRC:
		return gus_default_mixer_ioctl(dev, cmd, arg);
		break;

	    case SOUND_MIXER_DEVMASK:
		return *(int *) arg = MIX_DEVS;
		break;

	    case SOUND_MIXER_STEREODEVS:
		return *(int *) arg = SOUND_MASK_LINE | SOUND_MASK_CD |
			SOUND_MASK_SYNTH | SOUND_MASK_VOLUME | SOUND_MASK_MIC;
		break;

	    case SOUND_MIXER_RECMASK:
		return *(int *) arg = SOUND_MASK_MIC | SOUND_MASK_LINE;
		break;

	    case SOUND_MIXER_CAPS:
		return *(int *) arg = 0;
		break;

	    case SOUND_MIXER_MIC:
		return *(int *) arg = volumes[DEV_MIC];
		break;

	    case SOUND_MIXER_LINE:
		return *(int *) arg = volumes[DEV_LINE];
		break;

	    case SOUND_MIXER_CD:
		return *(int *) arg = volumes[DEV_CD];
		break;

	    case SOUND_MIXER_VOLUME:
		return *(int *) arg = volumes[DEV_VOL];
		break;

	    case SOUND_MIXER_SYNTH:
		return *(int *) arg = volumes[DEV_GF1];
		break;

	    default:
		return -(EINVAL);
	    }
    }
    return -(EINVAL);
}

static struct mixer_operations ics2101_mixer_operations =
{
	"ICS2101 Multimedia Mixer",
	ics2101_mixer_ioctl
};

void
ics2101_mixer_init()
{
    int             i;

    if (num_mixers < MAX_MIXER_DEV) {
	mixer_devs[num_mixers++] = &ics2101_mixer_operations;

	/*
	 * Some GUS v3.7 cards had some channels flipped. Disable the
	 * flipping feature if the model id is other than 5.
	 */

	if (inb(u_MixSelect) != 5) {
	    for (i = 0; i < ICS_MIXDEVS; i++)
		left_fix[i] = 1;
	    for (i = 0; i < ICS_MIXDEVS; i++)
		right_fix[i] = 2;
	}
	set_volumes(DEV_GF1, 0x5a5a);
	set_volumes(DEV_CD, 0x5a5a);
	set_volumes(DEV_MIC, 0x0000);
	set_volumes(DEV_LINE, 0x5a5a);
	set_volumes(DEV_VOL, 0x5a5a);
	set_volumes(DEV_UNUSED, 0x0000);
    }
}

#endif
