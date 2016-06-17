/* -*- linux-c -*- */
/*
 * sp502.h - chip definitions for the
 *	Sipex SP502 Multi-Mode Serial Transceiver
 *
 * Bjoren Davis, Aurora Technologies, 21. January, 1995.
 *
 * COPYRIGHT (c) 1995-1999 BY AURORA TECHNOLOGIES, INC., WALTHAM, MA.
 *
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 *	file: sp502.h
 *	author: bkd
 *	created: 1/21/95
 *	revision info: $Id: sp502.h,v 1.3 2002/02/10 22:17:26 martillo Exp $
 *	ripped off from: Header: /vol/sources.cvs/dev/acs/include/sp502.h,v 1.4 1996/11/07 21:35:10 bkd Exp 
 *	Used without modification in the multichannel server portion of the Linux driver by Joachim Martillo
 */

#ifndef _SP502_H
#define _SP502_H

#ifdef sun
#   pragma ident "@(#)$Header: /usr/local/cvs/linux-2.4.6/drivers/net/wan/8253x/sp502.h,v 1.3 2002/02/10 22:17:26 martillo Exp $"
#endif

/*
 * These following nibble values are from the SP502 Data Sheet, which
 *  is in the Sipex Interface Products Catalog, 1994 Edition, pages
 *  168 and 170.
 */

/* same order as the modes in 8253xioc.h and as the names in 8253xtty.c and as the progbytes in 8253xmcs.c*/

#define SP502_OFF	((unsigned char) 0x00)
#define SP502_RS232	((unsigned char) 0x02)
#define SP502_RS422	((unsigned char) 0x04)
#define SP502_RS485	((unsigned char) 0x05)
#define SP502_RS449	((unsigned char) 0x0c)
#define SP502_EIA530	((unsigned char) 0x0d)
#define SP502_V35	((unsigned char) 0x0e)

#endif		/* !_SP502_H */
