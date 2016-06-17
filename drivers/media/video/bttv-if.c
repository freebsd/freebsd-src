/*
    bttv-if.c  --  interfaces to other kernel modules
	all the i2c code is here
	also the gpio interface exported by bttv (used by lirc)

    bttv - Bt848 frame grabber driver

    Copyright (C) 1996,97,98 Ralph  Metzler (rjkm@thp.uni-koeln.de)
                           & Marcus Metzler (mocm@thp.uni-koeln.de)
    (c) 1999-2003 Gerd Knorr <kraxel@bytesex.org>

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

#include <asm/io.h>

#include "bttvp.h"

static struct i2c_algo_bit_data bttv_i2c_algo_template;
static struct i2c_adapter bttv_i2c_adap_template;
static struct i2c_client bttv_i2c_client_template;

EXPORT_SYMBOL(bttv_get_cardinfo);
EXPORT_SYMBOL(bttv_get_pcidev);
EXPORT_SYMBOL(bttv_get_id);
EXPORT_SYMBOL(bttv_gpio_enable);
EXPORT_SYMBOL(bttv_read_gpio);
EXPORT_SYMBOL(bttv_write_gpio);
EXPORT_SYMBOL(bttv_get_gpio_queue);
EXPORT_SYMBOL(bttv_i2c_call);

/* ----------------------------------------------------------------------- */
/* Exported functions - for other modules which want to access the         */
/*                      gpio ports (IR for example)                        */
/*                      see bttv.h for comments                            */

int bttv_get_cardinfo(unsigned int card, int *type, unsigned *cardid)
{
	if (card >= bttv_num) {
		return -1;
	}
	*type   = bttvs[card].type;
	*cardid = bttvs[card].cardid;
	return 0;
}

struct pci_dev* bttv_get_pcidev(unsigned int card)
{
	if (card >= bttv_num)
		return NULL;
	return bttvs[card].dev;
}

int bttv_get_id(unsigned int card)
{
	printk("bttv_get_id is obsolete, use bttv_get_cardinfo instead\n");
	if (card >= bttv_num) {
		return -1;
	}
	return bttvs[card].type;
}

int bttv_gpio_enable(unsigned int card, unsigned long mask, unsigned long data)
{
	struct bttv *btv;

	if (card >= bttv_num) {
		return -EINVAL;
	}
	
	btv = &bttvs[card];
	btaor(data, ~mask, BT848_GPIO_OUT_EN);
	if (bttv_gpio)
		bttv_gpio_tracking(btv,"extern enable");
	return 0;
}

int bttv_read_gpio(unsigned int card, unsigned long *data)
{
	struct bttv *btv;
	
	if (card >= bttv_num) {
		return -EINVAL;
	}

	btv = &bttvs[card];

	if(btv->shutdown) {
		return -ENODEV;
	}

/* prior setting BT848_GPIO_REG_INP is (probably) not needed 
   because we set direct input on init */
	*data = btread(BT848_GPIO_DATA);
	return 0;
}

int bttv_write_gpio(unsigned int card, unsigned long mask, unsigned long data)
{
	struct bttv *btv;
	
	if (card >= bttv_num) {
		return -EINVAL;
	}

	btv = &bttvs[card];

/* prior setting BT848_GPIO_REG_INP is (probably) not needed 
   because direct input is set on init */
	btaor(data & mask, ~mask, BT848_GPIO_DATA);
	if (bttv_gpio)
		bttv_gpio_tracking(btv,"extern write");
	return 0;
}

wait_queue_head_t* bttv_get_gpio_queue(unsigned int card)
{
	struct bttv *btv;

	if (card >= bttv_num) {
		return NULL;
	}

	btv = &bttvs[card];
	if (bttvs[card].shutdown) {
		return NULL;
	}
	return &btv->gpioq;
}


/* ----------------------------------------------------------------------- */
/* I2C functions                                                           */

void bttv_bit_setscl(void *data, int state)
{
	struct bttv *btv = (struct bttv*)data;

	if (state)
		btv->i2c_state |= 0x02;
	else
		btv->i2c_state &= ~0x02;
	btwrite(btv->i2c_state, BT848_I2C);
	btread(BT848_I2C);
}

void bttv_bit_setsda(void *data, int state)
{
	struct bttv *btv = (struct bttv*)data;

	if (state)
		btv->i2c_state |= 0x01;
	else
		btv->i2c_state &= ~0x01;
	btwrite(btv->i2c_state, BT848_I2C);
	btread(BT848_I2C);
}

static int bttv_bit_getscl(void *data)
{
	struct bttv *btv = (struct bttv*)data;
	int state;
	
	state = btread(BT848_I2C) & 0x02 ? 1 : 0;
	return state;
}

static int bttv_bit_getsda(void *data)
{
	struct bttv *btv = (struct bttv*)data;
	int state;

	state = btread(BT848_I2C) & 0x01;
	return state;
}

#ifndef I2C_PEC
static void bttv_inc_use(struct i2c_adapter *adap)
{
	MOD_INC_USE_COUNT;
}

static void bttv_dec_use(struct i2c_adapter *adap)
{
	MOD_DEC_USE_COUNT;
}
#endif

static int attach_inform(struct i2c_client *client)
{
        struct bttv *btv = i2c_get_adapdata(client->adapter);

	if (btv->tuner_type != UNSET)
		bttv_call_i2c_clients(btv,TUNER_SET_TYPE,&btv->tuner_type);
	if (btv->pinnacle_id != UNSET)
		bttv_call_i2c_clients(btv,AUDC_CONFIG_PINNACLE,
				      &btv->pinnacle_id);

        if (bttv_debug)
		printk("bttv%d: i2c attach [client=%s]\n",
		       btv->nr, i2c_clientname(client));
        return 0;
}

void bttv_call_i2c_clients(struct bttv *btv, unsigned int cmd, void *arg)
{
	if (0 != btv->i2c_rc)
		return;
	i2c_clients_command(&btv->i2c_adap, cmd, arg);
}

void bttv_i2c_call(unsigned int card, unsigned int cmd, void *arg)
{
	if (card >= bttv_num)
		return;
	bttv_call_i2c_clients(&bttvs[card], cmd, arg);
}

static struct i2c_algo_bit_data bttv_i2c_algo_template = {
	.setsda  = bttv_bit_setsda,
	.setscl  = bttv_bit_setscl,
	.getsda  = bttv_bit_getsda,
	.getscl  = bttv_bit_getscl,
	.udelay  = 16,
	.mdelay  = 10,
	.timeout = 200,
};

static struct i2c_adapter bttv_i2c_adap_template = {
#ifdef I2C_PEC
	.owner             = THIS_MODULE,
#else
	.inc_use           = bttv_inc_use,
	.dec_use           = bttv_dec_use,
#endif
#ifdef I2C_ADAP_CLASS_TV_ANALOG
	.class             = I2C_ADAP_CLASS_TV_ANALOG,
#endif
	I2C_DEVNAME("bt848"),
	.id                = I2C_HW_B_BT848,
	.client_register   = attach_inform,
};

static struct i2c_client bttv_i2c_client_template = {
	I2C_DEVNAME("bttv internal"),
        .id       = -1,
};


/* read I2C */
int bttv_I2CRead(struct bttv *btv, unsigned char addr, char *probe_for) 
{
        unsigned char buffer = 0;

	if (0 != btv->i2c_rc)
		return -1;
	if (bttv_verbose && NULL != probe_for)
		printk(KERN_INFO "bttv%d: i2c: checking for %s @ 0x%02x... ",
		       btv->nr,probe_for,addr);
        btv->i2c_client.addr = addr >> 1;
        if (1 != i2c_master_recv(&btv->i2c_client, &buffer, 1)) {
		if (NULL != probe_for) {
			if (bttv_verbose)
				printk("not found\n");
		} else
			printk(KERN_WARNING "bttv%d: i2c read 0x%x: error\n",
			       btv->nr,addr);
                return -1;
	}
	if (bttv_verbose && NULL != probe_for)
		printk("found\n");
        return buffer;
}

/* write I2C */
int bttv_I2CWrite(struct bttv *btv, unsigned char addr, unsigned char b1,
                    unsigned char b2, int both)
{
        unsigned char buffer[2];
        int bytes = both ? 2 : 1;

	if (0 != btv->i2c_rc)
		return -1;
        btv->i2c_client.addr = addr >> 1;
        buffer[0] = b1;
        buffer[1] = b2;
        if (bytes != i2c_master_send(&btv->i2c_client, buffer, bytes))
		return -1;
        return 0;
}

/* read EEPROM content */
void __devinit bttv_readee(struct bttv *btv, unsigned char *eedata, int addr)
{
	int i;
        
	if (bttv_I2CWrite(btv, addr, 0, -1, 0)<0) {
		printk(KERN_WARNING "bttv: readee error\n");
		return;
	}
	btv->i2c_client.addr = addr >> 1;
	for (i=0; i<256; i+=16) {
		if (16 != i2c_master_recv(&btv->i2c_client,eedata+i,16)) {
			printk(KERN_WARNING "bttv: readee error\n");
			break;
		}
	}
}

/* init + register i2c algo-bit adapter */
int __devinit init_bttv_i2c(struct bttv *btv)
{
	memcpy(&btv->i2c_adap, &bttv_i2c_adap_template,
	       sizeof(struct i2c_adapter));
	memcpy(&btv->i2c_algo, &bttv_i2c_algo_template,
	       sizeof(struct i2c_algo_bit_data));
	memcpy(&btv->i2c_client, &bttv_i2c_client_template,
	       sizeof(struct i2c_client));

	sprintf(btv->i2c_adap.name, "bt848 #%d", btv->nr);

        btv->i2c_algo.data = btv;
        i2c_set_adapdata(&btv->i2c_adap, btv);
        btv->i2c_adap.algo_data = &btv->i2c_algo;
        btv->i2c_client.adapter = &btv->i2c_adap;

	bttv_bit_setscl(btv,1);
	bttv_bit_setsda(btv,1);

	btv->i2c_rc = i2c_bit_add_bus(&btv->i2c_adap);
	return btv->i2c_rc;
}

/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
