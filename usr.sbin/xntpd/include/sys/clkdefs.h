/* clkdefs.h,v 3.1 1993/07/06 01:07:12 jbj Exp
 * Defines for the "clk" timestamping STREAMS module
 */

#include <sys/ioccom.h>

/*
 * First, we need to define the maximum size of the set of
 * characters to timestamp. 32 is MORE than enough.
 */

#define CLK_MAXSTRSIZE 32

/*
 * ioctl(fd, CLK_SETSTR, (char*)c );
 *
 * will tell the driver that any char in the null-terminated
 * string c should be timestamped. It is possible, though
 * unlikely that this ioctl number could collide with an
 * existing one on your system. If so, change the 'K'
 * to some other letter. However, once you've compiled
 * the kernel with this include file, you should NOT
 * change this file.
 */

#if __STDC__
#define CLK_SETSTR _IOWN('K',01,CLK_MAXSTRSIZE)
#else
#define CLK_SETSTR _IOWN(K,01,CLK_MAXSTRSIZE)
#endif

