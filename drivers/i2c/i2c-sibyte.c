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

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>

#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_smbus.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-sibyte.h>

static int sibyte_reg(struct i2c_client *client)
{
	return 0;
}

static int sibyte_unreg(struct i2c_client *client)
{
	return 0;
}

static void sibyte_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void sibyte_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static struct i2c_algo_sibyte_data sibyte_board_data[2] = {
        { NULL, 0, (void *)(KSEG1+A_SMB_BASE(0)) },
        { NULL, 1, (void *)(KSEG1+A_SMB_BASE(1)) }
};

static struct i2c_adapter sibyte_board_adapter[2] = {
        {
                name:              "SiByte SMBus 0",
                id:                I2C_HW_SIBYTE,
                algo:              NULL,
                algo_data:         &sibyte_board_data[0],
                inc_use:           sibyte_inc_use,
                dec_use:           sibyte_dec_use,
                client_register:   sibyte_reg,
                client_unregister: sibyte_unreg,
		client_count:      0
        } , 
        {
                name:              "SiByte SMBus 1",
                id:                I2C_HW_SIBYTE,
                algo:              NULL,
                algo_data:         &sibyte_board_data[1],
                inc_use:           sibyte_inc_use,
                dec_use:           sibyte_dec_use,
                client_register:   sibyte_reg,
                client_unregister: sibyte_unreg,
		client_count:      0
        }
};

int __init i2c_sibyte_init(void)
{
	printk("i2c-swarm.o: i2c SMBus adapter module for SiByte board\n");
        if (i2c_sibyte_add_bus(&sibyte_board_adapter[0], K_SMB_FREQ_100KHZ) < 0)
                return -ENODEV;
        if (i2c_sibyte_add_bus(&sibyte_board_adapter[1], K_SMB_FREQ_400KHZ) < 0)
                return -ENODEV;
	return 0;
}


EXPORT_NO_SYMBOLS;

#ifdef MODULE
MODULE_AUTHOR("Kip Walker, Broadcom Corp.");
MODULE_DESCRIPTION("SMBus adapter routines for SiByte boards");
MODULE_LICENSE("GPL");

int init_module(void)
{
	return i2c_sibyte_init();
}

void cleanup_module(void)
{
	i2c_sibyte_del_bus(&sibyte_board_adapter[0]);
	i2c_sibyte_del_bus(&sibyte_board_adapter[1]);
}

#endif
