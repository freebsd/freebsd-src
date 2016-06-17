/*
 *  include/asm-s390/init.h
 *
 *  S390 version
 */

#ifndef _S390_INIT_H
#define _S390_INIT_H

#define __init __attribute__ ((constructor))

/* don't know, if need on S390 */
#define __initdata
#define __initfunc(__arginit) \
        __arginit __init; \
        __arginit
/* For assembly routines
 * need to define ?
 */
/*
#define __INIT          .section        ".text.init",#alloc,#execinstr
#define __FINIT .previous
#define __INITDATA      .section        ".data.init",#alloc,#write
*/

#define __cacheline_aligned __attribute__ ((__aligned__(256)))

#endif

