/*
 *  linux/include/asm-m68k/q40_keyboard.h
 *
 *  Q40 specific keyboard definitions
 */


#ifdef __KERNEL__


#include <asm/machdep.h>



extern int q40kbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int q40kbd_getkeycode(unsigned int scancode);
extern int q40kbd_pretranslate(unsigned char scancode, char raw_mode);
extern int q40kbd_translate(unsigned char scancode, unsigned char *keycode,
			   char raw_mode);
extern char q40kbd_unexpected_up(unsigned char keycode);
extern void q40kbd_leds(unsigned char leds);
extern int q40kbd_is_sysrq(unsigned char keycode);
extern int q40kbd_init_hw(void);
extern unsigned char q40kbd_sysrq_xlate[128];



#endif /* __KERNEL__ */





