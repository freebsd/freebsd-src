/*
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <pccard/card.h>
#include <pccard/cis.h>

#include "readcis.h"

int     dump_pwr_desc(unsigned char *);
void    print_ext_speed(unsigned char, int);
void    dump_device_desc(unsigned char *p, int len, char *type);
void    dump_info_v1(unsigned char *p, int len);
void    dump_config_map(struct tuple *tp);
void    dump_cis_config(struct tuple *tp);
void    dump_other_cond(unsigned char *p);
void    dump_func_ext(unsigned char *p, int len);

void
dumpcis(struct cis *cp)
{
	struct tuple *tp;
	struct tuple_list *tl;
	int     count = 0, sz, ad, i;
	unsigned char *p;

	for (tl = cp->tlist; tl; tl = tl->next)
		for (tp = tl->tuples; tp; tp = tp->next) {
			printf("Tuple #%d, code = 0x%x (%s), length = %d\n",
			    ++count, tp->code, tuple_name(tp->code), tp->length);
			p = tp->data;
			sz = tp->length;
			ad = 0;
			while (sz > 0) {
				printf("    %03x: ", ad);
				for (i = 0; i < ((sz < 16) ? sz : 16); i++)
					printf(" %02x", p[i]);
				printf("\n");
				sz -= 16;
				p += 16;
				ad += 16;
			}
			switch (tp->code) {
			default:
				break;
			case CIS_MEM_COMMON:	/* 0x01 */
				dump_device_desc(tp->data, tp->length, "Common");
				break;
			case CIS_CHECKSUM:	/* 0x10 */
				if (tp->length == 5) {
					printf("\tChecksum from offset %d, length %d, value is 0x%x\n",
					    (short)((tp->data[1] << 8) | tp->data[0]),
					    (tp->data[3] << 8) | tp->data[2],
					    tp->data[4]);
				} else
					printf("\tIllegal length for checksum!\n");
				break;
			case CIS_LONGLINK_A:	/* 0x11 */
				printf("\tLong link to attribute memory, address 0x%x\n",
				    (tp->data[3] << 24) |
				    (tp->data[2] << 16) |
				    (tp->data[1] << 8)  |
				    tp->data[0]);
				break;
			case CIS_LONGLINK_C:	/* 0x12 */
				printf("\tLong link to common memory, address 0x%x\n",
				    (tp->data[3] << 24) |
				    (tp->data[2] << 16) |
				    (tp->data[1] << 8)  |
				    tp->data[0]);
				break;
				break;
			case CIS_INFO_V1:	/* 0x15 */
				dump_info_v1(tp->data, tp->length);
				break;
			case CIS_ALTSTR:	/* 0x16 */
				break;
			case CIS_MEM_ATTR:	/* 0x17 */
				dump_device_desc(tp->data, tp->length, "Attribute");
				break;
			case CIS_JEDEC_C:	/* 0x18 */
				break;
			case CIS_JEDEC_A:	/* 0x19 */
				break;
			case CIS_CONF_MAP:	/* 0x1A */
				dump_config_map(tp);
				break;
			case CIS_CONFIG:	/* 0x1B */
				dump_cis_config(tp);
				break;
			case CIS_DEVICE_OC:	/* 0x1C */
				dump_other_cond(tp->data);
				break;
			case CIS_DEVICE_OA:	/* 0x1D */
				dump_other_cond(tp->data);
				break;
			case CIS_DEVICEGEO:	/* 0x1E */
				break;
			case CIS_DEVICEGEO_A:	/* 0x1F */
				break;
			case CIS_MANUF_ID:	/* 0x20 */
				printf("\tPCMCIA ID = 0x%x, OEM ID = 0x%x\n",
				    (tp->data[1] << 8) | tp->data[0],
				    (tp->data[3] << 8) | tp->data[2]);
				break;
			case CIS_FUNC_ID:	/* 0x21 */
				switch (tp->data[0]) {
				default:
					printf("\tUnknown function");
					break;
				case 0:
					printf("\tMultifunction card");
					break;
				case 1:
					printf("\tMemory card");
					break;
				case 2:
					printf("\tSerial port/modem");
					break;
				case 3:
					printf("\tParallel port");
					break;
				case 4:
					printf("\tFixed disk card");
					break;
				case 5:
					printf("\tVideo adapter");
					break;
				case 6:
					printf("\tNetwork/LAN adapter");
					break;
				case 7:
					printf("\tAIMS");
					break;
				}
				printf("%s%s\n", (tp->data[1] & 1) ? " - POST initialize" : "",
				    (tp->data[1] & 2) ? " - Card has ROM" : "");
				break;
			case CIS_FUNC_EXT:	/* 0x22 */
				dump_func_ext(tp->data, tp->length);
				break;
			case CIS_VERS_2:	/* 0x40 */
				break;
			}
		}
}

/*
 *	Dump configuration map tuple.
 */
void
dump_config_map(struct tuple *tp)
{
	unsigned char *p, x;
	int     rlen, mlen;
	int     i;
	union {
		unsigned long l;
		unsigned char b[4];
	} u;

	rlen = (tp->data[0] & 3) + 1;
	mlen = ((tp->data[0] >> 2) & 3) + 1;
	u.l = 0;
	p = tp->data + 2;
	for (i = 0; i < rlen; i++)
		u.b[i] = *p++;
	printf("\tReg len = %d, config register addr = 0x%lx, last config = 0x%x\n",
	    rlen, u.l, tp->data[1]);
	if (mlen)
		printf("\tRegisters: ");
	for (i = 0; i < mlen; i++, p++) {
		for (x = 0x1; x; x <<= 1)
			printf("%c", x & *p ? 'X' : '-');
		printf(" ");
	}
	printf("\n");
}

/*
 *	Dump a config entry.
 */
void
dump_cis_config(struct tuple *tp)
{
	unsigned char *p, feat;
	int     i, j;
	char    c;

	p = tp->data;
	printf("\tConfig index = 0x%x%s\n", *p & 0x3F,
	    *p & 0x40 ? "(default)" : "");
	if (*p & 0x80) {
		p++;
		printf("\tInterface byte = 0x%x ", *p);
		switch (*p & 0xF) {
		default:
			printf("(reserved)");
			break;
		case 0:
			printf("(memory)");
			break;
		case 1:
			printf("(I/O)");
			break;
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
			printf("(custom)");
			break;
		}
		c = ' ';
		if (*p & 0x10) {
			printf(" BVD1/2 active");
			c = ',';
		}
		if (*p & 0x20) {
			printf("%c card WP active", c);	/* Write protect */
			c = ',';
		}
		if (*p & 0x40) {
			printf("%c +RDY/-BSY active", c);
			c = ',';
		}
		if (*p & 0x80)
			printf("%c wait signal supported", c);
		printf("\n");
	}
	p++;
	feat = *p++;
	switch (CIS_FEAT_POWER(feat)) {
	case 0:
		break;
	case 1:
		printf("\tVcc pwr:\n");
		p += dump_pwr_desc(p);
		break;
	case 2:
		printf("\tVcc pwr:\n");
		p += dump_pwr_desc(p);
		printf("\tVpp pwr:\n");
		p += dump_pwr_desc(p);
		break;
	case 3:
		printf("\tVcc pwr:\n");
		p += dump_pwr_desc(p);
		printf("\tVpp1 pwr:\n");
		p += dump_pwr_desc(p);
		printf("\tVpp2 pwr:\n");
		p += dump_pwr_desc(p);
		break;
	}
	if (feat & CIS_FEAT_TIMING) {
		i = CIS_WAIT_SCALE(*p);
		j = CIS_READY_SCALE(*p);
		p++;
		if (i != 3) {
			printf("\tWait scale ");
			print_ext_speed(*p, i);
			while (*p & 0x80)
				p++;
			printf("\n");
		}
		if (j != 7) {
			printf("\tRDY/BSY scale ");
			print_ext_speed(*p, j);
			while (*p & 0x80)
				p++;
			printf("\n");
		}
	}
	if (feat & CIS_FEAT_I_O) {
		if (CIS_IO_ADDR(*p))
			printf("\tCard decodes %d address lines",
				CIS_IO_ADDR(*p));
		else
			printf("\tCard provides address decode");
		switch (CIS_MEM_ADDRSZ(*p)) {
		case 0:
			break;
		case 1:
			printf(", 8 Bit I/O only");
			break;
		case 2:
			printf(", limited 8/16 Bit I/O");
			break;
		case 3:
			printf(", full 8/16 Bit I/O");
			break;
		}
		printf("\n");
		if (*p & CIS_IO_RANGE) {
			p++;
			c = *p++;
			for (i = 0; i <= CIS_IO_BLKS(c); i++) {
				printf("\t\tI/O address # %d: ", i + 1);
				switch (CIS_IO_ADSZ(c)) {
				case 0:
					break;
				case 1:
					printf("block start = 0x%x", *p++);
					break;
				case 2:
					printf("block start = 0x%x", (p[1] << 8) | *p);
					p += 2;
					break;
				case 3:
					printf("block start = 0x%x",
					    (p[3] << 24) | (p[2] << 16) |
					    (p[1] << 8) | *p);
					p += 4;
					break;
				}
				switch (CIS_IO_BLKSZ(c)) {
				case 0:
					break;
				case 1:
					printf(" block length = 0x%x", *p++ + 1);
					break;
				case 2:
					printf(" block length = 0x%x", ((p[1] << 8) | *p) + 1);
					p += 2;
					break;
				case 3:
					printf(" block length = 0x%x",
					    ((p[3] << 24) | (p[2] << 16) |
						(p[1] << 8) | *p) + 1);
					p += 4;
					break;
				}
				printf("\n");
			}
		}
	}

	/* IRQ descriptor */
	if (feat & CIS_FEAT_IRQ) {
		printf("\t\tIRQ modes:");
		c = ' ';
		if (*p & CIS_IRQ_LEVEL) {
			printf(" Level");
			c = ',';
		}
		if (*p & CIS_IRQ_PULSE) {
			printf("%c Pulse", c);
			c = ',';
		}
		if (*p & CIS_IRQ_SHARING)
			printf("%c Shared", c);
		printf("\n");
		if (*p & CIS_IRQ_MASK) {
			i = p[0] | (p[1] << 8);
			printf("\t\tIRQs: ");
			if (*p & 1)
				printf(" NMI");
			if (*p & 0x2)
				printf(" IOCK");
			if (*p & 0x4)
				printf(" BERR");
			if (*p & 0x8)
				printf(" VEND");
			for (j = 0; j < 16; j++)
				if (i & (1 << j))
					printf(" %d", j);
			printf("\n");
			p += 3;
		} else {
			printf("\t\tIRQ level = %d\n", CIS_IRQ_IRQN(*p));
			p++;
		}
	}
	switch (CIS_FEAT_MEMORY(feat)) {
	case 0:
		break;
	case 1:
		printf("\tMemory space length = 0x%x\n", (p[1] << 8) | p[0]);
		p += 2;
		break;
	case 2:
		printf("\tMemory space address = 0x%x, length = 0x%x\n",
		    (p[3] << 8) | p[2],
		    (p[1] << 8) | p[0]);
		p += 4;
		break;

	/* Memory descriptors. */
	case 3:
		c = *p++;
		for (i = 0; i <= (c & 7); i++) {
			printf("\tMemory descriptor %d\n\t\t", i + 1);
			switch (CIS_MEM_LENSZ(c)) {
			case 0:
				break;
			case 1:
				printf(" blk length = 0x%x00", *p++);
				break;
			case 2:
				printf(" blk length = 0x%x00", (p[1] << 8) | *p);
				p += 2;
				break;
			case 3:
				printf(" blk length = 0x%x00",
				    (p[3] << 24) | (p[2] << 16) |
				    (p[1] << 8) | *p);
				p += 4;
				break;
			}
			switch (CIS_MEM_ADDRSZ(c)) {
			case 0:
				break;
			case 1:
				printf(" card addr = 0x%x00", *p++);
				break;
			case 2:
				printf(" card addr = 0x%x00", (p[1] << 8) | *p);
				p += 2;
				break;
			case 3:
				printf(" card addr = 0x%x00",
				    (p[3] << 24) | (p[2] << 16) |
				    (p[1] << 8) | *p);
				p += 4;
				break;
			}
			if (c & CIS_MEM_HOST)
				switch ((c >> 5) & 3) {
				case 0:
					break;
				case 1:
					printf(" host addr = 0x%x00", *p++);
					break;
				case 2:
					printf(" host addr = 0x%x00", (p[1] << 8) | *p);
					p += 2;
					break;
				case 3:
					printf(" host addr = 0x%x00",
					    (p[3] << 24) | (p[2] << 16) |
					    (p[1] << 8) | *p);
					p += 4;
					break;
				}
			printf("\n");
		}
		break;
	}
	if (feat & CIS_FEAT_MISC) {
		printf("\tMax twin cards = %d\n", *p & 7);
		printf("\tMisc attr:");
		if (*p & 0x8)
			printf(" (Audio-BVD2)");
		if (*p & 0x10)
			printf(" (Read-only)");
		if (*p & 0x20)
			printf(" (Power down supported)");
		if (*p & 0x80) {
			printf(" (Ext byte = 0x%x)", p[1]);
			p++;
		}
		printf("\n");
		p++;
	}
}

/*
 *	dump_other_cond - Dump other conditions.
 */
void
dump_other_cond(unsigned char *p)
{
	if (p[0]) {
		printf("\t");
		if (p[0] & 1)
			printf("(MWAIT)");
		if (p[0] & 2)
			printf(" (3V card)");
		if (p[0] & 0x80)
			printf(" (Extension bytes follow)");
		printf("\n");
	}
}

/*
 *	Dump power descriptor.
 */
int
dump_pwr_desc(unsigned char *p)
{
	int     len = 1, i;
	unsigned char mask;
	char  **expp;
	static char *pname[] =
	{"Nominal operating supply voltage",
	 "Minimum operating supply voltage",
	 "Maximum operating supply voltage",
	 "Continuous supply current",
	 "Max current average over 1 second",
	 "Max current average over 10 ms",
	 "Power down supply current",
	 "Reserved"
	};
	static char *vexp[] =
	{"10uV", "100uV", "1mV", "10mV", "100mV", "1V", "10V", "100V"};
	static char *cexp[] =
	{"10nA", "1uA", "10uA", "100uA", "1mA", "10mA", "100mA", "1A"};
	static char *mant[] =
	{"1", "1.2", "1.3", "1.5", "2", "2.5", "3", "3.5", "4", "4.5",
	"5", "5.5", "6", "7", "8", "9"};

	mask = *p++;
	expp = vexp;
	for (i = 0; i < 8; i++)
		if (mask & (1 << i)) {
			len++;
			if (i >= 3)
				expp = cexp;
			printf("\t\t%s: ", pname[i]);
			printf("%s x %s",
			    mant[(*p >> 3) & 0xF],
			    expp[*p & 7]);
			while (*p & 0x80) {
				len++;
				p++;
				printf(", ext = 0x%x", *p);
			}
			printf("\n");
			p++;
		}
	return (len);
}

void
dump_device_desc(unsigned char *p, int len, char *type)
{
	static char *un_name[] =
	{"512b", "2Kb", "8Kb", "32Kb", "128Kb", "512Kb", "2Mb", "reserved"};
	static char *speed[] =
	{"No speed", "250nS", "200nS", "150nS",
	"100nS", "Reserved", "Reserved"};
	static char *dev[] =
	{"No device", "Mask ROM", "OTPROM", "UV EPROM",
	 "EEPROM", "FLASH EEPROM", "SRAM", "DRAM",
	 "Reserved", "Reserved", "Reserved", "Reserved",
	 "Reserved", "Function specific", "Extended",
	"Reserved"};
	int     count = 0;

	while (*p != 0xFF && len > 0) {
		unsigned char x;

		x = *p++;
		len -= 2;
		if (count++ == 0)
			printf("\t%s memory device information:\n", type);
		printf("\t\tDevice number %d, type %s, WPS = %s\n",
		    count, dev[x >> 4], (x & 0x8) ? "ON" : "OFF");
		if ((x & 7) == 7) {
			len--;
			if (*p) {
				printf("\t\t");
				print_ext_speed(*p, 0);
				while (*p & 0x80) {
					p++;
					len--;
				}
			}
			p++;
		} else
			printf("\t\tSpeed = %s", speed[x & 7]);
		printf(", Memory block size = %s, %d units\n",
		    un_name[*p & 7], (*p >> 3) + 1);
		p++;
	}
}

/*
 *	Print version info
 */
void
dump_info_v1(unsigned char *p, int len)
{
	printf("\tVersion = %d.%d", p[0], p[1]);
	p += 2;
	printf(", Manuf = [%s],", p);
	while (*p++);
	printf("card vers = [%s]\n", p);
	while (*p++);
	printf("\tAddit. info = [%s]", p);
	while (*p++);
	printf(",[%s]\n", p);
}

/*
 *	dump functional extension tuple.
 */
void
dump_func_ext(unsigned char *p, int len)
{
	if (len == 0)
		return;
	switch (p[0]) {
	case 0:
	case 8:
	case 10:
		if (len != 4) {
			printf("\tWrong length for serial extension\n");
			return;
		}
		printf("\tSerial interface extension:\n");
		switch (p[1] & 0x1F) {
		default:
			printf("\t\tUnkn device");
			break;
		case 0:
			printf("\t\t8250 UART");
			break;
		case 1:
			printf("\t\t16450 UART");
			break;
		case 2:
			printf("\t\t16550 UART");
			break;
		}
		printf(", Parity - %s%s%s%s",
		    (p[2] & 1) ? "Space," : "",
		    (p[2] & 2) ? "Mark," : "",
		    (p[2] & 4) ? "Odd," : "",
		    (p[2] & 8) ? "Even," : "");
		printf("\n");
		break;
	case 1:
	case 5:
	case 6:
	case 7:
		printf("\tModem interface capabilities:\n");
		break;
	case 2:
		printf("\tData modem services available:\n");
		break;
	case 9:
		printf("\tFax/modem services available:\n");
		break;
	case 4:
		printf("\tVoice services available:\n");
		break;
	}
}

/*
 *	print_ext_speed - Print extended speed.
 */
void
print_ext_speed(unsigned char x, int scale)
{
	static char *mant[] =
	{"Reserved", "1.0", "1.2", "1.3", "1.5", "2.0", "2.5", "3.0",
	"3.5", "4.0", "4.5", "5.0", "5.5", "6.0", "7.0", "8.0"};
	static char *exp[] =
	{"1 ns", "10 ns", "100 ns", "1 us", "10 us", "100 us",
	"1 ms", "10 ms"};
	static char *scale_name[] =
	{"None", "10", "100", "1,000", "10,000", "100,000",
	"1,000,000", "10,000,000"};

	printf("Speed = %s x %s", mant[(x >> 3) & 0xF], exp[x & 7]);
	if (scale)
		printf(", scaled by %s", scale_name[scale & 7]);
}
