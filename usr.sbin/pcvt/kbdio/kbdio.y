/* Hello emacs, this should be edited in -*- Fundamental -*- mode */
%{
/*
 * Copyright (c) 1994 Joerg Wunsch
 *
 * All rights reserved.
 *
 * This program is free software.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Joerg Wunsch
 * 4. The name of the developer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ident "$FreeBSD: src/usr.sbin/pcvt/kbdio/kbdio.y,v 1.6 1999/09/06 07:39:30 peter Exp $"

/*
 * $Log: kbdio.y,v $
 * Revision 1.2  1994/09/18  19:49:22  j
 * Refined expr handling; can now set/clear bits.
 *
 * Revision 1.1  1994/09/18  12:57:13  j
 * Initial revision
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/fcntl.h>
#include <machine/cpufunc.h>
#include <machine/pcvt_ioctl.h>

#ifdef __NetBSD__
#include <machine/pio.h>
#endif

#define	KBD_DELAY \
	{ u_char x = inb(0x84); } \
	{ u_char x = inb(0x84); } \
	{ u_char x = inb(0x84); } \
	{ u_char x = inb(0x84); } \
	{ u_char x = inb(0x84); } \
	{ u_char x = inb(0x84); }

#define YYDEBUG 1

void yyerror(const char *msg);

static	void help(int), status(void), data(int), kbd(int), cmdbyte(int),
	kbc(int), whatMCA(void);
static	int kbget(void);
%}

%union {
	int num;
}

%token		NEWLINE
%token		ALL CMD DATA DEFAULTS ECHOC ENABLE EXPR HELP ID LED
%token		MAKE ONLY RELEASE RESEND RESET SCAN STATUS TYPEMATIC
%token		WHAT
%token	<num>	NUM

%type	<num>	expr opr

%%

interpret:	lines ;

lines:		line
		| lines line
		;

line:		statements NEWLINE
		| NEWLINE
		| error	NEWLINE		{ fprintf(stderr, "bing!\n"); }
		;

statements:	statement
		| statements ';' statement
		;

statement:	'?'			{ help(0); }
		| HELP			{ help(0); }
		| HELP EXPR		{ help(1); }
		| STATUS '?'		{ status(); }
		| WHAT '?'		{ whatMCA(); }
		| DATA '?'		{ data(kbget()); }
		| LED '=' NUM		{ kbd(0xed); kbd($3); }
		| ECHOC			{ kbd(0xee); kbget(); }
		| SCAN '=' NUM		{ kbd(0xf0); kbd($3);
					  if($3 == 0) data(kbget()); }
		| SCAN '?'		{ kbd(0xf0); kbd(0); data(kbget()); }
		| ID '?'		{ kbd(0xf2); data(kbget());
					  data(kbget()); }
		| TYPEMATIC '=' NUM ',' NUM
					{ kbd(0xf3);
					  if($3 > 1000) $3 = 1000;
					  if($5 > 30) $5 = 30;
					  if($5 < 2) $5 = 2;
					  kbd(
					      (int)
						(8.0 * log(30.0 / (double)$5)
						 / log(2))
					      | ((($3 / 250) - 1) * 32)
					     );
					}
		| ENABLE		{ kbd(0xf4); }
		| DEFAULTS		{ kbd(0xf6); }
		| ALL TYPEMATIC		{ kbd(0xf7); }
		| ALL MAKE RELEASE	{ kbd(0xf8); }
		| ALL MAKE ONLY		{ kbd(0xf9); }
		| ALL TYPEMATIC MAKE RELEASE
					{ kbd(0xfa); }
		| NUM TYPEMATIC		{ kbd(0xfb); kbd($1); }
		| NUM MAKE RELEASE	{ kbd(0xfc); kbd($1); }
		| NUM MAKE ONLY		{ kbd(0xfd); kbd($1); }
		| RESEND		{ kbd(0xfe); }
		| RESET			{ kbd(0xff); }
		| CMD '?'		{ kbc(0x20); cmdbyte(kbget()); }
		| CMD '=' expr		{ kbc(0x60); kbd($3); }
		| /* lambda */
		;

expr:		opr			{ $$ = $1; }
		| expr '+' opr		{ $$ = $1 | $3; }
		| expr '-' opr		{ $$ = $1 & ~($3); }
		;

opr:		NUM			{ $$ = $1; }
		| CMD			{ kbc(0x20); $$ = kbget(); }
		;

%%

static void
help(int topic) {
	switch(topic) {
	case 0:
	printf(
	"Input consists of lines, containing one or more semicolon-separated\n"
	"statements. Numbers are implicitly hexadecimal, append a dot for\n"
	"decimal numbers. Valid statements include:\n"
	"help [expr];		give help [to expression syntax]\n"
	"status ?		interpret kbd ctrl status byte\n"
	"what ?			check for MCA type 1 or 2 motherboard controller\n"
	"data ?			get one byte of data\n"
	"led = NUM		set kbd LEDs\n"
	"echo = NUM		echo byte to kbd\n"
	"scan = NUM; scan ?	set scan code set; return current set\n"
	"id ?			get two id bytes\n"
	"typematic=delay,rate	set typematic delay(ms)&rate(1/s)\n"
	"enable; defaults	enable kbd; back to defaults\n"
	"all typematic		make all keys typematic\n"
	"all make release	make all keys sending make/release\n"
	"all make only		make all keys sending make only\n"
	"all typematic make release	make all keys typematic & make/release\n"
	"NUM typematic		make specific key typematic\n"
	"NUM make release	make specific key sending make/release\n"
	"NUM make only		make specific key sending make only\n"
	"resend; reset		resend last byte from kbd; reset kbd\n"
	"cmd ?			fetch kbd ctrl command byte\n"
	"cmd = expr		set kbd ctrl command byte\n"
	"\n");
	break;

	case 1:
	printf(
	"Expressions can either consist of a number, possibly followed\n"
	"by a + or - sign and bit values in numeric or symbolic form.\n"
	"Symbolic bit values are:\n"
	"SCCONV IGNPAR CLKLOW OVRINH TEST IRQ\n"
	"\n");
	break;
	}
}

static void
status(void) {
	int c = inb(0x64);
	if(c&0x80) printf("parity error | ");
	if(c&0x40) printf("rx timeout | ");
	if(c&0x20) printf("tx timeout | ");
	if(c&0x10) printf("kbd released ");
	else       printf("kbd locked   ");
	if(c&0x08) printf("| cmd last sent  ");
	else       printf("| data last sent ");
	if(c&0x04) printf("| power-on ");
	else       printf("| test ok  ");
	if(c&0x02) printf("| ctrl write busy ");
	else       printf("| ctrl write ok   ");
	if(c&0x01) printf("| ctrl read ok\n");
	else       printf("| ctrl read empty\n");
}

/* see: Frank van Gilluwe, "The Undocumented PC", Addison Wesley 1994, pp 273 */

static void
whatMCA(void) {
	int new, sav;
	kbc(0x20);		/* get command byte */
	sav = kbget();		/* sav = command byte */
	kbc(0x60);		/* set command byte */
	kbd(sav | 0x40);	/* set keyboard xlate bit */
	kbc(0x20);		/* get keyboard command */
	new = kbget();		/* new = command byte */
	kbc(0x60);		/* set command byte */
	kbd(sav);		/* restore command byte */
	if(new & 0xbf)
		printf("Hmm - looks like MCA type 1 motherboard controller\n");
	else
		printf("Hmm - looks like MCA type 2 motherboard controller\n");
}

static void
kbd(int d) {
	int i = 100000;
	while(i && (inb(0x64) & 2)) i--;
	if(i == 0) { printf("kbd write: timed out\n"); return; }
	outb(0x60, d);
}

static void
kbc(int d) {
	int i = 100000;
	while(i && (inb(0x64) & 2)) i--;
	if(i == 0) { printf("ctrl write: timed out\n"); return; }
	outb(0x64, d);
}

static int
kbget(void) {
	int i, c;
	for(;;) {
		i = 10000;
		while(i && (inb(0x64) & 1) == 0) i--;
		if(i == 0) { printf("data read: timed out\n"); return -1; }
		KBD_DELAY
		c = (unsigned char)inb(0x60);
		switch(c) {
			case 0: case 0xff:
				printf("got kbd overrun\n"); break;
			case 0xaa:
				printf("got self-test OK\n"); break;
			case 0xee:
				printf("got ECHO byte\n"); break;
			case 0xfa:
				printf("got ACK\n"); break;
			case 0xfc:
				printf("got self-test FAIL\n"); break;
			case 0xfd:
				printf("got internal failure\n"); break;
			case 0xfe:
				printf("got RESEND request\n"); break;
			default:
				goto done;
		}
	}
done:
	return c;
}

static void
cmdbyte(int d) {
	if(d&0x40) printf("scan conv ");
	else       printf("pass thru ");
	if(d&0x20) printf("| ign parity   ");
	else       printf("| check parity ");
	if(d&0x10) printf("| kbd clk low ");
	else       printf("| enable kbd  ");
	if(d&0x08) printf("| override kbd inh ");
	if(d&0x04) printf("| test ok  ");
	else       printf("| power-on ");
	if(d&0x01) printf("| irq 1 enable\n");
	else       printf("| no irq\n");
}

static void
data(int d) {
	if(d < 0) return;
	printf("data: 0x%02x\n", d);
}

void yyerror(const char *msg) {
	fprintf(stderr, "yyerror: %s\n", msg);
}

int main(int argc, char **argv) {
	int fd;

	if(argc > 1) yydebug = 1;

	if((fd = open("/dev/console", O_RDONLY)) < 0)
		fd = 0;

	if(ioctl(fd, KDENABIO, 0) < 0) {
		perror("ioctl(KDENABIO)");
		return 1;
	}
	yyparse();

	(void)ioctl(fd, KDDISABIO, 0);
	return 0;
}

