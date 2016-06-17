/*
 * I2C driver for parallel port
 *
 * Author: Phil Blundell <philb@gnu.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * This driver implements a simple I2C protocol by bit-twiddling some
 * signals on the parallel port.  Since the outputs on the parallel port
 * aren't open collector, three lines rather than two are used:
 *
 *	D0	clock out
 *	D1	data out
 *	BUSY	data in	
 */

#include <linux/parport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c-old.h>
#include <linux/init.h>
#include <linux/spinlock.h>

#define I2C_DELAY   10

static int debug = 0;

struct parport_i2c_bus
{
  struct i2c_bus i2c;
  struct parport_i2c_bus *next;
};

static struct parport_i2c_bus *bus_list;

static spinlock_t bus_list_lock = SPIN_LOCK_UNLOCKED;

/* software I2C functions */

static void i2c_setlines(struct i2c_bus *bus, int clk, int data)
{
  struct parport *p = bus->data;
  parport_write_data(p, (clk?1:0) | (data?2:0)); 
  udelay(I2C_DELAY);
}

static int i2c_getdataline(struct i2c_bus *bus)
{
  struct parport *p = bus->data;
  return (parport_read_status(p) & PARPORT_STATUS_BUSY) ? 0 : 1;
}

static struct i2c_bus parport_i2c_bus_template = 
{
  "...",
  I2C_BUSID_PARPORT,
  NULL,
  
  SPIN_LOCK_UNLOCKED,
  
  NULL,
  NULL,
	
  i2c_setlines,
  i2c_getdataline,
  NULL,
  NULL,
};

static void i2c_parport_attach(struct parport *port)
{
  struct parport_i2c_bus *b = kmalloc(sizeof(struct parport_i2c_bus), 
				      GFP_KERNEL);
  if (!b) {
	  printk(KERN_ERR "i2c_parport: Memory allocation failed. Not attaching.\n");
	  return;
  }
  b->i2c = parport_i2c_bus_template;
  b->i2c.data = parport_get_port (port);
  strncpy(b->i2c.name, port->name, 32);
  spin_lock(&bus_list_lock);
  b->next = bus_list;
  bus_list = b;
  spin_unlock(&bus_list_lock);
  i2c_register_bus(&b->i2c);
  if (debug)
    printk(KERN_DEBUG "i2c: attached to %s\n", port->name);
}

static void i2c_parport_detach(struct parport *port)
{
  struct parport_i2c_bus *b, *old_b = NULL;
  spin_lock(&bus_list_lock);
  b = bus_list;
  while (b)
  {
    if (b->i2c.data == port)
    {
      if (old_b)
	old_b->next = b->next;
      else
	bus_list = b->next;
      i2c_unregister_bus(&b->i2c);
      kfree(b);
      break;
    }
    old_b = b;
    b = b->next;
  }
  spin_unlock(&bus_list_lock);
  if (debug)
    printk(KERN_DEBUG "i2c: detached from %s\n", port->name);
}

static struct parport_driver parport_i2c_driver = 
{
  "i2c",
  i2c_parport_attach,
  i2c_parport_detach
};

#ifdef MODULE
int init_module(void)
#else
int __init i2c_parport_init(void)
#endif
{
  printk("I2C: driver for parallel port v0.1 philb@gnu.org\n");
  parport_register_driver(&parport_i2c_driver);
  return 0;
}

#ifdef MODULE
MODULE_PARM(debug, "i");

void cleanup_module(void)
{
  struct parport_i2c_bus *b = bus_list;
  while (b)
  {
    struct parport_i2c_bus *next = b->next;
    i2c_unregister_bus(&b->i2c);
    kfree(b);
    b = next;
  }
  parport_unregister_driver(&parport_i2c_driver);
}
#endif
MODULE_LICENSE("GPL");
