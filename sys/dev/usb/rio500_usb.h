/*-
    ----------------------------------------------------------------------

    Copyright (C) 2000  Cesar Miquel  (miquel@df.uba.ar)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted under any licence of your choise which
    meets the open source licence definiton
    http://www.opensource.org/opd.html such as the GNU licence or the
    BSD licence.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License or the BSD license for more details.

    ----------------------------------------------------------------------

    Modified for FreeBSD by Iwasa Kazmi <kzmi@ca2.so-net.ne.jp>

    ---------------------------------------------------------------------- */

/*  $FreeBSD$ */

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
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
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
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

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define RIO_SEND_COMMAND	_IOWR('U', 200, struct RioCommand)
#define RIO_RECV_COMMAND	_IOWR('U', 201, struct RioCommand)
#else
#define RIO_SEND_COMMAND			0x1
#define RIO_RECV_COMMAND			0x2
#endif

#define RIO_DIR_OUT               	        0x0
#define RIO_DIR_IN				0x1
