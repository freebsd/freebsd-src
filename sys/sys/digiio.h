/*-
 * Copyright (c) 2001 Brian Somers <brian@Awfulhak.org>
 *   based on work by Slawa Olhovchenkov
 *                    John Prince <johnp@knight-trosoft.com>
 *                    Eric Hernes
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/digiio.h,v 1.4 2001/06/20 14:51:58 brian Exp $
 */

/*
 * A very small subset of cards.
 */
enum digi_model {
	PCXE,
	PCXEVE,
	PCXI,
	PCXEM,
	PCCX,
	PCIEPCX,
	PCIXR
};

enum {
	DIGIDB_INIT = (1<<0),
	DIGIDB_OPEN = (1<<1),
	DIGIDB_CLOSE = (1<<2),
	DIGIDB_SET = (1<<3),
	DIGIDB_INT = (1<<4),
	DIGIDB_READ = (1<<5),
	DIGIDB_WRITE = (1<<6),
	DIGIDB_RX = (1<<7),
	DIGIDB_TX = (1<<8),
	DIGIDB_IRQ = (1<<9),
	DIGIDB_MODEM = (1<<10),
	DIGIDB_RI = (1<<11),
};

#define	DIGIIO_REINIT		_IO('e', 'A')
#define	DIGIIO_DEBUG		_IOW('e', 'B', int)
#define	DIGIIO_RING		_IO('e', 'C')
#define	DIGIIO_MODEL		_IOR('e', 'D', enum digi_model)
#define	DIGIIO_IDENT		_IOW('e', 'E', char *)
#define	DIGIIO_SETALTPIN	_IOW('e', 'F', int)
#define	DIGIIO_GETALTPIN	_IOR('e', 'G', int)
