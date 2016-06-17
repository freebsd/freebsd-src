#define DEBUGLEVEL 0
/* 
    adv7175 - adv7175a video encoder driver version 0.0.3

    Copyright (C) 1998 Dave Perks <dperks@ibm.net>

    Copyright (C) 1999 Wolfgang Scherr <scherr@net4you.net>
    Copyright (C) 2000 Serguei Miridonov <mirsev@cicese.mx>
       - some corrections for Pinnacle Systems Inc. DC10plus card.

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
#include <linux/pci.h>
#include <linux/signal.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <linux/sched.h>
#include <asm/segment.h>
#include <linux/types.h>
#include <linux/wrapper.h>

#include <linux/videodev.h>
#include <linux/version.h>
#include <asm/uaccess.h>

#include <linux/i2c-old.h>

#include <linux/video_encoder.h>

#if (DEBUGLEVEL > 0)
#define DEBUG(x...)  x		/* Debug driver */
#else
#define DEBUG(x...)
#endif

/* ----------------------------------------------------------------------- */

struct adv7175 {
	struct i2c_bus *bus;
	int addr;
	unsigned char reg[128];

	int norm;
	int input;
	int enable;
	int bright;
	int contrast;
	int hue;
	int sat;
};

#define   I2C_ADV7175        0xd4
#define   I2C_ADV7176        0x54

static char adv7175_name[] = "adv7175";
static char adv7176_name[] = "adv7176";
static char unknown_name[] = "UNKNOWN";

#if (DEBUGLEVEL > 0)
static char *inputs[] = { "pass_through", "play_back", "color_bar" };
static char *norms[] = { "PAL", "NTSC", "SECAM->PAL (may not work!)" };
#endif

#define I2C_DELAY   10

/* ----------------------------------------------------------------------- */

static int adv7175_write(struct adv7175 *dev, unsigned char subaddr, unsigned char data)
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

static unsigned char adv7175_read(struct adv7175 *dev, unsigned char subaddr)
{
	unsigned char data;

	LOCK_I2C_BUS(dev->bus);

	i2c_start(dev->bus);
	i2c_sendbyte(dev->bus, dev->addr, I2C_DELAY);
	i2c_sendbyte(dev->bus, subaddr, I2C_DELAY);
	i2c_sendbyte(dev->bus, dev->addr + 1, I2C_DELAY);
	data = i2c_readbyte(dev->bus, 1);
	dev->reg[subaddr] = data;
	i2c_stop(dev->bus);
	UNLOCK_I2C_BUS(dev->bus);
	return data;
}

static int adv7175_write_block(struct adv7175 *dev,
			       unsigned const char *data, unsigned int len)
{
	int ack = 0;
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
			ack = i2c_sendbyte(dev->bus, (dev->reg[subaddr] = *data++), I2C_DELAY);
			len -= 2;
		}
		i2c_stop(dev->bus);
		UNLOCK_I2C_BUS(dev->bus);
	}
	return ack;
}

/* ----------------------------------------------------------------------- */
// Output filter:  S-Video  Composite

#define MR050       0x11	//0x09
#define MR060       0x14	//0x0c

//---------------------------------------------------------------------------

#define TR0MODE     0x46
#define TR0RST	    0x80

#define TR1CAPT	    0x80
#define TR1PLAY	    0x00

static const unsigned char init_common[] = {

	0x00, MR050,		/* MR0, PAL enabled */
	0x01, 0x00,		/* MR1 */
	0x02, 0x0c,		/* subc. freq. */
	0x03, 0x8c,		/* subc. freq. */
	0x04, 0x79,		/* subc. freq. */
	0x05, 0x26,		/* subc. freq. */
	0x06, 0x40,		/* subc. phase */

	0x07, TR0MODE,		/* TR0, 16bit */
	0x08, 0x21,		/*  */
	0x09, 0x00,		/*  */
	0x0a, 0x00,		/*  */
	0x0b, 0x00,		/*  */
	0x0c, TR1CAPT,		/* TR1 */
	0x0d, 0x4f,		/* MR2 */
	0x0e, 0x00,		/*  */
	0x0f, 0x00,		/*  */
	0x10, 0x00,		/*  */
	0x11, 0x00,		/*  */
	0x12, 0x00,		/* MR3 */
	0x24, 0x00,		/*  */
};

static const unsigned char init_pal[] = {
	0x00, MR050,		/* MR0, PAL enabled */
	0x01, 0x00,		/* MR1 */
	0x02, 0x0c,		/* subc. freq. */
	0x03, 0x8c,		/* subc. freq. */
	0x04, 0x79,		/* subc. freq. */
	0x05, 0x26,		/* subc. freq. */
	0x06, 0x40,		/* subc. phase */
};

static const unsigned char init_ntsc[] = {
	0x00, MR060,		/* MR0, NTSC enabled */
	0x01, 0x00,		/* MR1 */
	0x02, 0x55,		/* subc. freq. */
	0x03, 0x55,		/* subc. freq. */
	0x04, 0x55,		/* subc. freq. */
	0x05, 0x25,		/* subc. freq. */
	0x06, 0x1a,		/* subc. phase */
};

static int adv7175_attach(struct i2c_device *device)
{
	int i;
	struct adv7175 *encoder;
	char *dname;

	MOD_INC_USE_COUNT;

	device->data = encoder = kmalloc(sizeof(struct adv7175), GFP_KERNEL);
	if (encoder == NULL) {
		MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}


	memset(encoder, 0, sizeof(struct adv7175));
	if ((device->addr == I2C_ADV7175) || (device->addr == (I2C_ADV7175 + 2))) {
		dname = adv7175_name;
	} else if ((device->addr == I2C_ADV7176) || (device->addr == (I2C_ADV7176 + 2))) {
		dname = adv7176_name;
	} else {
		// We should never get here!!!
		dname = unknown_name;
	}
	strcpy(device->name, dname);
	encoder->bus = device->bus;
	encoder->addr = device->addr;
	encoder->norm = VIDEO_MODE_PAL;
	encoder->input = 0;
	encoder->enable = 1;

	i = adv7175_write_block(encoder, init_common, sizeof(init_common));
	if (i >= 0) {
		i = adv7175_write(encoder, 0x07, TR0MODE | TR0RST);
		i = adv7175_write(encoder, 0x07, TR0MODE);
		i = adv7175_read(encoder, 0x12);
		printk(KERN_INFO "%s_attach: %s rev. %d at 0x%02x\n",
		       device->name, dname, i & 1, device->addr);
	}
	if (i < 0) {
		printk(KERN_ERR "%s_attach: init error %d\n", device->name,
		       i);
	}

	return 0;
}


static int adv7175_detach(struct i2c_device *device)
{
	kfree(device->data);
	MOD_DEC_USE_COUNT;
	return 0;
}

static int adv7175_command(struct i2c_device *device, unsigned int cmd,
			   void *arg)
{
	struct adv7175 *encoder = device->data;

	switch (cmd) {

	case ENCODER_GET_CAPABILITIES:
		{
			struct video_encoder_capability *cap = arg;

			cap->flags = VIDEO_ENCODER_PAL | VIDEO_ENCODER_NTSC
			    // | VIDEO_ENCODER_SECAM
			    // | VIDEO_ENCODER_CCIR
			    ;
			cap->inputs = 2;
			cap->outputs = 1;
		}
		break;

	case ENCODER_SET_NORM:
		{
			int iarg = *(int *) arg;

			if (encoder->norm != iarg) {
				switch (iarg) {

				case VIDEO_MODE_NTSC:
					adv7175_write_block(encoder,
							    init_ntsc,
							    sizeof
							    (init_ntsc));
					if (encoder->input == 0)
						adv7175_write(encoder, 0x0d, 0x4f);	// Enable genlock
					adv7175_write(encoder, 0x07,
						      TR0MODE | TR0RST);
					adv7175_write(encoder, 0x07,
						      TR0MODE);
					break;

				case VIDEO_MODE_PAL:
					adv7175_write_block(encoder,
							    init_pal,
							    sizeof
							    (init_pal));
					if (encoder->input == 0)
						adv7175_write(encoder, 0x0d, 0x4f);	// Enable genlock
					adv7175_write(encoder, 0x07,
						      TR0MODE | TR0RST);
					adv7175_write(encoder, 0x07,
						      TR0MODE);
					break;

				case VIDEO_MODE_SECAM:	// WARNING! ADV7176 does not support SECAM.
					// This is an attempt to convert SECAM->PAL (typically
					// it does not work due to genlock: when decoder
					// is in SECAM and encoder in in PAL the subcarrier
					// can not be syncronized with horizontal frequency)
					adv7175_write_block(encoder,
							    init_pal,
							    sizeof
							    (init_pal));
					if (encoder->input == 0)
						adv7175_write(encoder, 0x0d, 0x49);	// Disable genlock
					adv7175_write(encoder, 0x07,
						      TR0MODE | TR0RST);
					adv7175_write(encoder, 0x07,
						      TR0MODE);
					break;
				default:
					printk(KERN_ERR
					       "%s: illegal norm: %d\n",
					       device->name, iarg);
					return -EINVAL;

				}
				DEBUG(printk
				      (KERN_INFO "%s: switched to %s\n",
				       device->name, norms[iarg]));
				encoder->norm = iarg;
			}
		}
		break;

	case ENCODER_SET_INPUT:
		{
			int iarg = *(int *) arg;

			/* RJ: *iarg = 0: input is from SAA7110
			   *iarg = 1: input is from ZR36060
			   *iarg = 2: color bar */

			if (encoder->input != iarg) {
				switch (iarg) {

				case 0:
					adv7175_write(encoder, 0x01, 0x00);
					adv7175_write(encoder, 0x0c, TR1CAPT);	/* TR1 */
					if (encoder->norm ==
					    VIDEO_MODE_SECAM)
						adv7175_write(encoder, 0x0d, 0x49);	// Disable genlock
					else
						adv7175_write(encoder, 0x0d, 0x4f);	// Enable genlock
					adv7175_write(encoder, 0x07,
						      TR0MODE | TR0RST);
					adv7175_write(encoder, 0x07,
						      TR0MODE);
					//udelay(10);
					break;

				case 1:
					adv7175_write(encoder, 0x01, 0x00);
					adv7175_write(encoder, 0x0c, TR1PLAY);	/* TR1 */
					adv7175_write(encoder, 0x0d, 0x49);
					adv7175_write(encoder, 0x07,
						      TR0MODE | TR0RST);
					adv7175_write(encoder, 0x07,
						      TR0MODE);
					//udelay(10);
					break;

				case 2:
					adv7175_write(encoder, 0x01, 0x80);
					adv7175_write(encoder, 0x0d, 0x49);
					adv7175_write(encoder, 0x07,
						      TR0MODE | TR0RST);
					adv7175_write(encoder, 0x07,
						      TR0MODE);
					//udelay(10);
					break;

				default:
					printk(KERN_ERR
					       "%s: illegal input: %d\n",
					       device->name, iarg);
					return -EINVAL;

				}
				DEBUG(printk
				      (KERN_INFO "%s: switched to %s\n",
				       device->name, inputs[iarg]));
				encoder->input = iarg;
			}
		}
		break;

	case ENCODER_SET_OUTPUT:
		{
			int *iarg = arg;

			/* not much choice of outputs */
			if (*iarg != 0) {
				return -EINVAL;
			}
		}
		break;

	case ENCODER_ENABLE_OUTPUT:
		{
			int *iarg = arg;

			encoder->enable = !!*iarg;
			adv7175_write(encoder, 0x61,
				      (encoder->
				       reg[0x61] & 0xbf) | (encoder->
							    enable ? 0x00 :
							    0x40));
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/* ----------------------------------------------------------------------- */

static struct i2c_driver i2c_driver_adv7175 = {
	"adv7175",		/* name */
	I2C_DRIVERID_VIDEOENCODER,	/* ID */
	I2C_ADV7175, I2C_ADV7175 + 3,

	adv7175_attach,
	adv7175_detach,
	adv7175_command
};

static struct i2c_driver i2c_driver_adv7176 = {
	"adv7175",		/* name */
	I2C_DRIVERID_VIDEOENCODER,	/* ID */
	I2C_ADV7176, I2C_ADV7176 + 3,

	adv7175_attach,
	adv7175_detach,
	adv7175_command
};

EXPORT_NO_SYMBOLS;

static int adv7175_init(void)
{
	int res_adv7175 = 0, res_adv7176 = 0;
	res_adv7175 = i2c_register_driver(&i2c_driver_adv7175);
	res_adv7176 = i2c_register_driver(&i2c_driver_adv7176);
	return (res_adv7175 | res_adv7176);	// Any idea how to make it better?
}

static void adv7175_exit(void)
{
	i2c_unregister_driver(&i2c_driver_adv7176);
	i2c_unregister_driver(&i2c_driver_adv7175);
}

module_init(adv7175_init);
module_exit(adv7175_exit);
MODULE_LICENSE("GPL");
