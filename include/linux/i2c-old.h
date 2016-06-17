#ifndef I2C_H
#define I2C_H

/*
 * linux i2c interface.  Works a little bit like the scsi subsystem.
 * There are:
 *
 *     i2c          the basic control module        (like scsi_mod)
 *     bus driver   a driver with a i2c bus         (hostadapter driver)
 *     chip driver  a driver for a chip connected
 *                  to a i2c bus                    (cdrom/hd driver)
 *
 * A device will be attached to one bus and one chip driver.  Every chip
 * driver gets a unique ID.
 *
 * A chip driver can provide a ioctl-like callback for the
 * communication with other parts of the kernel (not every i2c chip is
 * useful without other devices, a TV card tuner for example). 
 *
 * "i2c internal" parts of the structs: only the i2c module is allowed to
 * write to them, for others they are read-only.
 *
 */

#include <linux/version.h>

#define I2C_BUS_MAX       4    /* max # of bus drivers  */
#define I2C_DRIVER_MAX    8    /* max # of chip drivers */
#define I2C_DEVICE_MAX    8    /* max # if devices per bus/driver */

struct i2c_bus;
struct i2c_driver;
struct i2c_device;

#define I2C_DRIVERID_MSP3400    	 1
#define I2C_DRIVERID_TUNER      	 2
#define I2C_DRIVERID_VIDEOTEXT		 3
#define I2C_DRIVERID_VIDEODECODER	 4
#define I2C_DRIVERID_VIDEOENCODER	 5

#define I2C_BUSID_BT848		1	/* I2C bus on a BT848 */
#define I2C_BUSID_PARPORT	2	/* Bit banging on a parallel port */
#define I2C_BUSID_BUZ		3
#define I2C_BUSID_ZORAN		4
#define I2C_BUSID_CYBER2000	5

/*
 * struct for a driver for a i2c chip (tuner, soundprocessor,
 * videotext, ... ).
 *
 * a driver will register within the i2c module.  The i2c module will
 * callback the driver (i2c_attach) for every device it finds on a i2c
 * bus at the specified address.  If the driver decides to "accept"
 * the, device, it must return a struct i2c_device, and NULL
 * otherwise.
 *
 * i2c_detach = i2c_attach ** -1
 * 
 * i2c_command will be used to pass commands to the driver in a
 * ioctl-line manner.
 *
 */

struct i2c_driver 
{
    char           name[32];         /* some useful label         */
    int            id;               /* device type ID            */
    unsigned char  addr_l, addr_h;   /* address range of the chip */

    int (*attach)(struct i2c_device *device);
    int (*detach)(struct i2c_device *device);
    int (*command)(struct i2c_device *device,unsigned int cmd, void *arg);

    /* i2c internal */
    struct i2c_device   *devices[I2C_DEVICE_MAX];
    int                 devcount;
};


/*
 * this holds the informations about a i2c bus available in the system.
 * 
 * a chip with a i2c bus interface (like bt848) registers the bus within
 * the i2c module. This struct provides functions to access the i2c bus.
 * 
 * One must hold the spinlock to access the i2c bus (XXX: is the irqsave
 * required? Maybe better use a semaphore?). 
 * [-AC-] having a spinlock_irqsave is only needed if we have drivers wishing
 *	  to bang their i2c bus from an interrupt.
 * 
 * attach/detach_inform is a callback to inform the bus driver about
 * attached chip drivers.
 *
 */

/* needed: unsigned long flags */

#if LINUX_VERSION_CODE >= 0x020100
# if 0
#  define LOCK_FLAGS unsigned long flags;
#  define LOCK_I2C_BUS(bus)    spin_lock_irqsave(&(bus->bus_lock),flags);
#  define UNLOCK_I2C_BUS(bus)  spin_unlock_irqrestore(&(bus->bus_lock),flags);
# else
#  define LOCK_FLAGS
#  define LOCK_I2C_BUS(bus)    spin_lock(&(bus->bus_lock));
#  define UNLOCK_I2C_BUS(bus)  spin_unlock(&(bus->bus_lock));
# endif
#else
# define LOCK_FLAGS unsigned long flags;
# define LOCK_I2C_BUS(bus)    { save_flags(flags); cli(); }
# define UNLOCK_I2C_BUS(bus)  { restore_flags(flags);     }
#endif

struct i2c_bus 
{
	char  name[32];         /* some useful label */
	int   id;
	void  *data;            /* free for use by the bus driver */

#if LINUX_VERSION_CODE >= 0x020100
	spinlock_t bus_lock;
#endif

	/* attach/detach inform callbacks */
	void    (*attach_inform)(struct i2c_bus *bus, int id);
	void    (*detach_inform)(struct i2c_bus *bus, int id);

	/* Software I2C */
	void    (*i2c_setlines)(struct i2c_bus *bus, int ctrl, int data);
	int     (*i2c_getdataline)(struct i2c_bus *bus);

	/* Hardware I2C */
	int     (*i2c_read)(struct i2c_bus *bus, unsigned char addr);
	int     (*i2c_write)(struct i2c_bus *bus, unsigned char addr,
			 unsigned char b1, unsigned char b2, int both);

	/* internal data for i2c module */
	struct i2c_device   *devices[I2C_DEVICE_MAX];
	int                 devcount;
};


/*
 *	This holds per-device data for a i2c device
 */

struct i2c_device 
{
	char           name[32];         /* some useful label */
	void           *data;            /* free for use by the chip driver */
	unsigned char  addr;             /* chip addr */

	/* i2c internal */
	struct i2c_bus     *bus;
	struct i2c_driver  *driver;
};


/* ------------------------------------------------------------------- */
/* i2c module functions                                                */

/* register/unregister a i2c bus */
int i2c_register_bus(struct i2c_bus *bus);
int i2c_unregister_bus(struct i2c_bus *bus);

/* register/unregister a chip driver */
int i2c_register_driver(struct i2c_driver *driver);
int i2c_unregister_driver(struct i2c_driver *driver);

/* send a command to a chip using the ioctl-like callback interface */
int i2c_control_device(struct i2c_bus *bus, int id,
		       unsigned int cmd, void *arg);

/* i2c bus access functions */
void    i2c_start(struct i2c_bus *bus);
void    i2c_stop(struct i2c_bus *bus);
void    i2c_one(struct i2c_bus *bus);
void    i2c_zero(struct i2c_bus *bus);
int     i2c_ack(struct i2c_bus *bus);

int     i2c_sendbyte(struct i2c_bus *bus,unsigned char data,int wait_for_ack);
unsigned char i2c_readbyte(struct i2c_bus *bus,int last);

/* i2c (maybe) hardware functions */
int     i2c_read(struct i2c_bus *bus, unsigned char addr);
int     i2c_write(struct i2c_bus *bus, unsigned char addr,
		  unsigned char b1, unsigned char b2, int both);

int	i2c_init(void);
#endif /* I2C_H */
