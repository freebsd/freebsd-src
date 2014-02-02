/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/types.h>
#include <sys/endian.h>

#include <inttypes.h>


/*
 * Altera University Program SD Card micro-driver for boot2 and loader.
 */

/* Constants lifted from altera_sdcard.h -- possibly we should share headers? */
#define ALTERA_SDCARD_OFF_RXTX_BUFFER   0       /* 512-byte I/O buffer */
#define ALTERA_SDCARD_OFF_CID           512     /* 16-byte Card ID number */
#define ALTERA_SDCARD_OFF_CSD           528     /* 16-byte Card Specific Data */
#define ALTERA_SDCARD_OFF_OCR           544     /* Operating Conditions Reg */
#define ALTERA_SDCARD_OFF_SR            548     /* SD Card Status Register */
#define ALTERA_SDCARD_OFF_RCA           552     /* Relative Card Address Reg */
#define ALTERA_SDCARD_OFF_CMD_ARG       556     /* Command Argument Register */
#define ALTERA_SDCARD_OFF_CMD           560     /* Command Register */
#define ALTERA_SDCARD_OFF_ASR           564     /* Auxiliary Status Register */
#define ALTERA_SDCARD_OFF_RR1           568     /* Response R1 */

#define ALTERA_SDCARD_SECTORSIZE        512

#define ALTERA_SDCARD_CMD_SEND_RCA      0x03    /* Retrieve card RCA. */
#define ALTERA_SDCARD_CMD_SEND_CSD      0x09    /* Retrieve CSD register. */
#define ALTERA_SDCARD_CMD_SEND_CID      0x0A    /* Retrieve CID register. */
#define ALTERA_SDCARD_CMD_READ_BLOCK    0x11    /* Read block from disk. */
#define ALTERA_SDCARD_CMD_WRITE_BLOCK   0x18    /* Write block to disk. */

#define ALTERA_SDCARD_ASR_CMDVALID      0x0001
#define ALTERA_SDCARD_ASR_CARDPRESENT   0x0002
#define ALTERA_SDCARD_ASR_CMDINPROGRESS 0x0004
#define ALTERA_SDCARD_ASR_SRVALID       0x0008
#define ALTERA_SDCARD_ASR_CMDTIMEOUT    0x0010
#define ALTERA_SDCARD_ASR_CMDDATAERROR  0x0020

#define	ALTERA_SDCARD_RR1_INITPROCRUNNING	0x0100
#define	ALTERA_SDCARD_RR1_ERASEINTERRUPTED	0x0200
#define	ALTERA_SDCARD_RR1_ILLEGALCOMMAND	0x0400
#define	ALTERA_SDCARD_RR1_COMMANDCRCFAILED	0x0800
#define	ALTERA_SDCARD_RR1_ADDRESSMISALIGNED	0x1000
#define	ALTERA_SDCARD_RR1_ADDRBLOCKRANGE	0x2000

/*
 * Not all RR1 values are "errors" per se -- check only for the ones that are
 * when performing error handling.
 */
#define ALTERA_SDCARD_RR1_ERRORMASK                                           \
    (ALTERA_SDCARD_RR1_ERASEINTERRUPTED | ALTERA_SDCARD_RR1_ILLEGALCOMMAND |  \
    ALTERA_SDCARD_RR1_COMMANDCRCFAILED | ALTERA_SDCARD_RR1_ADDRESSMISALIGNED |\
    ALTERA_SDCARD_RR1_ADDRBLOCKRANGE)

extern void __cheri_sdcard_vaddr__;

#define	ALTERA_SDCARD_PTR(type, offset)					\
	(type *)((uint8_t *)&__cheri_sdcard_vaddr__ + (offset))

static __inline uint16_t
altera_sdcard_read_uint16(u_int offset)
{
	uint16_t *p;

	p = ALTERA_SDCARD_PTR(uint16_t, offset);
	return (le16toh(*p));
}

static __inline void
altera_sdcard_write_uint16(u_int offset, uint16_t v)
{
	uint16_t *p;

	p = ALTERA_SDCARD_PTR(uint16_t, offset);
	*p = htole16(v);
}

static __inline void
altera_sdcard_write_uint32(u_int offset, uint32_t v)
{
	uint32_t *p;

	p = ALTERA_SDCARD_PTR(uint32_t, offset);
	*p = htole32(v);
}

static __inline uint16_t
altera_sdcard_read_asr(void)
{

	return (altera_sdcard_read_uint16(ALTERA_SDCARD_OFF_ASR));
}

static __inline uint16_t
altera_sdcard_read_rr1(void)
{

	return (altera_sdcard_read_uint16(ALTERA_SDCARD_OFF_RR1));
}

static __inline void
altera_sdcard_write_cmd(uint16_t cmd)
{

	altera_sdcard_write_uint16(ALTERA_SDCARD_OFF_CMD, cmd);
}

static __inline void
altera_sdcard_write_cmd_arg(uint32_t cmd_arg)
{

	altera_sdcard_write_uint32(ALTERA_SDCARD_OFF_CMD_ARG, cmd_arg);
}

/*
 * Private interface: load exactly one block of size ALTERA_SDCARD_SECTORSIZE
 * from block #lba.
 */
static int
altera_sdcard_read_block(void *buf, unsigned lba)
{
	uint32_t *bufp, *rxtxp;
	uint16_t asr, rr1;
	int i;

	if (!(altera_sdcard_read_asr() & ALTERA_SDCARD_ASR_CARDPRESENT))
		return (-1);

	bufp = (uint32_t *)buf;
	rxtxp = ALTERA_SDCARD_PTR(uint32_t, ALTERA_SDCARD_OFF_RXTX_BUFFER);

	/*
	 * Issue read block command.
	 */
	altera_sdcard_write_cmd_arg(lba * ALTERA_SDCARD_SECTORSIZE);
	altera_sdcard_write_cmd(ALTERA_SDCARD_CMD_READ_BLOCK);

	/*
	 * Wait for device to signal completion of command.
	 */
	while ((asr = altera_sdcard_read_asr()) &
	    ALTERA_SDCARD_ASR_CMDINPROGRESS);

	/*
	 * Due to hardware bugs/features, interpretting this field is messy.
	 */
	rr1 &= ~ALTERA_SDCARD_RR1_COMMANDCRCFAILED;	/* ??? */
	rr1 = altera_sdcard_read_rr1();
	if (asr & ALTERA_SDCARD_ASR_CMDTIMEOUT)
		return (-1);
	if ((asr & ALTERA_SDCARD_ASR_CMDDATAERROR) &&
	    (rr1 & ALTERA_SDCARD_RR1_ERRORMASK))
		return (-1);

	/*
	 * We can't use a regular memcpy() due to byte-enable bugs in the
	 * Altera IP core: instead copy in 32-bit units.
	 */
	for (i = 0; i < ALTERA_SDCARD_SECTORSIZE/sizeof(uint32_t); i++)
		bufp[i] = rxtxp[i];
	return (0);
}

/*
 * Public interface: load 'nblk' blocks from block #lba into *buf.
 */
int
altera_sdcard_read(void *buf, unsigned lba, unsigned nblk)
{
	uint8_t *bufp = buf;
	int i;

	for (i = 0; i < nblk; i++) {
		if (altera_sdcard_read_block(bufp + i *
		    ALTERA_SDCARD_SECTORSIZE, lba + i) < 0)
			return (-1);
	}
	return (0);
}
