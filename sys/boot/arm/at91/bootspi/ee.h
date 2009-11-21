/* $FreeBSD: src/sys/boot/arm/at91/bootspi/ee.h,v 1.1.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $ */

void EEInit(void);
void EERead(unsigned ee_off, char *data_addr, unsigned size);
void EEWrite(unsigned ee_off, const char *data_addr, unsigned size);

