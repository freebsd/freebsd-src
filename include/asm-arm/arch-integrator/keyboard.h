/*
 *  linux/include/asm-arm/arch-integrator/keyboard.h
 *
 *  Copyright (C) 2000-2001 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Keyboard driver definitions for the Integrator architecture
 */
#include <asm/irq.h>

#define NR_SCANCODES 128

extern int kmi_kbd_init(void);

#define kbd_disable_irq()	disable_irq(IRQ_KMIINT0)
#define kbd_enable_irq()	enable_irq(IRQ_KMIINT0)
#define kbd_init_hw()		kmi_kbd_init()
