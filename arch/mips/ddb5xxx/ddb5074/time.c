/*
 *  arch/mips/ddb5074/time.c -- Timer routines
 *
 *  Copyright (C) 2000 Geert Uytterhoeven <geert@sonycom.com>
 *                     Sony Software Development Center Europe (SDCE), Brussels
 *
 */
#include <linux/init.h>
#include <asm/mc146818rtc.h>
#include <asm/ddb5xxx/ddb5074.h>
#include <asm/ddb5xxx/ddb5xxx.h>


static unsigned char ddb_rtc_read_data(unsigned long addr)
{
	return *(volatile unsigned char *)(KSEG1ADDR(DDB_PCI_MEM_BASE)+addr);
}

static void ddb_rtc_write_data(unsigned char data, unsigned long addr)
{
 	*(volatile unsigned char *)(KSEG1ADDR(DDB_PCI_MEM_BASE)+addr)=data;
}

static int ddb_rtc_bcd_mode(void)
{
	return 1;
}

struct rtc_ops ddb_rtc_ops = {
	ddb_rtc_read_data,
	ddb_rtc_write_data,
	ddb_rtc_bcd_mode
};
