/*
 *	Generic i2c interface for linux
 *
 *	(c) 1998 Gerd Knorr <kraxel@cs.tu-berlin.de>
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/locks.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/i2c-old.h>

#define REGPRINT(x)   if (verbose)   (x)
#define I2C_DEBUG(x)  if (i2c_debug) (x)

static int scan      = 0;
static int verbose   = 0;
static int i2c_debug = 0;

#if LINUX_VERSION_CODE >= 0x020117
MODULE_PARM(scan,"i");
MODULE_PARM(verbose,"i");
MODULE_PARM(i2c_debug,"i");
#endif

/* ----------------------------------------------------------------------- */

static struct i2c_bus    *busses[I2C_BUS_MAX];
static struct i2c_driver *drivers[I2C_DRIVER_MAX];
static int bus_count = 0, driver_count = 0;

int i2c_init(void)
{
	printk(KERN_INFO "i2c: initialized%s\n",
		scan ? " (i2c bus scan enabled)" : "");

	return 0;
}

/* ----------------------------------------------------------------------- */

static void i2c_attach_device(struct i2c_bus *bus, struct i2c_driver *driver)
{
	struct i2c_device *device;
	int i,j,ack=1;
	unsigned char addr;
	LOCK_FLAGS;
    
	/* probe for device */
	LOCK_I2C_BUS(bus);
	for (addr = driver->addr_l; addr <= driver->addr_h; addr += 2) 
	{
		i2c_start(bus);
		ack = i2c_sendbyte(bus,addr,0);
		i2c_stop(bus);
		if (!ack)
			break;
	}
	UNLOCK_I2C_BUS(bus);
	if (ack)
		return;

	/* got answer */
	for (i = 0; i < I2C_DEVICE_MAX; i++)
		if (NULL == driver->devices[i])
			break;
	if (I2C_DEVICE_MAX == i)
		return;

	for (j = 0; j < I2C_DEVICE_MAX; j++)
		if (NULL == bus->devices[j])
			break;
	if (I2C_DEVICE_MAX == j)
		return;

	if (NULL == (device = kmalloc(sizeof(struct i2c_device),GFP_KERNEL)))
		return;
	device->bus = bus;
	device->driver = driver;
	device->addr = addr;

	/* Attach */
	
	if (driver->attach(device)!=0) 
	{
		kfree(device);
		return;
	}
	driver->devices[i] = device;
	driver->devcount++;
	bus->devices[j] = device;
	bus->devcount++;

	if (bus->attach_inform)
		bus->attach_inform(bus,driver->id);
	REGPRINT(printk("i2c: device attached: %s (addr=0x%02x, bus=%s, driver=%s)\n",device->name,addr,bus->name,driver->name));
}

static void i2c_detach_device(struct i2c_device *device)
{
	int i;

	if (device->bus->detach_inform)
		device->bus->detach_inform(device->bus,device->driver->id);
	device->driver->detach(device);

	for (i = 0; i < I2C_DEVICE_MAX; i++)
		if (device == device->driver->devices[i])
			break;
	if (I2C_DEVICE_MAX == i) 
	{
		printk(KERN_WARNING "i2c: detach_device #1: device not found: %s\n",
			device->name);
		return;
	}
	device->driver->devices[i] = NULL;
	device->driver->devcount--;

	for (i = 0; i < I2C_DEVICE_MAX; i++)
		if (device == device->bus->devices[i])
			break;
	if (I2C_DEVICE_MAX == i) 
	{
		printk(KERN_WARNING "i2c: detach_device #2: device not found: %s\n",
		       device->name);
		return;
	}
	device->bus->devices[i] = NULL;
	device->bus->devcount--;

	REGPRINT(printk("i2c: device detached: %s (addr=0x%02x, bus=%s, driver=%s)\n",device->name,device->addr,device->bus->name,device->driver->name));
	kfree(device);
}

/* ----------------------------------------------------------------------- */

int i2c_register_bus(struct i2c_bus *bus)
{
	int i,ack;
	LOCK_FLAGS;

	memset(bus->devices,0,sizeof(bus->devices));
	bus->devcount = 0;

	for (i = 0; i < I2C_BUS_MAX; i++)
		if (NULL == busses[i])
			break;
	if (I2C_BUS_MAX == i)
		return -ENOMEM;

	busses[i] = bus;
	bus_count++;
	REGPRINT(printk("i2c: bus registered: %s\n",bus->name));
	
	MOD_INC_USE_COUNT;

	if (scan) 
	{
		/* scan whole i2c bus */
		LOCK_I2C_BUS(bus);
		for (i = 0; i < 256; i+=2) 
		{
			i2c_start(bus);
			ack = i2c_sendbyte(bus,i,0);
			i2c_stop(bus);
			if (!ack) 
			{
				printk(KERN_INFO "i2c: scanning bus %s: found device at addr=0x%02x\n",
					bus->name,i);
			}
		}
		UNLOCK_I2C_BUS(bus);
	}

	/* probe available drivers */
	for (i = 0; i < I2C_DRIVER_MAX; i++)
		if (drivers[i])
			i2c_attach_device(bus,drivers[i]);
	return 0;
}

int i2c_unregister_bus(struct i2c_bus *bus)
{
	int i;

	/* detach devices */
	for (i = 0; i < I2C_DEVICE_MAX; i++)
		if (bus->devices[i])
			i2c_detach_device(bus->devices[i]);

	for (i = 0; i < I2C_BUS_MAX; i++)
		if (bus == busses[i])
			break;
	if (I2C_BUS_MAX == i) 
	{
		printk(KERN_WARNING "i2c: unregister_bus #1: bus not found: %s\n",
			bus->name);
		return -ENODEV;
	}
	
	MOD_DEC_USE_COUNT;
	
	busses[i] = NULL;
	bus_count--;
	REGPRINT(printk("i2c: bus unregistered: %s\n",bus->name));

	return 0;    
}

/* ----------------------------------------------------------------------- */

int i2c_register_driver(struct i2c_driver *driver)
{
	int i;

	memset(driver->devices,0,sizeof(driver->devices));
	driver->devcount = 0;

	for (i = 0; i < I2C_DRIVER_MAX; i++)
		if (NULL == drivers[i])
			break;
	if (I2C_DRIVER_MAX == i)
		return -ENOMEM;

	drivers[i] = driver;
	driver_count++;
	
	MOD_INC_USE_COUNT;
	
	REGPRINT(printk("i2c: driver registered: %s\n",driver->name));

	/* Probe available busses */
	for (i = 0; i < I2C_BUS_MAX; i++)
		if (busses[i])
			i2c_attach_device(busses[i],driver);

	return 0;
}

int i2c_unregister_driver(struct i2c_driver *driver)
{
	int i;

	/* detach devices */
	for (i = 0; i < I2C_DEVICE_MAX; i++)
		if (driver->devices[i])
			i2c_detach_device(driver->devices[i]);

	for (i = 0; i < I2C_DRIVER_MAX; i++)
		if (driver == drivers[i])
			break;
	if (I2C_DRIVER_MAX == i) 
	{
		printk(KERN_WARNING "i2c: unregister_driver: driver not found: %s\n",
			driver->name);
		return -ENODEV;
	}

	MOD_DEC_USE_COUNT;
	
	drivers[i] = NULL;
	driver_count--;
	REGPRINT(printk("i2c: driver unregistered: %s\n",driver->name));

	return 0;
}

/* ----------------------------------------------------------------------- */

int i2c_control_device(struct i2c_bus *bus, int id,
		       unsigned int cmd, void *arg)
{
	int i;

	for (i = 0; i < I2C_DEVICE_MAX; i++)
		if (bus->devices[i] && bus->devices[i]->driver->id == id)
			break;
	if (i == I2C_DEVICE_MAX)
		return -ENODEV;
	if (NULL == bus->devices[i]->driver->command)
		return -ENODEV;
	return bus->devices[i]->driver->command(bus->devices[i],cmd,arg);
}

/* ----------------------------------------------------------------------- */

#define I2C_SET(bus,ctrl,data)  (bus->i2c_setlines(bus,ctrl,data))
#define I2C_GET(bus)            (bus->i2c_getdataline(bus))

void i2c_start(struct i2c_bus *bus)
{
	I2C_SET(bus,0,1);
	I2C_SET(bus,1,1);
	I2C_SET(bus,1,0);
	I2C_SET(bus,0,0);
	I2C_DEBUG(printk("%s: < ",bus->name));
}

void i2c_stop(struct i2c_bus *bus)
{
	I2C_SET(bus,0,0);
	I2C_SET(bus,1,0);
	I2C_SET(bus,1,1);
	I2C_DEBUG(printk(">\n"));
}

void i2c_one(struct i2c_bus *bus)
{
	I2C_SET(bus,0,1);
	I2C_SET(bus,1,1);
	I2C_SET(bus,0,1);
}

void i2c_zero(struct i2c_bus *bus)
{
	I2C_SET(bus,0,0);
	I2C_SET(bus,1,0);
	I2C_SET(bus,0,0);
}

int i2c_ack(struct i2c_bus *bus)
{
	int ack;
    
	I2C_SET(bus,0,1);
	I2C_SET(bus,1,1);
	ack = I2C_GET(bus);
	I2C_SET(bus,0,1);
	return ack;
}

int i2c_sendbyte(struct i2c_bus *bus,unsigned char data,int wait_for_ack)
{
	int i, ack;
    
	I2C_SET(bus,0,0);
	for (i=7; i>=0; i--)
		(data&(1<<i)) ? i2c_one(bus) : i2c_zero(bus);
	if (wait_for_ack)
		udelay(wait_for_ack);
	ack=i2c_ack(bus);
	I2C_DEBUG(printk("%02x%c ",(int)data,ack?'-':'+'));
	return ack;
}

unsigned char i2c_readbyte(struct i2c_bus *bus,int last)
{
	int i;
	unsigned char data=0;
    
	I2C_SET(bus,0,1);
	for (i=7; i>=0; i--) 
	{
		I2C_SET(bus,1,1);
		if (I2C_GET(bus))
			data |= (1<<i);
		I2C_SET(bus,0,1);
	}
	last ? i2c_one(bus) : i2c_zero(bus);
	I2C_DEBUG(printk("=%02x%c ",(int)data,last?'-':'+'));
	return data;
}

/* ----------------------------------------------------------------------- */

int i2c_read(struct i2c_bus *bus, unsigned char addr)
{
	int ret;
    
	if (bus->i2c_read)
		return bus->i2c_read(bus, addr);

	i2c_start(bus);
	i2c_sendbyte(bus,addr,0);
	ret = i2c_readbyte(bus,1);
	i2c_stop(bus);
	return ret;
}

int i2c_write(struct i2c_bus *bus, unsigned char addr,
	      unsigned char data1, unsigned char data2, int both)
{
	int ack;

	if (bus->i2c_write)
		return bus->i2c_write(bus, addr, data1, data2, both);

	i2c_start(bus);
	i2c_sendbyte(bus,addr,0);
	ack = i2c_sendbyte(bus,data1,0);
	if (both)
		ack = i2c_sendbyte(bus,data2,0);
	i2c_stop(bus);
	return ack ? -1 : 0 ;
}

/* ----------------------------------------------------------------------- */

#ifdef MODULE

#if LINUX_VERSION_CODE >= 0x020100
EXPORT_SYMBOL(i2c_register_bus);
EXPORT_SYMBOL(i2c_unregister_bus);
EXPORT_SYMBOL(i2c_register_driver);
EXPORT_SYMBOL(i2c_unregister_driver);
EXPORT_SYMBOL(i2c_control_device);
EXPORT_SYMBOL(i2c_start);
EXPORT_SYMBOL(i2c_stop);
EXPORT_SYMBOL(i2c_one);
EXPORT_SYMBOL(i2c_zero);
EXPORT_SYMBOL(i2c_ack);
EXPORT_SYMBOL(i2c_sendbyte);
EXPORT_SYMBOL(i2c_readbyte);
EXPORT_SYMBOL(i2c_read);
EXPORT_SYMBOL(i2c_write);
#endif

int init_module(void)
{
	return i2c_init();
}

void cleanup_module(void)
{
}
#endif
MODULE_LICENSE("GPL");
