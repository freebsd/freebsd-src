/* $FreeBSD: src/sys/boot/arm/at91/bootspi/ee.h,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $ */

void EEInit(void);
void EERead(unsigned ee_off, char *data_addr, unsigned size);
void EEWrite(unsigned ee_off, const char *data_addr, unsigned size);

