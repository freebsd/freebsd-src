/* ------------------------------------------------------------------------- */
/* i2c-algo-bit.c i2c driver algorithms for bit-shift adapters		     */
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

/* With some changes from Kyösti Mälkki <kmalkki@cc.hut.fi> and even
   Frodo Looijaard <frodol@dds.nl> */

/* $Id: i2c-algo-bit.c,v 1.30 2001/07/29 02:44:25 mds Exp $ */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/sched.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

/* ----- global defines ----------------------------------------------- */
#define DEB(x) if (i2c_debug>=1) x;
#define DEB2(x) if (i2c_debug>=2) x;
#define DEBSTAT(x) if (i2c_debug>=3) x; /* print several statistical values*/
#define DEBPROTO(x) if (i2c_debug>=9) { x; }
 	/* debug the protocol by showing transferred bits */

/* debugging - slow down transfer to have a look at the data .. 	*/
/* I use this with two leds&resistors, each one connected to sda,scl 	*/
/* respectively. This makes sure that the algorithm works. Some chips   */
/* might not like this, as they have an internal timeout of some mils	*/
/*
#define SLO_IO      jif=jiffies;while(time_before_eq(jiffies, jif+i2c_table[minor].veryslow))\
                        if (need_resched) schedule();
*/


/* ----- global variables ---------------------------------------------	*/

#ifdef SLO_IO
	int jif;
#endif

/* module parameters:
 */
static int i2c_debug;
static int bit_test;	/* see if the line-setting functions work	*/
static int bit_scan;	/* have a look at what's hanging 'round		*/

/* --- setting states on the bus with the right timing: ---------------	*/

#define setsda(adap,val) adap->setsda(adap->data, val)
#define setscl(adap,val) adap->setscl(adap->data, val)
#define getsda(adap) adap->getsda(adap->data)
#define getscl(adap) adap->getscl(adap->data)

static inline void sdalo(struct i2c_algo_bit_data *adap)
{
	setsda(adap,0);
	udelay(adap->udelay);
}

static inline void sdahi(struct i2c_algo_bit_data *adap)
{
	setsda(adap,1);
	udelay(adap->udelay);
}

static inline void scllo(struct i2c_algo_bit_data *adap)
{
	setscl(adap,0);
	udelay(adap->udelay);
#ifdef SLO_IO
	SLO_IO
#endif
}

/*
 * Raise scl line, and do checking for delays. This is necessary for slower
 * devices.
 */
static inline int sclhi(struct i2c_algo_bit_data *adap)
{
	int start=jiffies;

	setscl(adap,1);

	udelay(adap->udelay);

	/* Not all adapters have scl sense line... */
	if (adap->getscl == NULL )
		return 0;

 	while (! getscl(adap) ) {	
 		/* the hw knows how to read the clock line,
 		 * so we wait until it actually gets high.
 		 * This is safer as some chips may hold it low
 		 * while they are processing data internally. 
 		 */
		setscl(adap,1);
		if (time_after_eq(jiffies, start+adap->timeout)) {
			return -ETIMEDOUT;
		}
		if (current->need_resched)
			schedule();
	}
	DEBSTAT(printk(KERN_DEBUG "needed %ld jiffies\n", jiffies-start));
#ifdef SLO_IO
	SLO_IO
#endif
	return 0;
} 


/* --- other auxiliary functions --------------------------------------	*/
static void i2c_start(struct i2c_algo_bit_data *adap) 
{
	/* assert: scl, sda are high */
	DEBPROTO(printk("S "));
	sdalo(adap);
	scllo(adap);
}

static void i2c_repstart(struct i2c_algo_bit_data *adap) 
{
	/* scl, sda may not be high */
	DEBPROTO(printk(" Sr "));
	setsda(adap,1);
	setscl(adap,1);
	udelay(adap->udelay);
	
	sdalo(adap);
	scllo(adap);
}


static void i2c_stop(struct i2c_algo_bit_data *adap) 
{
	DEBPROTO(printk("P\n"));
	/* assert: scl is low */
	sdalo(adap);
	sclhi(adap); 
	sdahi(adap);
}



/* send a byte without start cond., look for arbitration, 
   check ackn. from slave */
/* returns:
 * 1 if the device acknowledged
 * 0 if the device did not ack
 * -ETIMEDOUT if an error occurred (while raising the scl line)
 */
static int i2c_outb(struct i2c_adapter *i2c_adap, char c)
{
	int i;
	int sb;
	int ack;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	/* assert: scl is low */
	DEB2(printk(KERN_DEBUG " i2c_outb:%2.2X\n",c&0xff));
	for ( i=7 ; i>=0 ; i-- ) {
		sb = c & ( 1 << i );
		setsda(adap,sb);
		udelay(adap->udelay);
		DEBPROTO(printk(KERN_DEBUG "%d",sb!=0));
		if (sclhi(adap)<0) { /* timed out */
			sdahi(adap); /* we don't want to block the net */
			return -ETIMEDOUT;
		};
		/* do arbitration here: 
		 * if ( sb && ! getsda(adap) ) -> ouch! Get out of here.
		 */
		setscl(adap, 0 );
		udelay(adap->udelay);
	}
	sdahi(adap);
	if (sclhi(adap)<0){ /* timeout */
		return -ETIMEDOUT;
	};
	/* read ack: SDA should be pulled down by slave */
	ack=getsda(adap);	/* ack: sda is pulled low ->success.	 */
	DEB2(printk(KERN_DEBUG " i2c_outb: getsda() =  0x%2.2x\n", ~ack ));

	DEBPROTO( printk(KERN_DEBUG "[%2.2x]",c&0xff) );
	DEBPROTO(if (0==ack){ printk(KERN_DEBUG " A ");} else printk(KERN_DEBUG " NA ") );
	scllo(adap);
	return 0==ack;		/* return 1 if device acked	 */
	/* assert: scl is low (sda undef) */
}


static int i2c_inb(struct i2c_adapter *i2c_adap) 
{
	/* read byte via i2c port, without start/stop sequence	*/
	/* acknowledge is sent in i2c_read.			*/
	int i;
	unsigned char indata=0;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	/* assert: scl is low */
	DEB2(printk(KERN_DEBUG "i2c_inb.\n"));

	sdahi(adap);
	for (i=0;i<8;i++) {
		if (sclhi(adap)<0) { /* timeout */
			return -ETIMEDOUT;
		};
		indata *= 2;
		if ( getsda(adap) ) 
			indata |= 0x01;
		scllo(adap);
	}
	/* assert: scl is low */
	DEBPROTO(printk(KERN_DEBUG " 0x%02x", indata & 0xff));
	return (int) (indata & 0xff);
}

/*
 * Sanity check for the adapter hardware - check the reaction of
 * the bus lines only if it seems to be idle.
 */
static int test_bus(struct i2c_algo_bit_data *adap, char* name) {
	int scl,sda;
	sda=getsda(adap);
	if (adap->getscl==NULL) {
		printk("i2c-algo-bit.o: Warning: Adapter can't read from clock line - skipping test.\n");
		return 0;		
	}
	scl=getscl(adap);
	printk("i2c-algo-bit.o: Adapter: %s scl: %d  sda: %d -- testing...\n",
	       name,getscl(adap),getsda(adap));
	if (!scl || !sda ) {
		printk("i2c-algo-bit.o: %s seems to be busy.\n",name);
		goto bailout;
	}
	sdalo(adap);
	printk("i2c-algo-bit.o:1 scl: %d  sda: %d \n",getscl(adap),
	       getsda(adap));
	if ( 0 != getsda(adap) ) {
		printk("i2c-algo-bit.o: %s SDA stuck high!\n",name);
		sdahi(adap);
		goto bailout;
	}
	if ( 0 == getscl(adap) ) {
		printk("i2c-algo-bit.o: %s SCL unexpected low while pulling SDA low!\n",
			name);
		goto bailout;
	}		
	sdahi(adap);
	printk("i2c-algo-bit.o:2 scl: %d  sda: %d \n",getscl(adap),
	       getsda(adap));
	if ( 0 == getsda(adap) ) {
		printk("i2c-algo-bit.o: %s SDA stuck low!\n",name);
		sdahi(adap);
		goto bailout;
	}
	if ( 0 == getscl(adap) ) {
		printk("i2c-algo-bit.o: %s SCL unexpected low while SDA high!\n",
		       name);
	goto bailout;
	}
	scllo(adap);
	printk("i2c-algo-bit.o:3 scl: %d  sda: %d \n",getscl(adap),
	       getsda(adap));
	if ( 0 != getscl(adap) ) {
		printk("i2c-algo-bit.o: %s SCL stuck high!\n",name);
		sclhi(adap);
		goto bailout;
	}
	if ( 0 == getsda(adap) ) {
		printk("i2c-algo-bit.o: %s SDA unexpected low while pulling SCL low!\n",
			name);
		goto bailout;
	}
	sclhi(adap);
	printk("i2c-algo-bit.o:4 scl: %d  sda: %d \n",getscl(adap),
	       getsda(adap));
	if ( 0 == getscl(adap) ) {
		printk("i2c-algo-bit.o: %s SCL stuck low!\n",name);
		sclhi(adap);
		goto bailout;
	}
	if ( 0 == getsda(adap) ) {
		printk("i2c-algo-bit.o: %s SDA unexpected low while SCL high!\n",
			name);
		goto bailout;
	}
	printk("i2c-algo-bit.o: %s passed test.\n",name);
	return 0;
bailout:
	sdahi(adap);
	sclhi(adap);
	return -ENODEV;
}

/* ----- Utility functions
 */

/* try_address tries to contact a chip for a number of
 * times before it gives up.
 * return values:
 * 1 chip answered
 * 0 chip did not answer
 * -x transmission error
 */
static inline int try_address(struct i2c_adapter *i2c_adap,
		       unsigned char addr, int retries)
{
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;
	int i,ret = -1;
	for (i=0;i<=retries;i++) {
		ret = i2c_outb(i2c_adap,addr);
		if (ret==1)
			break;	/* success! */
		i2c_stop(adap);
		udelay(5/*adap->udelay*/);
		if (i==retries)  /* no success */
			break;
		i2c_start(adap);
		udelay(adap->udelay);
	}
	DEB2(if (i) printk(KERN_DEBUG "i2c-algo-bit.o: needed %d retries for %d\n",
	                   i,addr));
	return ret;
}

static int sendbytes(struct i2c_adapter *i2c_adap,const char *buf, int count)
{
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;
	char c;
	const char *temp = buf;
	int retval;
	int wrcount=0;

	while (count > 0) {
		c = *temp;
		DEB2(printk(KERN_DEBUG "i2c-algo-bit.o: %s sendbytes: writing %2.2X\n",
			    i2c_adap->name, c&0xff));
		retval = i2c_outb(i2c_adap,c);
		if (retval>0) {
			count--; 
			temp++;
			wrcount++;
		} else { /* arbitration or no acknowledge */
			printk(KERN_ERR "i2c-algo-bit.o: %s sendbytes: error - bailout.\n",
			       i2c_adap->name);
			i2c_stop(adap);
			return (retval<0)? retval : -EFAULT;
			        /* got a better one ?? */
		}
#if 0
		/* from asm/delay.h */
		__delay(adap->mdelay * (loops_per_sec / 1000) );
#endif
	}
	return wrcount;
}

static inline int readbytes(struct i2c_adapter *i2c_adap,char *buf,int count)
{
	char *temp = buf;
	int inval;
	int rdcount=0;   	/* counts bytes read */
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	while (count > 0) {
		inval = i2c_inb(i2c_adap);
/*printk("%#02x ",inval); if ( ! (count % 16) ) printk("\n"); */
		if (inval>=0) {
			*temp = inval;
			rdcount++;
		} else {   /* read timed out */
			printk(KERN_ERR "i2c-algo-bit.o: readbytes: i2c_inb timed out.\n");
			break;
		}

		if ( count > 1 ) {		/* send ack */
			sdalo(adap);
			DEBPROTO(printk(" Am "));
		} else {
			sdahi(adap);	/* neg. ack on last byte */
			DEBPROTO(printk(" NAm "));
		}
		if (sclhi(adap)<0) {	/* timeout */
			sdahi(adap);
			printk(KERN_ERR "i2c-algo-bit.o: readbytes: Timeout at ack\n");
			return -ETIMEDOUT;
		};
		scllo(adap);
		sdahi(adap);
		temp++;
		count--;
	}
	return rdcount;
}

/* doAddress initiates the transfer by generating the start condition (in
 * try_address) and transmits the address in the necessary format to handle
 * reads, writes as well as 10bit-addresses.
 * returns:
 *  0 everything went okay, the chip ack'ed
 * -x an error occurred (like: -EREMOTEIO if the device did not answer, or
 *	-ETIMEDOUT, for example if the lines are stuck...) 
 */
static inline int bit_doAddress(struct i2c_adapter *i2c_adap,
                                struct i2c_msg *msg, int retries) 
{
	unsigned short flags = msg->flags;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	unsigned char addr;
	int ret;
	if ( (flags & I2C_M_TEN)  ) { 
		/* a ten bit address */
		addr = 0xf0 | (( msg->addr >> 7) & 0x03);
		DEB2(printk(KERN_DEBUG "addr0: %d\n",addr));
		/* try extended address code...*/
		ret = try_address(i2c_adap, addr, retries);
		if (ret!=1) {
			printk(KERN_ERR "died at extended address code.\n");
			return -EREMOTEIO;
		}
		/* the remaining 8 bit address */
		ret = i2c_outb(i2c_adap,msg->addr & 0x7f);
		if (ret != 1) {
			/* the chip did not ack / xmission error occurred */
			printk(KERN_ERR "died at 2nd address code.\n");
			return -EREMOTEIO;
		}
		if ( flags & I2C_M_RD ) {
			i2c_repstart(adap);
			/* okay, now switch into reading mode */
			addr |= 0x01;
			ret = try_address(i2c_adap, addr, retries);
			if (ret!=1) {
				printk(KERN_ERR "died at extended address code.\n");
				return -EREMOTEIO;
			}
		}
	} else {		/* normal 7bit address	*/
		addr = ( msg->addr << 1 );
		if (flags & I2C_M_RD )
			addr |= 1;
		if (flags & I2C_M_REV_DIR_ADDR )
			addr ^= 1;
		ret = try_address(i2c_adap, addr, retries);
		if (ret!=1) {
			return -EREMOTEIO;
		}
	}
	return 0;
}

static int bit_xfer(struct i2c_adapter *i2c_adap,
		    struct i2c_msg msgs[], int num)
{
	struct i2c_msg *pmsg;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;
	
	int i,ret;

	i2c_start(adap);
	for (i=0;i<num;i++) {
		pmsg = &msgs[i];
		if (!(pmsg->flags & I2C_M_NOSTART)) {
			if (i) {
				i2c_repstart(adap);
			}
			ret = bit_doAddress(i2c_adap,pmsg,i2c_adap->retries);
			if (ret != 0) {
				DEB2(printk(KERN_DEBUG "i2c-algo-bit.o: NAK from device addr %2.2x msg #%d\n",
					msgs[i].addr,i));
				return (ret<0) ? ret : -EREMOTEIO;
			}
		}
		if (pmsg->flags & I2C_M_RD ) {
			/* read bytes into buffer*/
			ret = readbytes(i2c_adap,pmsg->buf,pmsg->len);
			DEB2(printk(KERN_DEBUG "i2c-algo-bit.o: read %d bytes.\n",ret));
			if (ret < pmsg->len ) {
				return (ret<0)? ret : -EREMOTEIO;
			}
		} else {
			/* write bytes from buffer */
			ret = sendbytes(i2c_adap,pmsg->buf,pmsg->len);
			DEB2(printk(KERN_DEBUG "i2c-algo-bit.o: wrote %d bytes.\n",ret));
			if (ret < pmsg->len ) {
				return (ret<0) ? ret : -EREMOTEIO;
			}
		}
	}
	i2c_stop(adap);
	return num;
}

static int algo_control(struct i2c_adapter *adapter, 
	unsigned int cmd, unsigned long arg)
{
	return 0;
}

static u32 bit_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR | 
	       I2C_FUNC_PROTOCOL_MANGLING;
}


/* -----exported algorithm data: -------------------------------------	*/

static struct i2c_algorithm i2c_bit_algo = {
	"Bit-shift algorithm",
	I2C_ALGO_BIT,
	bit_xfer,
	NULL,
	NULL,				/* slave_xmit		*/
	NULL,				/* slave_recv		*/
	algo_control,			/* ioctl		*/
	bit_func,			/* functionality	*/
};

/* 
 * registering functions to load algorithms at runtime 
 */
int i2c_bit_add_bus(struct i2c_adapter *adap)
{
	int i;
	struct i2c_algo_bit_data *bit_adap = adap->algo_data;

	if (bit_test) {
		int ret = test_bus(bit_adap, adap->name);
		if (ret<0)
			return -ENODEV;
	}

	DEB2(printk(KERN_DEBUG "i2c-algo-bit.o: hw routines for %s registered.\n",
	            adap->name));

	/* register new adapter to i2c module... */

	adap->id |= i2c_bit_algo.id;
	adap->algo = &i2c_bit_algo;

	adap->timeout = 100;	/* default values, should	*/
	adap->retries = 3;	/* be replaced by defines	*/

	/* scan bus */
	if (bit_scan) {
		int ack;
		printk(KERN_INFO " i2c-algo-bit.o: scanning bus %s.\n",
		       adap->name);
		for (i = 0x00; i < 0xff; i+=2) {
			i2c_start(bit_adap);
			ack = i2c_outb(adap,i);
			i2c_stop(bit_adap);
			if (ack>0) {
				printk("(%02x)",i>>1); 
			} else 
				printk("."); 
		}
		printk("\n");
	}

#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
	i2c_add_adapter(adap);

	return 0;
}


int i2c_bit_del_bus(struct i2c_adapter *adap)
{
	int res;

	if ((res = i2c_del_adapter(adap)) < 0)
		return res;

	DEB2(printk("i2c-algo-bit.o: adapter unregistered: %s\n",adap->name));

#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}

int __init i2c_algo_bit_init (void)
{
	printk(KERN_INFO "i2c-algo-bit.o: i2c bit algorithm module\n");
	return 0;
}



EXPORT_SYMBOL(i2c_bit_add_bus);
EXPORT_SYMBOL(i2c_bit_del_bus);

#ifdef MODULE
MODULE_AUTHOR("Simon G. Vogl <simon@tk.uni-linz.ac.at>");
MODULE_DESCRIPTION("I2C-Bus bit-banging algorithm");
MODULE_LICENSE("GPL");

MODULE_PARM(bit_test, "i");
MODULE_PARM(bit_scan, "i");
MODULE_PARM(i2c_debug,"i");

MODULE_PARM_DESC(bit_test, "Test the lines of the bus to see if it is stuck");
MODULE_PARM_DESC(bit_scan, "Scan for active chips on the bus");
MODULE_PARM_DESC(i2c_debug,
            "debug level - 0 off; 1 normal; 2,3 more verbose; 9 bit-protocol");

int init_module(void) 
{
	return i2c_algo_bit_init();
}

void cleanup_module(void) 
{
}
#endif
