/* $FreeBSD: src/sys/boot/arm/at91/bootspi/ee.h,v 1.1 2006/10/21 22:51:21 imp Exp $ */

void EEInit(void);
void EERead(unsigned ee_off, char *data_addr, unsigned size);
void EEWrite(unsigned ee_off, const char *data_addr, unsigned size);

