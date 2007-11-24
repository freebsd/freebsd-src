/*
 * Defines for the "clk" timestamping STREAMS module
 */

#if defined(sun)
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif

/*
 * First, we need to define the maximum size of the set of
 * characters to timestamp. 32 is MORE than enough.
 */

#define CLK_MAXSTRSIZE 32
struct clk_tstamp_charset {		/* XXX to use _IOW not _IOWN */
	char	val[CLK_MAXSTRSIZE];
};

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

#if defined(__STDC__)			/* XXX avoid __STDC__=0 on SOLARIS */
#define CLK_SETSTR _IOW('K', 01, struct clk_tstamp_charset)
#else
#define CLK_SETSTR _IOW(K, 01, struct clk_tstamp_charset)
#endif

