/*
 *   Copyright (c) 1999 Hellmuth Michaelis. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *   ---
 *
 *   The A-law to u-law and u-law to A-law conversion routines and tables
 *   were taken from the G.711 reference implementation from Sun and freely
 *   available as http://www.itu.int/itudoc/itu-t/rec/g/g700-799/refimpl.txt.
 *
 *   Therefore for that part of the code, the following restrictions apply:
 *
 *
 *   This source code is a product of Sun Microsystems, Inc. and is provided
 *   for unrestricted use.  Users may copy or modify this source code without
 *   charge.
 *
 *   SUN SOURCE CODE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING
 *   THE WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 *   PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 *   Sun source code is provided with no support and without any obligation on
 *   the part of Sun Microsystems, Inc. to assist in its use, correction,
 *   modification or enhancement.
 *
 *   SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 *   INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS SOFTWARE
 *   OR ANY PART THEREOF.
 *
 *   In no event will Sun Microsystems, Inc. be liable for any lost revenue
 *   or profits or other special, indirect and consequential damages, even if
 *   Sun has been advised of the possibility of such damages.
 *
 *   Sun Microsystems, Inc.
 *   2550 Garcia Avenue
 *   Mountain View, California  94043
 *
 *   ---
 *
 *   The bitreverse table was contributed by Stefan Bethke.
 *
 *---------------------------------------------------------------------------
 *
 *	A-law / u-law conversions as specified in G.711
 *	-----------------------------------------------
 *
 *	last edit-date: [Mon Dec 13 21:44:01 1999]
 *
 *	$Id: g711conv.c,v 1.5 1999/12/13 21:25:24 hm Exp $
 *
 * $FreeBSD: src/usr.sbin/i4b/g711conv/g711conv.c,v 1.4 1999/12/14 21:07:24 hm Exp $
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>
#include <machine/i4b_ioctl.h>

/* copy from CCITT G.711 specifications */

/* u- to A-law conversions */

unsigned char _u2a[128] = {
	1,	1,	2,	2,	3,	3,	4,	4,
	5,	5,	6,	6,	7,	7,	8,	8,
	9,	10,	11,	12,	13,	14,	15,	16,
	17,	18,	19,	20,	21,	22,	23,	24,
	25,	27,	29,	31,	33,	34,	35,	36,
	37,	38,	39,	40,	41,	42,	43,	44,
	46,	48,	49,	50,	51,	52,	53,	54,
	55,	56,	57,	58,	59,	60,	61,	62,
	64,	65,	66,	67,	68,	69,	70,	71,
	72,	73,	74,	75,	76,	77,	78,	79,
	81,	82,	83,	84,	85,	86,	87,	88,
	89,	90,	91,	92,	93,	94,	95,	96,
	97,	98,	99,	100,	101,	102,	103,	104,
	105,	106,	107,	108,	109,	110,	111,	112,
	113,	114,	115,	116,	117,	118,	119,	120,
	121,	122,	123,	124,	125,	126,	127,	128
};

/* A- to u-law conversions */

unsigned char _a2u[128] = {
	1,	3,	5,	7,	9,	11,	13,	15,
	16,	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,	31,
	32,	32,	33,	33,	34,	34,	35,	35,
	36,	37,	38,	39,	40,	41,	42,	43,
	44,	45,	46,	47,	48,	48,	49,	49,
	50,	51,	52,	53,	54,	55,	56,	57,
	58,	59,	60,	61,	62,	63,	64,	64,
	65,	66,	67,	68,	69,	70,	71,	72,
	73,	74,	75,	76,	77,	78,	79,	79,
	80,	81,	82,	83,	84,	85,	86,	87,
	88,	89,	90,	91,	92,	93,	94,	95,
	96,	97,	98,	99,	100,	101,	102,	103,
	104,	105,	106,	107,	108,	109,	110,	111,
	112,	113,	114,	115,	116,	117,	118,	119,
	120,	121,	122,	123,	124,	125,	126,	127
};

/* reverse bits (7->0, 6->1, 5->2 etc) for tx to / rx from ISDN */

unsigned char bitreverse[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa, 
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/* A-law to u-law conversion */

unsigned char alaw2ulaw(unsigned char aval)
{
	aval &= 0xff;
	return ((aval & 0x80) ? (0xFF ^ _a2u[aval ^ 0xD5]) :
				(0x7F ^ _a2u[aval ^ 0x55]));
}

/* u-law to A-law conversion */

unsigned char ulaw2alaw(unsigned char uval)
{
	uval &= 0xff;
	return ((uval & 0x80) ? (0xD5 ^ (_u2a[0xFF ^ uval] - 1)) :
				(0x55 ^ (_u2a[0x7F ^ uval] - 1)));
}

void
usage(void)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "g711conv - do conversions according to ITU G.711, (version %d.%d.%d)\n",VERSION, REL, STEP);
	fprintf(stderr, "usage: g711conv -a -r -R -u -P\n");
	fprintf(stderr, "       -a      A-law to u-law conversion\n");
	fprintf(stderr, "       -r      reverse bits before conversion\n");
	fprintf(stderr, "       -R      reverse bits after conversion\n");
	fprintf(stderr, "       -u      u-law to A-law conversion\n");
	fprintf(stderr, "       -P      print conversion table as C source\n");	
	fprintf(stderr, "\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	int i;
	int c;
	int opt_a = 0;
	int opt_u = 0;
	int opt_r = 0;
	int opt_P = 0;
	int opt_R = 0;	
	unsigned char uc;
	
	while ((c = getopt(argc, argv, "aurPR?")) != -1)
	{
		switch(c)
		{
			case 'a':
				opt_a = 1;
				break;
				
			case 'u':
				opt_u = 1;
				break;
				
			case 'r':
				opt_r = 1;
				break;
				
			case 'R':
				opt_R = 1;
				break;

			case 'P':
				opt_P = 1;
				break;

			case '?':
			default:
				usage();
				break;
		}
	}

	if((opt_a + opt_u) > 1)
		usage();

	if(opt_P)
	{		
		printf("\n/* ");
		
		if((opt_a + opt_u) == 0)
			printf("No Conversion");
	
		if(opt_a)
			printf("A-law to u-law conversion");
	
		if(opt_u)
			printf("u-law to A-law conversion");
	
		if(opt_r)
			printf(", reverse bits BEFORE conversion");
	
		if(opt_R)
			printf(", reverse bits AFTER conversion");

		if(opt_a)
		{
			printf(" */\n\nunsigned char a2u_tab[256] = {");
		}
		else if(opt_u)
		{
			printf(" */\n\nunsigned char u2a_tab[256] = {");
		}
		else
		{
			printf(" */\n\nunsigned char table[256] = {");
		}
		
		for(i=0; i < 256; i++)
		{
			uc = i;
			
			if(!(i % 8))
				printf("\n/* %02x */\t", i);
	
			if(opt_r)
				uc = bitreverse[uc];
	
			if(opt_u)
				uc = ulaw2alaw(uc);
	
			if(opt_a)
				uc = alaw2ulaw(uc);
				
			if(opt_R)
				uc = bitreverse[uc];

			if(i == 255)	
				printf("0x%02x", uc);
			else
				printf("0x%02x, ", uc);
		}
		printf("\n};\n");
	}
	else
	{
		unsigned char ib[1];
		
		while(fread(ib, 1, 1, stdin) == 1)
		{
			if(opt_r)
				ib[0] = bitreverse[ib[0]];
	
			if(opt_u)
				ib[0] = ulaw2alaw(ib[0]);
	
			if(opt_a)
				ib[0] = alaw2ulaw(ib[0]);
				
			if(opt_R)
				ib[0] = bitreverse[ib[0]];
	
			fwrite(ib, 1, 1, stdout);
		}
	}	
	return(0);
}

/* EOF */
