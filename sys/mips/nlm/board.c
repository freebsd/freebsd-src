/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NETLOGIC_BSD */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <net/ethernet.h>

#include <mips/nlm/hal/mips-extns.h>
#include <mips/nlm/hal/haldefs.h>
#include <mips/nlm/hal/iomap.h>
#include <mips/nlm/hal/fmn.h>
#include <mips/nlm/hal/pic.h>
#include <mips/nlm/hal/sys.h>
#include <mips/nlm/hal/uart.h>

#include <mips/nlm/xlp.h>
#include <mips/nlm/board.h>

static uint8_t board_eeprom_buf[EEPROM_SIZE];
static int board_eeprom_set;

struct xlp_board_info xlp_board_info;

static void
nlm_print_processor_info(void)
{
	uint32_t procid;
	int prid, rev;
	char *chip, *revstr;

	procid = mips_rd_prid();
	prid = (procid >> 8) & 0xff;
	rev = procid & 0xff;

	switch (prid) {
	case CHIP_PROCESSOR_ID_XLP_8XX:
		chip = "XLP 832";
		break;
	case CHIP_PROCESSOR_ID_XLP_3XX:
		chip = "XLP 3xx";
		break;
	case CHIP_PROCESSOR_ID_XLP_432:
	case CHIP_PROCESSOR_ID_XLP_416:
		chip = "XLP 4xx";
		break;
	default:
		chip = "XLP ?xx";
		break;
	}
	switch (rev) {
	case 0:
		revstr = "A0"; break;
	case 1:
		revstr = "A1"; break;
	case 2:
		revstr = "A2"; break;
	case 3:
		revstr = "B0"; break;
	default:
		revstr = "??"; break;
	}

	printf("Processor info:\n");
	printf("  Netlogic %s %s [%x]\n", chip, revstr, procid);
}

/*
 * All our knowledge of chip and board that cannot be detected by probing 
 * at run-time goes here
 */
static int
nlm_setup_xlp_board(void)
{
	struct xlp_board_info	*boardp;
	int rv;
	uint8_t *b;

	/* start with a clean slate */
	boardp = &xlp_board_info;
	memset(boardp, 0, sizeof(*boardp));
	boardp->nodemask = 0x1;	/* only node 0 */
	nlm_print_processor_info();

	b =  board_eeprom_buf;
	rv = nlm_board_eeprom_read(0, EEPROM_I2CBUS, EEPROM_I2CADDR, 0, b,
	    EEPROM_SIZE);
	if (rv == 0) {
		board_eeprom_set = 1;
		printf("Board info (EEPROM on i2c@%d at %#X):\n",
		    EEPROM_I2CBUS, EEPROM_I2CADDR);
		printf("  Model:      %7.7s %2.2s\n", &b[16], &b[24]);
		printf("  Serial #:   %3.3s-%2.2s\n", &b[27], &b[31]);
		printf("  MAC addr:   %02x:%02x:%02x:%02x:%02x:%02x\n",
		    b[2], b[3], b[4], b[5], b[6], b[7]);
	} else
		printf("Board Info: Error on EEPROM read (i2c@%d %#X).\n",
		    EEPROM_I2CBUS, EEPROM_I2CADDR);

	return (0);
}

int nlm_board_info_setup(void)
{
	nlm_setup_xlp_board();
	return (0);
}
