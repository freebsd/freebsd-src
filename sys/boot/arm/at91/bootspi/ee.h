/* $FreeBSD: src/sys/boot/arm/at91/bootspi/ee.h,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */

void EEInit(void);
void EERead(unsigned ee_off, char *data_addr, unsigned size);
void EEWrite(unsigned ee_off, const char *data_addr, unsigned size);

