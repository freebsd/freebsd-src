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

#include <sys/ioccom.h>

struct RioCommand {
	uint16_t length;
	int	request;
	int	requesttype;
	int	value;
	int	index;
	void   *buffer;
	int	timeout;
};

#define	RIO_SEND_COMMAND	_IOWR('U', 200, struct RioCommand)
#define	RIO_RECV_COMMAND	_IOWR('U', 201, struct RioCommand)

#define	RIO_DIR_OUT               	        0x0
#define	RIO_DIR_IN				0x1
