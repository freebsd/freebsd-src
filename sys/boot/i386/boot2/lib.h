/*
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

/*
 * $FreeBSD: src/sys/boot/i386/boot2/lib.h,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

void sio_init(int);
void sio_flush(void);
void sio_putc(int);
int sio_getc(void);
int sio_ischar(void);
