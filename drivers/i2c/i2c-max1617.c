/*
 * Copyright (C) 2001,2002,2003 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * SMBus/I2C device driver for the MAX1617 temperature sensor
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/errno.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-sibyte.h>

#define IF_NAME "max1617"

#define MAX1617_SMBUS_DEV	0x2A
#define MAX1617_LOCAL           0
#define MAX1617_REMOTE          1
#define MAX1617_STATUS          2
#define MAX1617_POLL_PERIOD    10

static int max1617_verbose = 0;
static int max1617_polling = 1;

/* Addresses to scan */
static unsigned short normal_i2c[] = {MAX1617_SMBUS_DEV, I2C_CLIENT_END};
static unsigned short normal_i2c_range[] = {I2C_CLIENT_END};
static unsigned short probe[2]        = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short probe_range[2]  = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore[2]       = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore_range[2] = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short force[2]        = { I2C_CLIENT_END, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
        normal_i2c, normal_i2c_range,
        probe, probe_range,
        ignore, ignore_range,
        force
};

struct max1617_info {
        struct i2c_client *client;
        struct timer_list  timer;
        int                local;
        int                remote;
};

static int max1617_probe(struct i2c_adapter *adap);
static int max1617_detach(struct i2c_client *device);
static int max1617_command(struct i2c_client *device, unsigned int cmd, void *arg);
static void max1617_inc_use(struct i2c_client *device);
static void max1617_dec_use(struct i2c_client *device);

struct i2c_driver i2c_driver_max1617 = {
	name:           IF_NAME,
	id:             I2C_DRIVERID_MAX1617,
	flags:          I2C_DF_NOTIFY,
	attach_adapter: max1617_probe,
	detach_client:  max1617_detach,
	command:        max1617_command,
	inc_use:        max1617_inc_use,
	dec_use:        max1617_dec_use
};
\
static int max1617_read(struct i2c_client *client, unsigned char subaddr)
{
        return i2c_smbus_read_byte_data(client, subaddr);
}

/* poll the device, check for temperature/status changes */
static void max1617_update(unsigned long arg)
{
        struct max1617_info *m = (struct max1617_info *)arg;
        int status, remote, local;
        char statstr[50];

        status = max1617_read(m->client, MAX1617_STATUS);
        remote = max1617_read(m->client, MAX1617_REMOTE);
        local  = max1617_read(m->client, MAX1617_LOCAL);
        if (status < 0 || remote < 0 || local < 0) {
                printk(KERN_ERR IF_NAME ": sensor device did not respond.\n");
        } else {
                statstr[0] = 0;
                if (status & 0x80) strcat(statstr,"Busy ");
                if (status & 0x40) strcat(statstr,"HiTempLcl ");
                if (status & 0x20) strcat(statstr,"LoTempLcl ");
                if (status & 0x10) strcat(statstr,"HiTempRem ");
                if (status & 0x08) strcat(statstr,"LoTempRem ");
                if (status & 0x04) strcat(statstr,"Fault ");

                if (max1617_verbose || (local != m->local) || (remote != m->remote)) {
                        printk(KERN_DEBUG IF_NAME ": Temperature - CPU: %dC  Board: %dC  Status:%02X [ %s]\n",
                               remote, local, status, statstr);
                }
                m->local = local;
                m->remote = remote;
                mod_timer(&m->timer, jiffies + (HZ * MAX1617_POLL_PERIOD));
        }
}

/* attach to an instance of the device that was probed on a bus */
static int max1617_attach(struct i2c_adapter *adap, int addr, unsigned short flags, int kind)
{
        struct max1617_info *m;
        struct i2c_client   *client;
        int err;

        client = kmalloc(sizeof(*client), GFP_KERNEL);
        if (client == NULL)
                return -ENOMEM;
        client->adapter = adap;
        client->addr = addr;
	client->driver = &i2c_driver_max1617;
        sprintf(client->name, "%s-%x", IF_NAME, addr);
        if ((err = i2c_attach_client(client)) < 0) {
                kfree(client);
                return err;
        }

        m = kmalloc(sizeof(*m), GFP_KERNEL);
        if (m == NULL) {
                i2c_detach_client(client);
                kfree(client);
                return -ENOMEM;
        }
        m->client = client;
        m->remote = m->local = 0;
        init_timer(&m->timer);
        m->timer.data = (unsigned long)m;
        m->timer.function = max1617_update;
        if (max1617_polling) {
                m->timer.expires = jiffies + (HZ * MAX1617_POLL_PERIOD);
                add_timer(&m->timer);
        }
        client->data = m;
        return 0;
}

/* initiate probing on a particular bus */
static int max1617_probe(struct i2c_adapter *adap)
{
        /* Look for this device on the given adapter (bus) */
        if (adap->id == (I2C_ALGO_SIBYTE | I2C_HW_SIBYTE))
                return i2c_probe(adap, &addr_data, &max1617_attach);
        else
                return 0;
}

static int max1617_detach(struct i2c_client *device)
{
        struct max1617_info *m = (struct max1617_info *)device->data;
	int rc = 0;

	if ((rc = i2c_detach_client(device)) != 0) {
		printk(IF_NAME "detach failed: %d\n", rc);
	} else {
		kfree(device);
		if (max1617_polling)
			del_timer(&m->timer);
		kfree(m);
	}
	return rc;
}

static int max1617_command(struct i2c_client *device, unsigned int cmd, void *arg)
{
        return 0;
}

static void max1617_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void max1617_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

void i2c_max1617_init(void)
{
        i2c_add_driver(&i2c_driver_max1617);
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE
MODULE_AUTHOR("Kip Walker, Broadcom Corp.");
MODULE_DESCRIPTION("Max 1617 temperature sensor for SiByte SOC boards");
MODULE_LICENSE("GPL");

int init_module(void)
{
	i2c_max1617_init();
	return 0;
}

void cleanup_module(void)
{
        i2c_del_driver(&i2c_driver_max1617);
}
#endif
