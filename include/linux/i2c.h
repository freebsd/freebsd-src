/* ------------------------------------------------------------------------- */
/* 									     */
/* i2c.h - definitions for the i2c-bus interface			     */
/* 									     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-2000 Simon G. Vogl

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */

/* With some changes from Kyösti Mälkki <kmalkki@cc.hut.fi> and
   Frodo Looijaard <frodol@dds.nl> */

/* $Id: i2c.h,v 1.46 2001/08/31 00:04:07 phil Exp $ */

#ifndef I2C_H
#define I2C_H

#define I2C_DATE "20010830"
#define I2C_VERSION "2.6.1"

#include <linux/i2c-id.h>	/* id values of adapters et. al. 	*/
#include <linux/types.h>


struct i2c_msg;


#ifdef __KERNEL__

/* --- Includes and compatibility declarations ------------------------ */

#include <asm/semaphore.h>
#include <linux/config.h>

/* --- General options ------------------------------------------------	*/

#define I2C_ALGO_MAX	4		/* control memory consumption	*/
#define I2C_ADAP_MAX	16
#define I2C_DRIVER_MAX	16
#define I2C_CLIENT_MAX	32
#define I2C_DUMMY_MAX 4

struct i2c_algorithm;
struct i2c_adapter;
struct i2c_client;
struct i2c_driver;
struct i2c_client_address_data;
union i2c_smbus_data;


/*
 * The master routines are the ones normally used to transmit data to devices
 * on a bus (or read from them). Apart from two basic transfer functions to 
 * transmit one message at a time, a more complex version can be used to 
 * transmit an arbitrary number of messages without interruption.
 */
extern int i2c_master_send(struct i2c_client *,const char* ,int);
extern int i2c_master_recv(struct i2c_client *,char* ,int);

/* Transfer num messages.
 */
extern int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg msg[],int num);

/*
 * Some adapter types (i.e. PCF 8584 based ones) may support slave behaviuor. 
 * This is not tested/implemented yet and will change in the future.
 */
extern int i2c_slave_send(struct i2c_client *,char*,int);
extern int i2c_slave_recv(struct i2c_client *,char*,int);



/* This is the very generalized SMBus access routine. You probably do not
   want to use this, though; one of the functions below may be much easier,
   and probably just as fast. 
   Note that we use i2c_adapter here, because you do not need a specific
   smbus adapter to call this function. */
extern s32 i2c_smbus_xfer (struct i2c_adapter * adapter, u16 addr, 
                           unsigned short flags,
                           char read_write, u8 command, int size,
                           union i2c_smbus_data * data);

/* Now follow the 'nice' access routines. These also document the calling
   conventions of smbus_access. */

extern s32 i2c_smbus_write_quick(struct i2c_client * client, u8 value);
extern s32 i2c_smbus_read_byte(struct i2c_client * client);
extern s32 i2c_smbus_write_byte(struct i2c_client * client, u8 value);
extern s32 i2c_smbus_read_byte_data(struct i2c_client * client, u8 command);
extern s32 i2c_smbus_write_byte_data(struct i2c_client * client,
                                     u8 command, u8 value);
extern s32 i2c_smbus_read_word_data(struct i2c_client * client, u8 command);
extern s32 i2c_smbus_write_word_data(struct i2c_client * client,
                                     u8 command, u16 value);
extern s32 i2c_smbus_process_call(struct i2c_client * client,
                                  u8 command, u16 value);
/* Returns the number of read bytes */
extern s32 i2c_smbus_read_block_data(struct i2c_client * client,
                                     u8 command, u8 *values);
extern s32 i2c_smbus_write_block_data(struct i2c_client * client,
                                      u8 command, u8 length,
                                      u8 *values);
extern s32 i2c_smbus_write_i2c_block_data(struct i2c_client * client,
                                          u8 command, u8 length,
                                          u8 *values);


/*
 * A driver is capable of handling one or more physical devices present on
 * I2C adapters. This information is used to inform the driver of adapter
 * events.
 */

struct i2c_driver {
	char name[32];
	int id;
	unsigned int flags;		/* div., see below		*/

	/* Notifies the driver that a new bus has appeared. This routine
	 * can be used by the driver to test if the bus meets its conditions
	 * & seek for the presence of the chip(s) it supports. If found, it 
	 * registers the client(s) that are on the bus to the i2c admin. via
	 * i2c_attach_client.
	 */
	int (*attach_adapter)(struct i2c_adapter *);

	/* tells the driver that a client is about to be deleted & gives it 
	 * the chance to remove its private data. Also, if the client struct
	 * has been dynamically allocated by the driver in the function above,
	 * it must be freed here.
	 */
	int (*detach_client)(struct i2c_client *);
	
	/* a ioctl like command that can be used to perform specific functions
	 * with the device.
	 */
	int (*command)(struct i2c_client *client,unsigned int cmd, void *arg);
	
	/* These two are mainly used for bookkeeping & dynamic unloading of 
	 * kernel modules. inc_use tells the driver that a client is being  
	 * used by another module & that it should increase its ref. counter.
	 * dec_use is the inverse operation.
	 * NB: Make sure you have no circular dependencies, or else you get a 
	 * deadlock when trying to unload the modules.
	* You should use the i2c_{inc,dec}_use_client functions instead of
	* calling this function directly.
	 */
	void (*inc_use)(struct i2c_client *client);
	void (*dec_use)(struct i2c_client *client);
};

/*
 * i2c_client identifies a single device (i.e. chip) that is connected to an 
 * i2c bus. The behaviour is defined by the routines of the driver. This
 * function is mainly used for lookup & other admin. functions.
 */
struct i2c_client {
	char name[32];
	int id;
	unsigned int flags;		/* div., see below		*/
	unsigned int addr;		/* chip address - NOTE: 7bit 	*/
					/* addresses are stored in the	*/
					/* _LOWER_ 7 bits of this char	*/
	/* addr: unsigned int to make lm_sensors i2c-isa adapter work
	  more cleanly. It does not take any more memory space, due to
	  alignment considerations */
	struct i2c_adapter *adapter;	/* the adapter we sit on	*/
	struct i2c_driver *driver;	/* and our access routines	*/
	void *data;			/* for the clients		*/
	int usage_count;		/* How many accesses currently  */
					/* to the client		*/
};


/*
 * The following structs are for those who like to implement new bus drivers:
 * i2c_algorithm is the interface to a class of hardware solutions which can
 * be addressed using the same bus algorithms - i.e. bit-banging or the PCF8584
 * to name two of the most common.
 */
struct i2c_algorithm {
	char name[32];				/* textual description 	*/
	unsigned int id;

	/* If an adapter algorithm can't to I2C-level access, set master_xfer
	   to NULL. If an adapter algorithm can do SMBus access, set 
	   smbus_xfer. If set to NULL, the SMBus protocol is simulated
	   using common I2C messages */
	int (*master_xfer)(struct i2c_adapter *adap,struct i2c_msg msgs[], 
	                   int num);
	int (*smbus_xfer) (struct i2c_adapter *adap, u16 addr, 
	                   unsigned short flags, char read_write,
	                   u8 command, int size, union i2c_smbus_data * data);

	/* --- these optional/future use for some adapter types.*/
	int (*slave_send)(struct i2c_adapter *,char*,int);
	int (*slave_recv)(struct i2c_adapter *,char*,int);

	/* --- ioctl like call to set div. parameters. */
	int (*algo_control)(struct i2c_adapter *, unsigned int, unsigned long);

	/* To determine what the adapter supports */
	u32 (*functionality) (struct i2c_adapter *);
};

/*
 * i2c_adapter is the structure used to identify a physical i2c bus along
 * with the access algorithms necessary to access it.
 */
struct i2c_adapter {
	char name[32];	/* some useful name to identify the adapter	*/
	unsigned int id;/* == is algo->id | hwdep.struct->id, 		*/
			/* for registered values see below		*/
	struct i2c_algorithm *algo;/* the algorithm to access the bus	*/
	void *algo_data;

	/* --- These may be NULL, but should increase the module use count */
	void (*inc_use)(struct i2c_adapter *);
	void (*dec_use)(struct i2c_adapter *);

	/* --- administration stuff. */
	int (*client_register)(struct i2c_client *);
	int (*client_unregister)(struct i2c_client *);

	void *data;	/* private data for the adapter			*/
			/* some data fields that are used by all types	*/
			/* these data fields are readonly to the public	*/
			/* and can be set via the i2c_ioctl call	*/

			/* data fields that are valid for all devices	*/
	struct semaphore lock;  
	unsigned int flags;/* flags specifying div. data		*/

	struct i2c_client *clients[I2C_CLIENT_MAX];
	int client_count;

	int timeout;
	int retries;

#ifdef CONFIG_PROC_FS 
	/* No need to set this when you initialize the adapter          */
	int inode;
#endif /* def CONFIG_PROC_FS */
};

/*flags for the driver struct: */
#define I2C_DF_NOTIFY	0x01		/* notify on bus (de/a)ttaches 	*/
#define I2C_DF_DUMMY	0x02		/* do not connect any clients */

/*flags for the client struct: */
#define I2C_CLIENT_ALLOW_USE		0x01	/* Client allows access */
#define I2C_CLIENT_ALLOW_MULTIPLE_USE 	0x02  	/* Allow multiple access-locks */
						/* on an i2c_client */

/* i2c_client_address_data is the struct for holding default client
 * addresses for a driver and for the parameters supplied on the
 * command line
 */
struct i2c_client_address_data {
	unsigned short *normal_i2c;
	unsigned short *normal_i2c_range;
	unsigned short *probe;
	unsigned short *probe_range;
	unsigned short *ignore;
	unsigned short *ignore_range;
	unsigned short *force;
};

/* Internal numbers to terminate lists */
#define I2C_CLIENT_END 0xfffe

/* The numbers to use to set I2C bus address */
#define ANY_I2C_BUS 0xffff

/* The length of the option lists */
#define I2C_CLIENT_MAX_OPTS 48


/* ----- functions exported by i2c.o */

/* administration...
 */
extern int i2c_add_adapter(struct i2c_adapter *);
extern int i2c_del_adapter(struct i2c_adapter *);

extern int i2c_add_driver(struct i2c_driver *);
extern int i2c_del_driver(struct i2c_driver *);

extern int i2c_attach_client(struct i2c_client *);
extern int i2c_detach_client(struct i2c_client *);

/* Only call these if you grab a resource that makes unloading the
   client and the adapter it is on completely impossible. Like when a
   /proc directory is entered. */
extern void i2c_inc_use_client(struct i2c_client *);
extern void i2c_dec_use_client(struct i2c_client *);

/* New function: This is to get an i2c_client-struct for controlling the 
   client either by using i2c_control-function or having the 
   client-module export functions that can be used with the i2c_client
   -struct. */
extern struct i2c_client *i2c_get_client(int driver_id, int adapter_id, 
					struct i2c_client *prev);

/* Should be used with new function
   extern struct i2c_client *i2c_get_client(int,int,struct i2c_client *);
   to make sure that client-struct is valid and that it is okay to access
   the i2c-client. 
   returns -EACCES if client doesn't allow use (default)
   returns -EBUSY if client doesn't allow multiple use (default) and 
   usage_count >0 */
extern int i2c_use_client(struct i2c_client *);
extern int i2c_release_client(struct i2c_client *);

/* returns -EBUSY if address has been taken, 0 if not. Note that the only
   other place at which this is called is within i2c_attach_client; so
   you can cheat by simply not registering. Not recommended, of course! */
extern int i2c_check_addr (struct i2c_adapter *adapter, int addr);

/* Detect function. It iterates over all possible addresses itself.
 * It will only call found_proc if some client is connected at the
 * specific address (unless a 'force' matched);
 */
typedef int i2c_client_found_addr_proc (struct i2c_adapter *adapter,
                                     int addr, unsigned short flags,int kind);

extern int i2c_probe(struct i2c_adapter *adapter, 
		struct i2c_client_address_data *address_data,
		i2c_client_found_addr_proc *found_proc);

/* An ioctl like call to set div. parameters of the adapter.
 */
extern int i2c_control(struct i2c_client *,unsigned int, unsigned long);

/* This call returns a unique low identifier for each registered adapter,
 * or -1 if the adapter was not registered. 
 */
extern int i2c_adapter_id(struct i2c_adapter *adap);



/* Return the functionality mask */
extern u32 i2c_get_functionality (struct i2c_adapter *adap);

/* Return 1 if adapter supports everything we need, 0 if not. */
extern int i2c_check_functionality (struct i2c_adapter *adap, u32 func);

#endif /* __KERNEL__ */

/*
 * I2C Message - used for pure i2c transaction, also from /dev interface
 */
struct i2c_msg {
	__u16 addr;	/* slave address			*/
	__u16 flags;		
#define I2C_M_TEN	0x10	/* we have a ten bit chip address	*/
#define I2C_M_RD	0x01
#define I2C_M_NOSTART	0x4000
#define I2C_M_REV_DIR_ADDR	0x2000
	__u16 len;		/* msg length				*/
	__u8 *buf;		/* pointer to msg data			*/
};

/* To determine what functionality is present */

#define I2C_FUNC_I2C			0x00000001
#define I2C_FUNC_10BIT_ADDR		0x00000002
#define I2C_FUNC_PROTOCOL_MANGLING	0x00000004 /* I2C_M_{REV_DIR_ADDR,NOSTART} */
#define I2C_FUNC_SMBUS_QUICK		0x00010000 
#define I2C_FUNC_SMBUS_READ_BYTE	0x00020000 
#define I2C_FUNC_SMBUS_WRITE_BYTE	0x00040000 
#define I2C_FUNC_SMBUS_READ_BYTE_DATA	0x00080000 
#define I2C_FUNC_SMBUS_WRITE_BYTE_DATA	0x00100000 
#define I2C_FUNC_SMBUS_READ_WORD_DATA	0x00200000 
#define I2C_FUNC_SMBUS_WRITE_WORD_DATA	0x00400000 
#define I2C_FUNC_SMBUS_PROC_CALL	0x00800000 
#define I2C_FUNC_SMBUS_READ_BLOCK_DATA	0x01000000 
#define I2C_FUNC_SMBUS_WRITE_BLOCK_DATA 0x02000000 
#define I2C_FUNC_SMBUS_READ_I2C_BLOCK	0x04000000 /* New I2C-like block */
#define I2C_FUNC_SMBUS_WRITE_I2C_BLOCK	0x08000000 /* transfer */

#define I2C_FUNC_SMBUS_BYTE I2C_FUNC_SMBUS_READ_BYTE | \
                            I2C_FUNC_SMBUS_WRITE_BYTE
#define I2C_FUNC_SMBUS_BYTE_DATA I2C_FUNC_SMBUS_READ_BYTE_DATA | \
                                 I2C_FUNC_SMBUS_WRITE_BYTE_DATA
#define I2C_FUNC_SMBUS_WORD_DATA I2C_FUNC_SMBUS_READ_WORD_DATA | \
                                 I2C_FUNC_SMBUS_WRITE_WORD_DATA
#define I2C_FUNC_SMBUS_BLOCK_DATA I2C_FUNC_SMBUS_READ_BLOCK_DATA | \
                                  I2C_FUNC_SMBUS_WRITE_BLOCK_DATA
#define I2C_FUNC_SMBUS_I2C_BLOCK I2C_FUNC_SMBUS_READ_I2C_BLOCK | \
                                  I2C_FUNC_SMBUS_WRITE_I2C_BLOCK

#define I2C_FUNC_SMBUS_EMUL I2C_FUNC_SMBUS_QUICK | \
                            I2C_FUNC_SMBUS_BYTE | \
                            I2C_FUNC_SMBUS_BYTE_DATA | \
                            I2C_FUNC_SMBUS_WORD_DATA | \
                            I2C_FUNC_SMBUS_PROC_CALL | \
                            I2C_FUNC_SMBUS_WRITE_BLOCK_DATA

/* 
 * Data for SMBus Messages 
 */
#define I2C_SMBUS_BLOCK_MAX	32	/* As specified in SMBus standard */	
#define I2C_SMBUS_I2C_BLOCK_MAX	32	/* Not specified but we use same structure */
union i2c_smbus_data {
	__u8 byte;
	__u16 word;
	__u8 block[I2C_SMBUS_BLOCK_MAX + 2]; /* block[0] is used for length */
	                  /* one more for read length in block process call */
};

/* smbus_access read or write markers */
#define I2C_SMBUS_READ	1
#define I2C_SMBUS_WRITE	0

/* SMBus transaction types (size parameter in the above functions) 
   Note: these no longer correspond to the (arbitrary) PIIX4 internal codes! */
#define I2C_SMBUS_QUICK		    0
#define I2C_SMBUS_BYTE		    1
#define I2C_SMBUS_BYTE_DATA	    2 
#define I2C_SMBUS_WORD_DATA	    3
#define I2C_SMBUS_PROC_CALL	    4
#define I2C_SMBUS_BLOCK_DATA	    5
#define I2C_SMBUS_I2C_BLOCK_DATA    6


/* ----- commands for the ioctl like i2c_command call:
 * note that additional calls are defined in the algorithm and hw 
 *	dependent layers - these can be listed here, or see the 
 *	corresponding header files.
 */
				/* -> bit-adapter specific ioctls	*/
#define I2C_RETRIES	0x0701	/* number of times a device address      */
				/* should be polled when not            */
                                /* acknowledging 			*/
#define I2C_TIMEOUT	0x0702	/* set timeout - call with int 		*/


/* this is for i2c-dev.c	*/
#define I2C_SLAVE	0x0703	/* Change slave address			*/
				/* Attn.: Slave address is 7 or 10 bits */
#define I2C_SLAVE_FORCE	0x0706	/* Change slave address			*/
				/* Attn.: Slave address is 7 or 10 bits */
				/* This changes the address, even if it */
				/* is already taken!			*/
#define I2C_TENBIT	0x0704	/* 0 for 7 bit addrs, != 0 for 10 bit	*/

#define I2C_FUNCS	0x0705	/* Get the adapter functionality */
#define I2C_RDWR	0x0707	/* Combined R/W transfer (one stop only)*/
#if 0
#define I2C_ACK_TEST	0x0710	/* See if a slave is at a specific address */
#endif

#define I2C_SMBUS	0x0720	/* SMBus-level access */

/* ... algo-bit.c recognizes */
#define I2C_UDELAY	0x0705	/* set delay in microsecs between each	*/
				/* written byte (except address)	*/
#define I2C_MDELAY	0x0706	/* millisec delay between written bytes */

/* ----- I2C-DEV: char device interface stuff ------------------------- */

#define I2C_MAJOR	89		/* Device major number		*/

#ifdef __KERNEL__

#  ifndef NULL
#    define NULL ( (void *) 0 )
#  endif

#  ifndef ENODEV
#    include <asm/errno.h>
#  endif

/* These defines are used for probing i2c client addresses */
/* Default fill of many variables */
#define I2C_CLIENT_DEFAULTS {I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END, \
                          I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END}

/* This is ugly. We need to evaluate I2C_CLIENT_MAX_OPTS before it is 
   stringified */
#define I2C_CLIENT_MODPARM_AUX1(x) "1-" #x "h"
#define I2C_CLIENT_MODPARM_AUX(x) I2C_CLIENT_MODPARM_AUX1(x)
#define I2C_CLIENT_MODPARM I2C_CLIENT_MODPARM_AUX(I2C_CLIENT_MAX_OPTS)

/* I2C_CLIENT_MODULE_PARM creates a module parameter, and puts it in the
   module header */

#define I2C_CLIENT_MODULE_PARM(var,desc) \
  static unsigned short var[I2C_CLIENT_MAX_OPTS] = I2C_CLIENT_DEFAULTS; \
  MODULE_PARM(var,I2C_CLIENT_MODPARM); \
  MODULE_PARM_DESC(var,desc)

/* This is the one you want to use in your own modules */
#define I2C_CLIENT_INSMOD \
  I2C_CLIENT_MODULE_PARM(probe, \
                      "List of adapter,address pairs to scan additionally"); \
  I2C_CLIENT_MODULE_PARM(probe_range, \
                      "List of adapter,start-addr,end-addr triples to scan " \
                      "additionally"); \
  I2C_CLIENT_MODULE_PARM(ignore, \
                      "List of adapter,address pairs not to scan"); \
  I2C_CLIENT_MODULE_PARM(ignore_range, \
                      "List of adapter,start-addr,end-addr triples not to " \
                      "scan"); \
  I2C_CLIENT_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  static struct i2c_client_address_data addr_data = \
                                       {normal_i2c, normal_i2c_range, \
                                        probe, probe_range, \
                                        ignore, ignore_range, \
                                        force}

/* Detect whether we are on the isa bus. If this returns true, all i2c
   access will fail! */
#define i2c_is_isa_client(clientptr) \
        ((clientptr)->adapter->algo->id == I2C_ALGO_ISA)
#define i2c_is_isa_adapter(adapptr) \
        ((adapptr)->algo->id == I2C_ALGO_ISA)

#endif /* def __KERNEL__ */
#endif /* I2C_H */
