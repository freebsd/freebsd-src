#ifndef _SIS_H
#define _SIS_H

#if 1
#define TWDEBUG(x)
#else
#define TWDEBUG(x) printk(KERN_INFO x "\n");
#endif

#endif
