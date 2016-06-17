/* 
 *  saa7191 - Philips SAA7191 video decoder driver
 *
 *  Copyright (C) 2003 Ladislav Michl <ladis@linux-mips.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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
#include <linux/video_decoder.h>
#include <linux/i2c.h>

#include "saa7191.h"

#define VINO_ADAPTER	(I2C_ALGO_SGI | I2C_HW_SGI_VINO)

struct saa7191 {
	struct i2c_client *client;
	unsigned char reg[25];
	unsigned char norm;
	unsigned char input;
	unsigned char bright;
	unsigned char contrast;
	unsigned char hue;
	unsigned char sat;
};

static struct i2c_driver i2c_driver_saa7191;

static int saa7191_attach(struct i2c_adapter *adap, int addr, int kind)
{
	static const unsigned char initseq[] = {
		0,
		0x50,	/* SAA7191_REG_IDEL */
		0x30,	/* SAA7191_REG_HSYB */
		0x00,	/* SAA7191_REG_HSYS */
		0xe8,	/* SAA7191_REG_HCLB */
		0xb6,	/* SAA7191_REG_HCLS */
		0xf4,	/* SAA7191_REG_HPHI */
		0x01,	/* SAA7191_REG_LUMA */
		0x00,	/* SAA7191_REG_HUEC */
		0xf8,	/* SAA7191_REG_CKTQ */
		0xf8,	/* SAA7191_REG_CKTS */
		0x90,	/* SAA7191_REG_PLSE */
		0x90,	/* SAA7191_REG_SESE */
		0x00,	/* SAA7191_REG_GAIN */
		0x8c,	/* SAA7191_REG_STDC */
		0x78,	/* SAA7191_REG_IOCK */
		0x99,	/* SAA7191_REG_CTL3 */
		0x00,	/* SAA7191_REG_CTL4 */
		0x2c,	/* SAA7191_REG_CHCV */
		0x00,	/* unused */
		0x00,	/* unused */
		0x34,	/* SAA7191_REG_HS6B */
		0x0a,	/* SAA7191_REG_HS6S */
		0xf4,	/* SAA7191_REG_HC6B */
		0xce,	/* SAA7191_REG_HC6S */
		0xf4,	/* SAA7191_REG_HP6I */
	};
	int err = 0;
	struct saa7191 *decoder;
	struct i2c_client *client;

	client = kmalloc(sizeof(*client), GFP_KERNEL);
	if (!client) 
		return -ENOMEM;
	decoder = kmalloc(sizeof(*decoder), GFP_KERNEL);
	if (!decoder) {
		err = -ENOMEM;
		goto out_free_client;
	}
	client->data = decoder;
	client->adapter = adap;
	client->addr = addr;
	client->driver = &i2c_driver_saa7191;
	strcpy(client->name, "saa7191 client");			
	decoder->client = client;

	err = i2c_attach_client(client);
	if (err)
		goto out_free_decoder;

	decoder->norm = VIDEO_DECODER_PAL | VIDEO_MODE_NTSC |
			VIDEO_DECODER_AUTO;
	decoder->input = 0;
	/* Registers are 8bit wide and we are storing shifted values */
	decoder->bright = 128;
	decoder->contrast = 128;
	decoder->hue = 128;
	decoder->sat = 128;

	memcpy(decoder->reg, initseq, sizeof(initseq));
	err = i2c_master_send(client, initseq, sizeof(initseq));
	if (err)
		printk(KERN_INFO "saa7191 initialization failed (%d)\n", err);

	MOD_INC_USE_COUNT;
	return 0;

out_free_decoder:
	kfree(decoder);
out_free_client:
	kfree(client);
	return err;
}

static int saa7191_probe(struct i2c_adapter *adap)
{
	/* Always connected to VINO */
	if (adap->id == VINO_ADAPTER)
		return saa7191_attach(adap, SAA7191_ADDR, 0);
	/* Feel free to add probe here :-) */
	return -ENODEV;
}

static int saa7191_detach(struct i2c_client *client)
{
	struct saa7191 *decoder = client->data;

	i2c_detach_client(client);
	kfree(decoder);
	kfree(client);
	MOD_DEC_USE_COUNT;
	return 0;
}

static unsigned char saa7191_read(struct i2c_client *client,
				  unsigned char command)
{
	return ((struct saa7191 *)client->data)->reg[command];
}

static int saa7191_write(struct i2c_client *client, unsigned char command,
			 unsigned char value)
{
	((struct saa7191 *)client->data)->reg[command] = value;
	return i2c_smbus_write_byte_data(client, command, value);
}

static int vino_set_input(struct i2c_client *client, int val)
{
	unsigned char luma = saa7191_read(client, SAA7191_REG_LUMA);
	unsigned char iock = saa7191_read(client, SAA7191_REG_IOCK);

	switch (val) {
	case 0: /* Set Composite input */
		iock &= ~0x03;
		/* Chrominance trap active */
		luma |= ~SAA7191_LUMA_BYPS;
		break;
	case 1: /* Set S-Video input */
		iock |= 2;
		/* Chrominance trap bypassed */
		luma |= SAA7191_LUMA_BYPS;
		break;
	default:
		return -EINVAL;
	}
	saa7191_write(client, SAA7191_REG_LUMA, luma);
	saa7191_write(client, SAA7191_REG_IOCK, iock);

	return 0;
}

static int saa7191_command(struct i2c_client *client, unsigned int cmd,
			   void *arg)
{
	struct saa7191 *decoder = client->data;

	switch (cmd) {

	case DECODER_GET_CAPABILITIES: {
		struct video_decoder_capability *cap = arg;

		cap->flags  = VIDEO_DECODER_PAL | VIDEO_DECODER_NTSC |
			      VIDEO_DECODER_AUTO;
		cap->inputs = (client->adapter->id == VINO_ADAPTER) ? 2 : 1;
		cap->outputs = 1;
		break;
	}
	case DECODER_GET_STATUS: {
		int *iarg = arg;
		int status;
		int res = 0;

		status = i2c_smbus_read_byte_data(client, SAA7191_REG_STATUS);
		if ((status & SAA7191_STATUS_HLCK) == 0)
			res |= DECODER_STATUS_GOOD;
		switch (decoder->norm) {
		case VIDEO_MODE_NTSC:
			res |= DECODER_STATUS_NTSC;
			break;
		case VIDEO_MODE_PAL:
			res |= DECODER_STATUS_PAL;
			break;
		case VIDEO_MODE_AUTO:
		default:
			if (status & SAA7191_STATUS_FIDT)
				res |= DECODER_STATUS_NTSC;
			else
				res |= DECODER_STATUS_PAL;
			if (status & SAA7191_STATUS_CODE)
				res |= DECODER_STATUS_COLOR;
			break;
		}
		*iarg = res;
		break;
	}
	case DECODER_SET_NORM: {
		int *iarg = arg;

		switch (*iarg) {
		case VIDEO_MODE_NTSC:
			break;
		case VIDEO_MODE_PAL:
			break;
		default:
			return -EINVAL;
		}
		decoder->norm = *iarg;
		break;
	}
	case DECODER_SET_INPUT:	{
		int *iarg = arg;

		switch (client->adapter->id) {
		case VINO_ADAPTER:
			return vino_set_input(client, *iarg);
		default:
			if (*iarg != 0)
				return -EINVAL;
		}
		break;
	}
	case DECODER_SET_OUTPUT: {
		int *iarg = arg;

		/* not much choice of outputs */
		if (*iarg != 0)
			return -EINVAL;
		break;
	}
	case DECODER_ENABLE_OUTPUT: {
		/* Always enabled */
		break;
	}
	case DECODER_SET_PICTURE: {
		struct video_picture *pic = arg;
		unsigned val;
#if 0
		/* TODO */
		val = pic->brightness >> 8;
		if (decoder->bright != val) {
			decoder->bright = val;
			i2c_smbus_write_byte_data(client, XXX, val);
		}
		val = pic->contrast >> 8;
		if (decoder->contrast != val) {
			decoder->contrast = val;
			i2c_smbus_write_byte_data(client, XXX, val);
		}
		val = pic->colour >> 8;
		if (decoder->sat != val) {
			decoder->sat = val;
			i2c_smbus_write_byte_data(client, XXX, val);
		}
#endif
		val = (pic->hue >> 8) - 0x80;
		if (decoder->hue != val) {
			decoder->hue = val;
			i2c_smbus_write_byte_data(client, SAA7191_REG_HUEC,
						  val);
		}
		break;
	}
	default:
		return -EINVAL;
	}

	return 0;
}

static struct i2c_driver i2c_driver_saa7191 = {
	.name 		= "saa7191",
	.id 		= I2C_DRIVERID_SAA7191,
	.flags 		= I2C_DF_NOTIFY,
	.attach_adapter = saa7191_probe,
	.detach_client 	= saa7191_detach,
	.command 	= saa7191_command
};

static int saa7191_init(void)
{
	return i2c_add_driver(&i2c_driver_saa7191);
}

static void saa7191_exit(void)
{
	i2c_del_driver(&i2c_driver_saa7191);
}

module_init(saa7191_init);
module_exit(saa7191_exit);

MODULE_DESCRIPTION("Philips SAA7191 video decoder driver");
MODULE_AUTHOR("Ladislav Michl <ladis@linux-mips.org>");
MODULE_LICENSE("GPL");
