/* $FreeBSD$ */

#ifndef _MACHINE_CONSOLE_H_
#define _MACHINE_CONSOLE_H_

#if __GNUC__
#warning "this file includes <machine/console.h> which is deprecated, use <sys/{kb,cons,fb}io.h> instead"
#endif

#include <sys/fbio.h>
#include <sys/kbio.h>
#include <sys/consio.h>

#endif /* !_MACHINE_CONSOLE_H_ */
