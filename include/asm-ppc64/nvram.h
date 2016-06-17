/*
 * PreP compliant NVRAM access
 * This needs to be updated for PPC64
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _PPC64_NVRAM_H
#define _PPC64_NVRAM_H

#define NVRW_CNT 0x20
#define NVRAM_HEADER_LEN 16 /* sizeof(struct nvram_header) */
#define NVRAM_BLOCK_LEN 16
#define NVRAM_MAX_REQ (2080/NVRAM_BLOCK_LEN)
#define NVRAM_MIN_REQ (1056/NVRAM_BLOCK_LEN)

#define NVRAM_AS0  0x74
#define NVRAM_AS1  0x75
#define NVRAM_DATA 0x77


/* RTC Offsets */

#define MOTO_RTC_SECONDS	0x1FF9
#define MOTO_RTC_MINUTES	0x1FFA
#define MOTO_RTC_HOURS		0x1FFB
#define MOTO_RTC_DAY_OF_WEEK	0x1FFC
#define MOTO_RTC_DAY_OF_MONTH	0x1FFD
#define MOTO_RTC_MONTH		0x1FFE
#define MOTO_RTC_YEAR		0x1FFF
#define MOTO_RTC_CONTROLA       0x1FF8
#define MOTO_RTC_CONTROLB       0x1FF9

#ifndef BCD_TO_BIN
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#endif

#ifndef BIN_TO_BCD
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)
#endif

#define NVRAM_SIG_SP	0x02	/* support processor */
#define NVRAM_SIG_OF	0x50	/* open firmware config */
#define NVRAM_SIG_FW	0x51	/* general firmware */
#define NVRAM_SIG_HW	0x52	/* hardware (VPD) */
#define NVRAM_SIG_SYS	0x70	/* system env vars */
#define NVRAM_SIG_CFG	0x71	/* config data */
#define NVRAM_SIG_ELOG	0x72	/* error log */
#define NVRAM_SIG_VEND	0x7e	/* vendor defined */
#define NVRAM_SIG_FREE	0x7f	/* Free space */
#define NVRAM_SIG_OS	0xa0	/* OS defined */

/* If change this size, then change the size of NVRAM_HEADER_LEN */
struct nvram_header {
	unsigned char signature;
	unsigned char checksum;
	unsigned short length;
	char name[12];
};

struct nvram_partition {
	struct list_head partition;
	struct nvram_header header;
	unsigned int index;
};


ssize_t read_nvram(char *buf, size_t count, loff_t *index);
ssize_t write_nvram(char *buf, size_t count, loff_t *index);
int write_error_log_nvram(char * buff, int length, unsigned int err_type);
int read_error_log_nvram(char * buff, int length, unsigned int * err_type);
int clear_error_log_nvram(void);
void print_nvram_partitions(char * label);

#endif /* _PPC64_NVRAM_H */
