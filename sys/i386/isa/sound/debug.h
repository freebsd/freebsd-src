/* Kernel driver debugging stuff - cmetz@thor.tjhsst.edu */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#if defined (DEBUG_ME) && !defined(HALT_DEBUGGING)

#define DEB(X) 			printk("%s %s: ", __FILE__, __FUNCTION__); X
#define DEB1(X) 		printk("%s %s: ", __FILE__, __FUNCTION__); X
#define RETURN_HEX(X, Y)	{ Y _foo; _foo = X; printk("%s %s: 0x%x\n", __FILE__, __FUNCTION__, ((int)_foo)); return(_foo); }
#define RETURN_DEC(X, Y)	{ Y _foo; _foo = X; printk("%s %s: %d\n", __FILE__, __FUNCTION__, ((int)_foo)); return(_foo); }
#define RETURN_PTR(X, Y)	{ Y _foo; _foo = X; printk("%s %s: 0x%08x\n", __FILE__, __FUNCTION__, ((void *)_foo)); return(_foo); }
#define RETURN_ERR(X)		{ int _foo; _foo = X; printk("%s %s: ", __FILE__, __FUNCTION__); switch(_foo) { case 0: printk("No error"); break; case -ENODEV: printk("ENODEV"); break; case -EBUSY: printk("EBUSY"); break; default: printk("Error %d", _foo); } printk(".\n"); return(_foo); }
#define DEB_OUTB		OUTB
#define DEB_INB			INB

#else

#define DEB(X)		
#define DEB1(X)		
#define RETURN_HEX(X, Y)	return(X)
#define RETURN_DEC(X, Y)	return(X)
#define RETURN_PTR(X, Y)	return(X)
#define RETURN_ERR(X)		return(X)
#define DEB_OUTB		OUTB
#define DEB_INB			INB

#endif
#endif
