/*
 * PreP compliant NVRAM access
 */

#ifdef __KERNEL__
#ifndef _PPC_NVRAM_H
#define _PPC_NVRAM_H

#define NVRAM_AS0  0x74
#define NVRAM_AS1  0x75
#define NVRAM_DATA 0x77


/* RTC Offsets */

#define MOTO_RTC_SECONDS		0x1FF9
#define MOTO_RTC_MINUTES		0x1FFA
#define MOTO_RTC_HOURS		0x1FFB
#define MOTO_RTC_DAY_OF_WEEK		0x1FFC
#define MOTO_RTC_DAY_OF_MONTH	0x1FFD
#define MOTO_RTC_MONTH		0x1FFE
#define MOTO_RTC_YEAR		0x1FFF
#define MOTO_RTC_CONTROLA            0x1FF8
#define MOTO_RTC_CONTROLB            0x1FF9

#ifndef BCD_TO_BIN
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#endif

#ifndef BIN_TO_BCD
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)
#endif

/* PowerMac specific nvram stuffs */

enum {
	pmac_nvram_OF,		/* Open Firmware partition */
	pmac_nvram_XPRAM,	/* MacOS XPRAM partition */
	pmac_nvram_NR		/* MacOS Name Registry partition */
};

/* Return partition offset in nvram */
extern int	pmac_get_partition(int partition);

/* Direct access to XPRAM */
extern u8	pmac_xpram_read(int xpaddr);
extern void	pmac_xpram_write(int xpaddr, u8 data);

/* Some offsets in XPRAM */
#define PMAC_XPRAM_MACHINE_LOC	0xe4
#define PMAC_XPRAM_SOUND_VOLUME	0x08

/* Machine location structure in XPRAM */
struct pmac_machine_location {
	unsigned int	latitude;	/* 2+30 bit Fractional number */
	unsigned int	longitude;	/* 2+30 bit Fractional number */
	unsigned int	delta;		/* mix of GMT delta and DLS */
};

/* /dev/nvram ioctls */
#define PMAC_NVRAM_GET_OFFSET	_IOWR('p', 0x40, int) /* Get NVRAM partition offset */

#endif
#endif /* __KERNEL__ */
