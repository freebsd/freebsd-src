/*  ----------------------------------------------------------------------

    Copyright (C) 2000  Cesar Miquel  (miquel@df.uba.ar)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    ---------------------------------------------------------------------- */

/* modified for FreeBSD by Iwasa Kazmi <kzmi@ca2.so-net.ne.jp> */

/* $FreeBSD$ */

#ifdef __FreeBSD__
#include <sys/ioccom.h>
#ifndef USB_VENDOR_DIAMOND
#define USB_VENDOR_DIAMOND 0x841
#endif
#ifndef USB_PRODUCT_DIAMOND_RIO500USB
#define USB_PRODUCT_DIAMOND_RIO500USB 0x1
#endif
#endif

struct RioCommand
{
#ifdef __FreeBSD__
  u_int16_t  length;
#else
  short length;
#endif
  int   request;
  int   requesttype;
  int   value;
  int   index;
  void *buffer;
  int  timeout;
};

#ifdef __FreeBSD__
#define RIO_SEND_COMMAND	_IOWR('U', 200, struct RioCommand)
#define RIO_RECV_COMMAND	_IOWR('U', 201, struct RioCommand)
#else
#define RIO_SEND_COMMAND			0x1
#define RIO_RECV_COMMAND			0x2
#endif

#define RIO_DIR_OUT               	        0x0
#define RIO_DIR_IN				0x1


