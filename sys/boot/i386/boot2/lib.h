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
 * $FreeBSD: src/sys/boot/i386/boot2/lib.h,v 1.1.2.1 1999/08/29 16:20:46 peter Exp $
 */

void sio_init(void);
void sio_flush(void);
void sio_putc(int);
int sio_getc(void);
int sio_ischar(void);
