/*
 *	Interface between the low-level keyboard driver and the keymapper
 */

#ifndef _KBD_LL_H
#define _KBD_LL_H

extern struct pt_regs *kbd_pt_regs;

void handle_scancode(unsigned char scancode, int down);

#endif	/* _KBD_LL_H */
