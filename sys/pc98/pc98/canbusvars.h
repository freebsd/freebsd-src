/*-
 * Copyright (c) 2000 Takanori Watanabe <wnabe@par.odn.ne.jp>
 * Copyright (c) 2000 KIYOHARA Takashi <kiyohara@kk.iij4u.or.jp>
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
 *
 * $FreeBSD$
 */

#ifndef _PC98_PC98_CANBUSVARS_H_
#define _PC98_PC98_CANBUSVARS_H_


/* CanBe I/O register */
#define CANBE_IOPORT_INDEX 0xf4a
#define CANBE_IOPORT_DATA 0xf4b

/*
 * following registor purpose for spending -- unknown.
 */
#define CANBE_IOPORT1 0x0c24
#define CANBE_IOPORT2 0x0c2b
#define CANBE_IOPORT3 0x0c2d


/* CanBe register number */
#define CANBE_SOUND_INTR_ADDR	0x01
#define CANBE_RC_RESET		0x03
#define CANBE_MUTE_CTRL		0x04
#define CANBE_RC_DATA_STATUS	0x10
#define CANBE_RC_RECV_CODE	0x11
#define CANBE_POWER_CTRL	0x13
#define CANBE_RC_USED_INTR	0x14


/* CanBe sound interrupt address value */
#define CANBE_SOUND_INTR_VAL0	0x00
#define CANBE_SOUND_INTR_VAL1	0x02
#define CANBE_SOUND_INTR_VAL2	0x03
#define CANBE_SOUND_INTR_VAL3	0x08

/* CanBe remote controler reset */
#define CANBE_MIKE_THRUE	0x04
#define CANBE_CTRLR_RESET	0x01

/* CanBe mute control */
#define CANBE_MUTE		0x01

/* CanBe remote controler data status */
#define CANBE_RC_BUSY		0x02
#define CANBE_RC_STATUS		0x01

/* CanBe remote controler receive code */
#define CANBE_RC_DATA_CHUP	0x00
#define CANBE_RC_DATA_CHDOWN	0x01
#define CANBE_RC_DATA_VOLUP	0x02
#define CANBE_RC_DATA_VOLDOWN	0x03
#define CANBE_RC_DATA_EJECT	0x04
#define CANBE_RC_DATA_PLAY	0x05
#define CANBE_RC_DATA_MUTE	0x09
#define CANBE_RC_DATA_VIDEO	0x0a
#define CANBE_RC_DATA_NEXT	0x0c
#define CANBE_RC_DATA_PREVIOUS	0x0d
#define CANBE_RC_DATA_M_S	0x1d
#define CANBE_RC_DATA_UP	0x40
#define CANBE_RC_DATA_DOWN	0x41
#define CANBE_RC_DATA_LEFT	0x42
#define CANBE_RC_DATA_RIGHT	0x43
#define CANBE_RC_DATA_SIZE	0x4d
#define CANBE_RC_DATA_ESC	0x4e
#define CANBE_RC_DATA_CR	0x4f
#define CANBE_RC_DATA_TV	0x53
#define CANBE_RC_DATA_FREEZE	0x5d
#define CANBE_RC_DATA_CAPTURE	0x5e

/* CanBe power off data */
#define CANBE_POWEROFF_DATA {		\
	0x80, 0x06, 0x00, 0x00,		\
	0x80, 0x07, 0x00, 0x01,		\
	0x80, 0x01, 0x00, 0x00		\
}

/* CanBe remote controler used intr */
#define CANBE_RC_INTR		0x04
#define CANBE_RC_INTR_INT41	0x03 /* irq 10 */
#define CANBE_RC_INTR_INT1	0x02 /* irq  5 */
#define CANBE_RC_INTR_INT2	0x01 /* irq  6 */
#define CANBE_RC_INTR_INT0	0x00 /* irq  3 */

#endif	/* _PC98_PC98_CANBUSVARS_H_ */
