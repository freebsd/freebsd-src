/* $Id$ */
/*	From: NetBSD: crtend.c,v 1.2 1997/10/10 08:45:30 mrg Exp */

#ifndef ECOFF_COMPAT

#include <sys/cdefs.h>

static void (*__CTOR_LIST__[1]) __P((void))
    __attribute__((__unused__))
    __attribute__((section(".ctors"))) = { (void *)0 };		/* XXX */
static void (*__DTOR_LIST__[1]) __P((void))
    __attribute__((__unused__))
    __attribute__((section(".dtors"))) = { (void *)0 };		/* XXX */

#endif /* !ECOFF_COMPAT */
