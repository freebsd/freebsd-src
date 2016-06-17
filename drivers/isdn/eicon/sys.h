/*
 * Environment provided by system and miscellaneous definitions
 *
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.2  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#if !defined(SYS_H)
#define SYS_H

/* abreviations for unsigned types */
typedef int boolean_t;

typedef unsigned char byte;

typedef unsigned long dword;
typedef unsigned short word;

/* abreviations for volatile types */

typedef volatile byte	vbyte;
typedef volatile word	vword;
typedef volatile dword	vdword;

/* Booleans */

#if !defined(TRUE)
#define TRUE    (1)
#define FALSE   (0)
#endif

/* NULL pointer */

#if !defined(NULL)
#define NULL    ((void *) 0)
#endif

/* Return the dimension of an array */

#if !defined(DIM)
#define DIM(array)  (sizeof (array)/sizeof ((array)[0]))
#endif

/*
 * Return the number of milliseconds since last boot
 */

extern	dword	UxTimeGet(void);

extern	void 	DivasSprintf(char *buffer, char *format, ...);
extern	void 	DivasPrintf(char *format, ...);

/* fatal errors, asserts and tracing */

void HwFatalErrorFrom(char *file, int line);
void HwFatalError(void);
/* void HwAssert(char *file, int line, char *condition); */

#include <linux/kernel.h>
#include <linux/string.h>

#define _PRINTK printk

#define _PRINTF	DivasPrintf
void _PRINTF(char *format, ...);
#define PRINTF(arg_list)	_PRINTF arg_list
#if defined DTRACE
# define DPRINTF(arg_list)	_PRINTF arg_list
# define KDPRINTF(arg_list)	_PRINTF arg_list ; _PRINTK arg_list ; _PRINTK("\n");
#else
# define DPRINTF(arg_list)	(void)0
# define KDPRINTF(arg_list)	_PRINTK arg_list ; _PRINTK("\n");
#endif

#if !defined(ASSERT)
#if defined DEBUG || defined DBG
# define HwFatalError()	HwFatalErrorFrom(__FILE__, __LINE__)
# define ASSERT(cond)								\
		if (!(cond)) 								\
		{											\
/*			HwAssert(__FILE__, __LINE__, #cond);*/	\
		}
#else
# define ASSERT(cond)	((void)0)
#endif
#endif /* !defined(ASSERT) */

#define TRACE	(_PRINTF(__FILE__"@%d\n", __LINE__))

#endif /* SYS_H */
