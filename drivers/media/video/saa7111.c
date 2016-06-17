/* 
    saa7111 - Philips SAA7111A video decoder driver version 0.0.3

    Copyright (C) 1998 Dave Perks <dperks@ibm.net>

    Slight changes for video timing and attachment output by
    Wolfgang Scherr <scherr@net4you.net>
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include <linux/videodev.h>
#include <linux/version.h>
#include <linux/i2c-old.h>

#include <linux/video_decoder.h>

#define DEBUG(x)		/* Debug driver */

/* ----------------------------------------------------------------------- */

struct saa7111 {
	struct i2c_bus *bus;
	int addr;
	unsigned char reg[32];

	int norm;
	int input;
	int enable;
	int bright;
	int contrast;
	int hue;
	int sat;
};

#define   I2C_SAA7111        0x48

#define   I2C_DELAY   10

/* ----------------------------------------------------------------------- */

static int saa7111_write(struct saa7111 *dev, unsigned char subaddr,
			 unsigned char data)
{
	int ack;

	LOCK_I2C_BUS(dev->bus);
	i2c_start(dev->bus);
	i2c_sendbyte(dev->bus, dev->addr, I2C_DELAY);
	i2c_sendbyte(dev->bus, subaddr, I2C_DELAY);
	ack = i2c_sendbyte(dev->bus, data, I2C_DELAY);
	dev->reg[subaddr] = data;
	i2c_stop(dev->bus);
	UNLOCK_I2C_BUS(dev->bus);
	return ack;
}

static int saa7111_write_block(struct saa7111 *dev,
	       unsigned const char *data, unsigned int len)
{
	int ack = -1;
	unsigned subaddr;

	while (len > 1) {
		LOCK_I2C_BUS(dev->bus);
		i2c_start(dev->bus);
		i2c_sendbyte(dev->bus, dev->addr, I2C_DELAY);
		ack = i2c_sendbyte(dev->bus, (subaddr = *data++), I2C_DELAY);
		ack = i2c_sendbyte(dev->bus, (dev->reg[subaddr] = *data++), I2C_DELAY);
		len -= 2;
		while (len > 1 && *data == ++subaddr) {
			data++;
			ack =
			    i2c_sendbyte(dev->bus,
					 (dev->reg[subaddr] =
					  *data++), I2C_DELAY);
			len -= 2;
		}
		i2c_stop(dev->bus);
		UNLOCK_I2C_BUS(dev->bus);
	}
	return ack;
}

static int saa7111_read(struct saa7111 *dev, unsigned char subaddr)
{
	int data;

	LOCK_I2C_BUS(dev->bus);
	i2c_start(dev->bus);
	i2c_sendbyte(dev->bus, dev->addr, I2C_DELAY);
	i2c_sendbyte(dev->bus, subaddr, I2C_DELAY);
	i2c_start(dev->bus);
	i2c_sendbyte(dev->bus, dev->addr | 1, I2C_DELAY);
	data = i2c_readbyte(dev->bus, 1);
	i2c_stop(dev->bus);
	UNLOCK_I2C_BUS(dev->bus);
	return data;
}

/* ----------------------------------------------------------------------- */

static int saa7111_attach(struct i2c_device *device)
{
	int i;
	struct saa7111 *decoder;

	static const unsigned char init[] = {
		0x00, 0x00,	/* 00 - ID byte */
		0x01, 0x00,	/* 01 - reserved */

		/*front end */
		0x02, 0xd0,	/* 02 - FUSE=3, GUDL=2, MODE=0 */
		0x03, 0x23,	/* 03 - HLNRS=0, VBSL=1, WPOFF=0, HOLDG=0, GAFIX=0, GAI1=256, GAI2=256 */
		0x04, 0x00,	/* 04 - GAI1=256 */
		0x05, 0x00,	/* 05 - GAI2=256 */

		/* decoder */
		0x06, 0xf3,	/* 06 - HSB at  13(50Hz) /  17(60Hz) pixels after end of last line */
		0x07, 0x13,	/* 07 - HSS at 113(50Hz) / 117(60Hz) pixels after end of last line */
		0x08, 0xc8,	/* 08 - AUFD=1, FSEL=1, EXFIL=0, VTRC=1, HPLL=0, VNOI=0 */
		0x09, 0x01,	/* 09 - BYPS=0, PREF=0, BPSS=0, VBLB=0, UPTCV=0, APER=1 */
		0x0a, 0x80,	/* 0a - BRIG=128 */
		0x0b, 0x47,	/* 0b - CONT=1.109 */
		0x0c, 0x40,	/* 0c - SATN=1.0 */
		0x0d, 0x00,	/* 0d - HUE=0 */
		0x0e, 0x01,	/* 0e - CDTO=0, CSTD=0, DCCF=0, FCTC=0, CHBW=1 */
		0x0f, 0x00,	/* 0f - reserved */
		0x10, 0x48,	/* 10 - OFTS=1, HDEL=0, VRLN=1, YDEL=0 */
		0x11, 0x1c,	/* 11 - GPSW=0, CM99=0, FECO=0, COMPO=1, OEYC=1, OEHV=1, VIPB=0, COLO=0 */
		0x12, 0x00,	/* 12 - output control 2 */
		0x13, 0x00,	/* 13 - output control 3 */
		0x14, 0x00,	/* 14 - reserved */
		0x15, 0x00,	/* 15 - VBI */
		0x16, 0x00,	/* 16 - VBI */
		0x17, 0x00,	/* 17 - VBI */
	};

	MOD_INC_USE_COUNT;

	device->data = decoder = kmalloc(sizeof(struct saa7111), GFP_KERNEL);
	if (decoder == NULL)
	{
		MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}

	memset(decoder, 0, sizeof(struct saa7111));
	strcpy(device->name, "saa7111");
	decoder->bus = device->bus;
	decoder->addr = device->addr;
	decoder->norm = VIDEO_MODE_NTSC;
	decoder->input = 0;
	decoder->enable = 1;
	decoder->bright = 32768;
	decoder->contrast = 32768;
	decoder->hue = 32768;
	decoder->sat = 32768;

	i = saa7111_write_block(decoder, init, sizeof(init));
	if (i < 0) {
		printk(KERN_ERR "%s_attach: init status %d\n",
		       device->name, i);
	} else {
		printk(KERN_INFO "%s_attach: chip version %x\n",
		       device->name, saa7111_read(decoder, 0x00) >> 4);
	}
	return 0;
}


static int saa7111_detach(struct i2c_device *device)
{
	kfree(device->data);
	MOD_DEC_USE_COUNT;
	return 0;
}

static int saa7111_command(struct i2c_device *device, unsigned int cmd,
			   void *arg)
{
	struct saa7111 *decoder = device->data;

	switch (cmd) {

#if defined(DECODER_DUMP)
	case DECODER_DUMP:
		{
			int i;

			for (i = 0; i < 32; i += 16) {
				int j;

				printk("KERN_DEBUG %s: %03x", device->name,
				       i);
				for (j = 0; j < 16; ++j) {
					printk(" %02x",
					       saa7111_read(decoder,
							    i + j));
				}
				printk("\n");
			}
		}
		break;
#endif				/* defined(DECODER_DUMP) */

	case DECODER_GET_CAPABILITIES:
		{
			struct video_decoder_capability *cap = arg;

			cap->flags
			    = VIDEO_DECODER_PAL
			    | VIDEO_DECODER_NTSC
			    | VIDEO_DECODER_AUTO | VIDEO_DECODER_CCIR;
			cap->inputs = 8;
			cap->outputs = 1;
		}
		break;

	case DECODER_GET_STATUS:
		{
			int *iarg = arg;
			int status;
			int res;

			status = saa7111_read(decoder, 0x1f);
			res = 0;
			if ((status & (1 << 6)) == 0) {
				res |= DECODER_STATUS_GOOD;
			}
			switch (decoder->norm) {
			case VIDEO_MODE_NTSC:
				res |= DECODER_STATUS_NTSC;
				break;
			case VIDEO_MODE_PAL:
				res |= DECODER_STATUS_PAL;
				break;
			default:
			case VIDEO_MODE_AUTO:
				if ((status & (1 << 5)) != 0) {
					res |= DECODER_STATUS_NTSC;
				} else {
					res |= DECODER_STATUS_PAL;
				}
				break;
			}
			if ((status & (1 << 0)) != 0) {
				res |= DECODER_STATUS_COLOR;
			}
			*iarg = res;
		}
		break;

	case DECODER_SET_NORM:
		{
			int *iarg = arg;

			switch (*iarg) {

			case VIDEO_MODE_NTSC:
				saa7111_write(decoder, 0x08,
					      (decoder->
					       reg[0x08] & 0x3f) | 0x40);
				break;

			case VIDEO_MODE_PAL:
				saa7111_write(decoder, 0x08,
					      (decoder->
					       reg[0x08] & 0x3f) | 0x00);
				break;

			case VIDEO_MODE_AUTO:
				saa7111_write(decoder, 0x08,
					      (decoder->
					       reg[0x08] & 0x3f) | 0x80);
				break;

			default:
				return -EINVAL;

			}
			decoder->norm = *iarg;
		}
		break;

	case DECODER_SET_INPUT:
		{
			int *iarg = arg;

			if (*iarg < 0 || *iarg > 7) {
				return -EINVAL;
			}

			if (decoder->input != *iarg) {
				decoder->input = *iarg;
				/* select mode */
				saa7111_write(decoder, 0x02,
					      (decoder->
					       reg[0x02] & 0xf8) |
					      decoder->input);
				/* bypass chrominance trap for modes 4..7 */
				saa7111_write(decoder, 0x09,
					      (decoder->
					       reg[0x09] & 0x7f) |
					      ((decoder->input >
						3) ? 0x80 : 0));
			}
		}
		break;

	case DECODER_SET_OUTPUT:
		{
			int *iarg = arg;

			/* not much choice of outputs */
			if (*iarg != 0) {
				return -EINVAL;
			}
		}
		break;

	case DECODER_ENABLE_OUTPUT:
		{
			int *iarg = arg;
			int enable = (*iarg != 0);

			if (decoder->enable != enable) {
				decoder->enable = enable;

// RJ: If output should be disabled (for playing videos), we also need a open PLL.
//     The input is set to 0 (where no input source is connected), although this
//     is not necessary.
//
//     If output should be enabled, we have to reverse the above.

				if (decoder->enable) {
					saa7111_write(decoder, 0x02,
						      (decoder->
						       reg[0x02] & 0xf8) |
						      decoder->input);
					saa7111_write(decoder, 0x08,
						      (decoder->
						       reg[0x08] & 0xfb));
					saa7111_write(decoder, 0x11,
						      (decoder->
						       reg[0x11] & 0xf3) |
						      0x0c);
				} else {
					saa7111_write(decoder, 0x02,
						      (decoder->
						       reg[0x02] & 0xf8));
					saa7111_write(decoder, 0x08,
						      (decoder->
						       reg[0x08] & 0xfb) |
						      0x04);
					saa7111_write(decoder, 0x11,
						      (decoder->
						       reg[0x11] & 0xf3));
				}
			}
		}
		break;

	case DECODER_SET_PICTURE:
		{
			struct video_picture *pic = arg;

			if (decoder->bright != pic->brightness) {
				/* We want 0 to 255 we get 0-65535 */
				decoder->bright = pic->brightness;
				saa7111_write(decoder, 0x0a,
					      decoder->bright >> 8);
			}
			if (decoder->contrast != pic->contrast) {
				/* We want 0 to 127 we get 0-65535 */
				decoder->contrast = pic->contrast;
				saa7111_write(decoder, 0x0b,
					      decoder->contrast >> 9);
			}
			if (decoder->sat != pic->colour) {
				/* We want 0 to 127 we get 0-65535 */
				decoder->sat = pic->colour;
				saa7111_write(decoder, 0x0c,
					      decoder->sat >> 9);
			}
			if (decoder->hue != pic->hue) {
				/* We want -128 to 127 we get 0-65535 */
				decoder->hue = pic->hue;
				saa7111_write(decoder, 0x0d,
					      (decoder->hue - 32768) >> 8);
			}
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/* ----------------------------------------------------------------------- */

static struct i2c_driver i2c_driver_saa7111 = {
	"saa7111",		/* name */
	I2C_DRIVERID_VIDEODECODER,	/* ID */
	I2C_SAA7111, I2C_SAA7111 + 1,

	saa7111_attach,
	saa7111_detach,
	saa7111_command
};

EXPORT_NO_SYMBOLS;

static int saa7111_init(void)
{
	return i2c_register_driver(&i2c_driver_saa7111);
}

static void saa7111_exit(void)
{
	i2c_unregister_driver(&i2c_driver_saa7111);
}

module_init(saa7111_init);
module_exit(saa7111_exit);
MODULE_LICENSE("GPL");
