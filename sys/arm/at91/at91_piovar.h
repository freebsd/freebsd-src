/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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

#ifndef ARM_AT91_AT91_PIOVAR_H
#define	ARM_AT91_AT91_PIOVAR_H

void at91_pio_use_periph_a(uint32_t pio, uint32_t periph_a_mask,
    int use_pullup);
void at91_pio_use_periph_b(uint32_t pio, uint32_t periph_b_mask,
    int use_pullup);
void at91_pio_use_gpio(uint32_t pio, uint32_t gpio_mask);
void at91_pio_gpio_input(uint32_t pio, uint32_t input_enable_mask);
void at91_pio_gpio_output(uint32_t pio, uint32_t output_enable_mask,
    int use_pullup);
void at91_pio_gpio_high_z(uint32_t pio, uint32_t high_z_mask, int enable);
void at91_pio_gpio_set(uint32_t pio, uint32_t data_mask);
void at91_pio_gpio_clear(uint32_t pio, uint32_t data_mask);
uint8_t at91_pio_gpio_get(uint32_t pio, uint32_t data_mask);
void at91_pio_gpio_set_deglitch(uint32_t pio, uint32_t data_mask,
    int use_deglitch);
void at91_pio_gpio_set_interrupt(uint32_t pio, uint32_t data_mask,
    int enable_interrupt);
uint32_t at91_pio_gpio_clear_interrupt(uint32_t pio);

#endif /* ARM_AT91_AT91_PIOVAR_H */
