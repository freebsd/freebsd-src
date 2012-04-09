/* $FreeBSD: src/sys/boot/arm/at91/bootspi/ee.h,v 1.1.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $ */

void EEInit(void);
void EERead(unsigned ee_off, char *data_addr, unsigned size);
void EEWrite(unsigned ee_off, const char *data_addr, unsigned size);

