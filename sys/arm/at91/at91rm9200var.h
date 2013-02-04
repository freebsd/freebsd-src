/*-
 * Copyright (c) 2012 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef ARM_AT91_AT91RM9200VAR_H
#define ARM_AT91_AT91RM9200VAR_H

void at91rm9200_set_subtype(enum at91_soc_subtype st);

#define AT91RM9200_ID_USART0	1
#define AT91RM9200_ID_USART1	2
#define AT91RM9200_ID_USART2	3
#define AT91RM9200_ID_USART3	4

/*
 * Serial port convenience routines
 */
/* uart pins that are wired... */
#define	AT91_UART_CTS	0x01
#define	AT91_UART_RTS	0x02
#define	AT91_UART_RI    0x04
#define	AT91_UART_DTR	0x08
#define AT91_UART_DCD	0x10
#define	AT91_UART_DSR	0x20

#define AT91_ID_DBGU	0

void at91rm9200_config_uart(unsigned devid, unsigned unit, unsigned pinmask);

/*
 * MCI (sd/mmc card support)
 */
void at91rm9200_config_mci(int has_4wire);

#endif /* ARM_AT91_AT91RM9200VAR_H */
