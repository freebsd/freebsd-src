/* $FreeBSD: src/sys/i386/isa/gpibreg.h,v 1.3.10.1 2000/08/03 01:01:20 peter Exp $ */

static int gpib_port=0x2c0;
#define IEEE gpib_port

/*NAT4882 Registers*/

#define	DIR 	IEEE+0
#define CDOR	IEEE+0
#define ISR1 	IEEE+2
#define IMR1    IEEE+2
#define ISR2 	IEEE+4
#define IMR2    IEEE+4
#define	SPSR	IEEE+6
#define	KSR	IEEE+0x17
#define	KCR	IEEE+0x17
#define	SPMR	IEEE+6
#define	ADSR	IEEE+8
#define	ADMR	IEEE+8
#define CPTR	IEEE+0x0A
#define SASR	IEEE+0x1B
#define AUXMR	IEEE+0x0A
#define ADR0	IEEE+0x0c
#define ISR0	IEEE+0x1d
#define IMR0	IEEE+0x1d
#define ADR	IEEE+0x0c
#define	ADR1	IEEE+0x0e
#define	BSR	IEEE+0x1f
#define	BCR	IEEE+0x1f
#define	EOSR	IEEE+0x0e


/*Turbo 488 Registers*/

#define CNT2 IEEE+0x09
#define CNT3 IEEE+0x0b
#define HSSEL IEEE+0x0d
#define STS1 IEEE+0x10
#define CFG IEEE+0x10
#define IMR3 IEEE+0x12
#define CNT0 IEEE+0x14
#define CNT1 IEEE+0x16
#define FIFOB IEEE+0x18
#define FIFOA IEEE+0x19
#define ISR3 IEEE+0x1a
#define CCRG IEEE+0x1a
#define STS2 IEEE+0x1c
#define CMDR IEEE+0x1c
#define TIMER IEEE+0x1e
#define ACCWR IEEE+0x05
#define INTR IEEE+0x07



#define	pon		0
#define	chip_reset	2
#define rhdf		3
#define	trig		4
#define rtl_pulse	5
#define rtl_off		5
#define rtl_on		0x0d
#define seoi		6
#define ist_off		1
#define ist_on		9
#define rlc		0x0a
#define rqc		8
#define lut		0x0b
#define lul		0x0c
#define nbaf		0x0e
#define gts		0x10
#define tca		0x11
#define tcs		0x12
#define tcse		0x1a
#define ltn		0x13
#define ltn_cont	0x1b
#define	lun		0x1c
#define rsc_off		0x14
#define sic_rsc		0x1e
#define sic_rsc_off	0x16
#define sre_rsc		0x1f
#define sre_rsc_off	0x17
#define	reqt		0x18
#define reqf		0x19
#define rppl		0x1d
#define hldi		0x51



