/* $FreeBSD: src/sys/boot/arm/at91/bootspi/ee.h,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $ */

void EEInit(void);
void EERead(unsigned ee_off, char *data_addr, unsigned size);
void EEWrite(unsigned ee_off, const char *data_addr, unsigned size);

