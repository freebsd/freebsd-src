/*
 *	$Id$ 
 *	Copyright (C) 2000 YAEGASHI Takeshi
 *	HP600 keyboard scan routine and translation table
 *	Copyright (C) 2000 Niibe Yutaka
 *	HP620 keyboard translation table
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/delay.h>
#include <asm/io.h>
#include "scan_keyb.h"

#define PCDR 0xa4000124
#define PDDR 0xa4000126
#define PEDR 0xa4000128
#define PFDR 0xa400012a
#define PGDR 0xa400012c
#define PHDR 0xa400012e
#define PJDR 0xa4000130
#define PKDR 0xa4000132
#define PLDR 0xa4000134

static const unsigned char hp620_japanese_table[] = {
	/* PTD1 */
	0x0a, 0x0b, 0x0c, 0x00, 0x00, 0x0e, 0x00, 0x00,
	0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	/* PTD5 */
	0x18, 0x19, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	/* PTD7 */
	0x26, 0x1a, 0x2b, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
	/* PTE0 */
	0x27, 0x1b, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x2a, 0x0f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* PTE1 */
	0x35, 0x28, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x70, 0x3a, 0x00, 0x00, 0x2f, 0x00, 0x00, 0x34,
	/* PTE3 */
	0x48, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x2d, 0x2e, 0x7b, 0x30, 0x31, 0x32, 0x33,
	/* PTE6 */
	0x4b, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x2c, 0x38, 0x00, 0x39, 0x79, 0x7d, 0x73,
	/* PTE7 */
	0x41, 0x42, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
	/* **** */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static const unsigned char hp680_japanese_table[] = {
	/* PTD1 */
	0x3a, 0x70, 0x29, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x3b, 0x3c, 0x3d, 0x42, 0x41, 0x40, 0x3e, 0x3f,
	/* PTD5 */
	0x35, 0x28, 0x1c, 0x00, 0x2c, 0x00, 0x00, 0x00,
	0x2d, 0x2e, 0x2f, 0x34, 0x33, 0x32, 0x30, 0x31,
	/* PTD7 */
	0x50, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x4b, 0x00, 0x00, 0x38, 0x7b,
	/* PTE0 */
	0x00, 0x00, 0x00, 0x00, 0xdb, 0x00, 0x00, 0x00,
	0x1d, 0x00, 0x39, 0x53, 0x73, 0xf9, 0x00, 0x00,
	/* PTE1 */
	0x27, 0x1b, 0x2b, 0x00, 0x1e, 0x00, 0x00, 0x00,
	0x1f, 0x20, 0x21, 0x26, 0x25, 0x24, 0x22, 0x23,
	/* PTE3 */
	0x48, 0x7d, 0x36, 0x00, 0x0f, 0x00, 0x00, 0x00,
	0x00, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* PTE6 */
	0x19, 0x1a, 0x0e, 0x00, 0x10, 0x00, 0x00, 0x00,
	0x11, 0x12, 0x13, 0x18, 0x17, 0x16, 0x14, 0x15,
	/* PTE7 */
	0x0b, 0x0c, 0x0d, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x03, 0x04, 0x05, 0x0a, 0x09, 0x08, 0x06, 0x07,
	/* **** */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static int hp620_japanese_scan_kbd(unsigned char *s)
{
	int i;
	unsigned char matrix_switch[] = {
		0xfd, 0xff,	/* PTD1 */
		0xdf, 0xff,	/* PTD5 */
		0x7f, 0xff,	/* PTD7 */
		0xff, 0xfe,	/* PTE0 */
		0xff, 0xfd,	/* PTE1 */
		0xff, 0xf7,	/* PTE3 */
		0xff, 0xbf,	/* PTE6 */
		0xff, 0x7f,	/* PTE7 */
	}, *t=matrix_switch;

	for(i=0; i<8; i++) {
		ctrl_outb(*t++, PDDR);
		ctrl_outb(*t++, PEDR);
		udelay(50);
		*s++=ctrl_inb(PCDR);
		*s++=ctrl_inb(PFDR);
	}

	ctrl_outb(0xff, PDDR);
	ctrl_outb(0xff, PEDR);

	*s++=ctrl_inb(PGDR);
	*s++=ctrl_inb(PHDR);

	return 0;
}


static int hp680_japanese_scan_kbd(unsigned char *s)
{
	int i;
	unsigned char matrix_switch[] = {
		0xfd, 0xff,	/* PTD1 */
		0xdf, 0xff,	/* PTD5 */
		0x7f, 0xff,	/* PTD7 */
		0xff, 0xfe,	/* PTE0 */
		0xff, 0xfd,	/* PTE1 */
		0xff, 0xf7,	/* PTE3 */
		0xff, 0xbf,	/* PTE6 */
		0xff, 0x7f,	/* PTE7 */
	}, *t=matrix_switch;

	for(i=0; i<8; i++) {
		ctrl_outb(*t++, PDDR);
		ctrl_outb(*t++, PEDR);
		*s++=ctrl_inb(PCDR);
		*s++=ctrl_inb(PFDR);
	}

	ctrl_outb(0xff, PDDR);
	ctrl_outb(0xff, PEDR);

	*s++=ctrl_inb(PGDR);
	*s++=ctrl_inb(PHDR);

	return 0;
}


void __init hp600_kbd_init_hw(void)
{
	scan_kbd_init();

	if (MACH_HP620)
		register_scan_keyboard(hp620_japanese_scan_kbd,
				       hp620_japanese_table, 18);
	else if (MACH_HP680 || MACH_HP690) 
		register_scan_keyboard(hp680_japanese_scan_kbd,
				       hp680_japanese_table, 18);

	printk(KERN_INFO "HP600 matrix scan keyboard registered\n");
}

/****************************************************************
HP Jornada 690(Japanese version) keyboard scan matrix

	PTC7	PTC6	PTC5	PTC4	PTC3	PTC2	PTC1	PTC0	
PTD1	REC			Escape	on/off	Han/Zen	Hira	Eisu	
PTD5	REC			Z	on/off	Enter	:	/	
PTD7	REC						Right	Down	
PTE0	REC			Windows	on/off				
PTE1	REC			A	on/off	]	[	;	
PTE3	REC			Tab	on/off	ShirtR	\	Up	
PTE6	REC			Q	on/off	BS	@	P	
PTE7	REC			1	on/off	^	-	0	

	PTF7	PTF6	PTF5	PTF4	PTF3	PTF2	PTF1	PTF0
PTD1	F5	F4	F6	F7	F8	F3	F2	F1	
PTD5	N	B	M	,	.	V	C	X	
PTD7	Muhen	Alt			Left				
PTE0			Henkan	_	Del	Space		Ctrl	
PTE1	H	G	J	K	L	F	D	S	
PTE3							ShiftL		
PTE6	Y	T	U	I	O	R	E	W	
PTE7	6	5	7	8	9	4	3	2	

	PTG5	PTG4	PTG3	PTG0	PTH0
*	REC	REW	FWW	Cover	on/off


		7	6	5	4	3	2	1	0
C: 0xffff 0xdf	IP	IP	IP	IP	IP	IP	IP	IP
D: 0x6786 0x59	O	I	O	IP	I	F	O	I
E: 0x5045 0x00	O	O	F	F	O	F	O	O
F: 0xffff 0xff	IP	IP	IP	IP	IP	IP	IP	IP
G: 0xaffe 0xfd	I	I	IP	IP	IP	IP	IP	I
H: 0x70f2 0x49	O	IP	F	F	IP	IP	F	I
J: 0x0704 0x22	F	F	O	IP	F	F	O	F
K: 0x0100 0x10	F	F	F	O	F	F	F	F
L: 0x0c3c 0x26	F	F	IP	F	F	IP	IP	F

****************************************************************/

/****************************************************************
HP Jornada 620(Japanese version) keyboard scan matrix

	PTC7	PTC6	PTC5	PTC4	PTC3	PTC2	PTC1	PTC0
PTD1	EREC		BS	Ctrl	on/off	-	0	9
PTD5	EREC		BS	Ctrl	on/off	^	P	O
PTD7	EREC		BS	Ctrl	on/off	]	@	L
PTE0	EREC		BS	Ctrl	on/off	Han/Zen [	;
PTE1	EREC		BS	Ctrl	on/off	Enter	:	/
PTE3	EREC		BS	Ctrl	on/off		Right	Up
PTE6	EREC		BS	Ctrl	on/off		Down	Left
PTE7	EREC		BS	Ctrl	on/off		F8	F7

	PTF7	PTF6	PTF5	PTF4	PTF3	PTF2	PTF1	PTF0
PTD1	8	7	6	5	4	3	2	1
PTD5	I	U	Y	T	R	E	W	Q
PTD7	K	J	H	G	F	D	S	A
PTE0						ESC	Tab	Shift
PTE1	.			V			Caps	Hira
PTE3	,	M	N	B	Muhen	C	X
PTE6	_	\	Henkan	Space		Alt	Z
PTE7	F6	F5	F4	F3	F2	F1		REC

	PTH0
*	on/off

		7	6	5	4	3	2	1	0
C: 0xffff 0xff	IP	IP	IP	IP	IP	IP	IP	IP
D: 0x4404 0xaf	O	F	O	F	F	F	O	F
E: 0x5045 0xff	O	O	F	F	O	F	O	O
F: 0xffff 0xff	IP	IP	IP	IP	IP	IP	IP	IP
G: 0xd5ff 0x00	IP	O	O	O	IP	IP	IP	IP
H: 0x63ff 0xd1	O	I	F	IP	IP	IP	IP	IP
J: 0x0004 0x02	F	F	F	F	F	F	O	F
K: 0x0401 0xff	F	F	O	F	F	F	F	O
L: 0x0c00 0x20	F	F	IP	F	F	F	F	F

ADCSR: 0x08
ADCR: 0x3f

 ****************************************************************/

/****************************************************************
Japanese 109 keyboard scan code layout

                                              E02A-     E1- 
01    3B 3C 3D 3E  3F 40 41 42  43 44 57 58   E037  46  1045
                                                                   
29 02 03 04 05 06 07 08 09 0A 0B 0C 0D 7D 0E  E052 E047 E049   45 E035 37  4A
0F  10 11 12 13 14 15 16 17 18 19 1A 1B   1C  E053 E04F E051   47  48  49  4E
3A   1E 1F 20 21 22 23 24 25 26 27 28 2B                       4B  4C  4D
2A    2C 2D 2E 2F 30 31 32 33 34 35 73    36       E048        4F  50  51  E0-
1D  DB  38  7B   39   79 70  E038 DC DD E01D  E04B E050 E04D     52    53  1C

****************************************************************/

#if 0
int __init hp620_keyboard_test(void)
{
	int i;
	unsigned char s[18];
	unsigned long a, b, c, d;

	printk("PCCR: %04lx, PCDR: %02lx\n",
	       ctrl_inw(0xa4000104), ctrl_inb(0xa4000124));
	printk("PDCR: %04lx, PDDR: %02lx\n",
	       ctrl_inw(0xa4000106), ctrl_inb(0xa4000126));
	printk("PECR: %04lx, PEDR: %02lx\n",
	       ctrl_inw(0xa4000108), ctrl_inb(0xa4000128));
	printk("PFCR: %04lx, PFDR: %02lx\n",
	       ctrl_inw(0xa400010a), ctrl_inb(0xa400012a));
	printk("PGCR: %04lx, PGDR: %02lx\n",
	       ctrl_inw(0xa400010c), ctrl_inb(0xa400012c));
	printk("PHCR: %04lx, PHDR: %02lx\n",
	       ctrl_inw(0xa400010e), ctrl_inb(0xa400012e));
	printk("PJCR: %04lx, PJDR: %02lx\n",
	       ctrl_inw(0xa4000110), ctrl_inb(0xa4000130));
	printk("PKCR: %04lx, PKDR: %02lx\n",
	       ctrl_inw(0xa4000112), ctrl_inb(0xa4000132));
	printk("PLCR: %04lx, PLDR: %02lx\n",
	       ctrl_inw(0xa4000114), ctrl_inb(0xa4000134));

	printk("ADCSR: %02lx, ADCR: %02lx\n",
	       ctrl_inb(0xa4000090), ctrl_inb(0xa4000092));

	ctrl_inb(0xa4000004);
	ctrl_inb(0xa4000006);
	ctrl_inb(0xa4000008);
	ctrl_outb(0, 0xa4000004);
	ctrl_outb(0, 0xa4000006);
	ctrl_outb(0, 0xa4000008);
	ctrl_outb(0, 0xa4000090);
	ctrl_outb(0x3b, 0xa4000090);

	while(1) {
		hp620_japanese_scan_kbd(s);
		for(i=0; i<18; i+=2)
			printk("%02x%02x ", s[i], s[i+1]);

#if 0
		ctrl_outb(~2, PJDR);
		printk("%02lx%02lx ", ctrl_inb(PCDR), ctrl_inb(PFDR));
		ctrl_outb(0xff, PJDR);
		ctrl_outb(~1, PKDR);
		printk("%02lx%02lx ", ctrl_inb(PCDR), ctrl_inb(PFDR));
		ctrl_outb(~32, PKDR);
		printk("%02lx%02lx ", ctrl_inb(PCDR), ctrl_inb(PFDR));
		ctrl_outb(0xff, PKDR);
#endif

		printk("%02lx%02lx%02lx%02lx ", a, b, c, d);
		if(ctrl_inb(0xa4000090)&0x80) {
			a=ctrl_inb(0xa4000080);
			b=ctrl_inb(0xa4000084);
			c=ctrl_inb(0xa4000088);
			d=ctrl_inb(0xa400008c);
			ctrl_outb(0x3b, 0xa4000090);
		}
		printk("%02lx%02lx%02lx ",
		       ctrl_inb(0xa4000004),
		       ctrl_inb(0xa4000006),
		       ctrl_inb(0xa4000008));

		printk("\n");
	}

	return 0;
}
module_init(keyboard_probe);
#endif


MODULE_LICENSE("GPL");
