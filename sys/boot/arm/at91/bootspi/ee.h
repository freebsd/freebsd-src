/* $FreeBSD: src/sys/boot/arm/at91/bootspi/ee.h,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $ */

void EEInit(void);
void EERead(unsigned ee_off, char *data_addr, unsigned size);
void EEWrite(unsigned ee_off, const char *data_addr, unsigned size);

