#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/keyboard.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/kd.h>
#include <linux/kbd_ll.h>
#include <linux/random.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/init.h>

#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/apollohw.h>
#include <asm/uaccess.h>

#include "busmouse.h"

#define DNKEY_CAPS 0x7e
#define BREAK_FLAG 0x80
#define DNKEY_REPEAT_DELAY 50
#define DNKEY_CTRL 0x43
#define DNKEY_LSHIFT 0x5e
#define DNKEY_RSHIFT 0x6a
#define DNKEY_REPT 0x5d
#define DNKEY_REPEAT 0x7f
#define DNKEY_LALT 0x75
#define DNKEY_RALT 0x77

#define APOLLO_KEYB_CMD_ENTRIES 16
#define APOLLO_KBD_MODE_KEYB   0x01
#define APOLLO_KBD_MODE_MOUSE   0x02
#define APOLLO_KBD_MODE_CHANGE 0xff

static u_char keyb_cmds[APOLLO_KEYB_CMD_ENTRIES];
static short keyb_cmd_read=0, keyb_cmd_write=0;
static int keyb_cmd_transmit=0;
static int msedev;

static unsigned int kbd_mode=APOLLO_KBD_MODE_KEYB;

#if 0
static void debug_keyb_timer_handler(unsigned long ignored);
static u_char debug_buf1[4096],debug_buf2[4096],*debug_buf=&debug_buf1[0];
static u_char *shadow_buf=&debug_buf2[0];
static short debug_buf_count=0;
static int debug_buf_overrun=0,debug_timer_running=0;
static unsigned long debug_buffer_updated=0;
static struct timer_list debug_keyb_timer = { function: debug_keyb_timer_handler };
#endif

static u_short dnplain_map[NR_KEYS] __initdata = {
/*         ins     del     del     F1      F2      F3      F4  
           mark    line    char                                 */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
/* F5      F6      F7      F8      F9      F0      Again   Read */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
/* Edit    Exit    Hold    Copy    Paste   Grow            ESC  */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf01b,
/* 1       2       3       4       5       6       7       8    */
   0xf031, 0xf032, 0xf033, 0xf034, 0xf035, 0xf036, 0xf037, 0xf038,
/* 9       0       -       =       `       Back            |<--
                                           Space                */
   0xf039, 0xf030, 0xf02d, 0xf03d, 0xf060, 0xf07f, 0xf200, 0xf200,
/* Shell   -->|                    Tab     q       w       e
   Cmd                                                          */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf009, 0xfb71, 0xfb77, 0xfb65,
/* r       t       y       u       i       o       p       [    */
   0xfb72, 0xfb74, 0xfb79, 0xfb75, 0xfb69, 0xfb6f, 0xfb70, 0xf05b,
/* ]               Del             7       8       9       +    */
   0xf05d, 0xf200, 0xf200, 0xf200, 0xf307, 0xf308, 0xf300, 0xf30a,
/* [<--]   Up      [-->]   Ctrl                    a       s    */
   0xf200, 0xf600, 0xf200, 0xf702, 0xf200, 0xf200, 0xfb61, 0xfb73,
/* d       f       g       h       j       k       l       ;    */
   0xfb64, 0xfb66, 0xfb67, 0xfb68, 0xfb6a, 0xfb6b, 0xfb6c, 0xf03b,
/* '               Return  \               4       5       6    */
   0xf027, 0xf200, 0xf201, 0xf05c, 0xf200, 0xf304, 0xf305, 0xf306,
/* -       <--     Next    -->             Rept    Shift        
                   Window                                       */
   0xf30b, 0xf601, 0xf200, 0xf602, 0xf200, 0xf200, 0xf700, 0xf200,
/* z       x       c       v       b       n       m       ,    */
   0xfb7a, 0xfb78, 0xfb63, 0xfb76, 0xfb62, 0xfb6e, 0xfb6d, 0xf02c,
/* .       /       Shift           Pop             1       2    */
   0xf02e, 0xf02f, 0xf700, 0xf200, 0xf200, 0xf200, 0xf301, 0xf302,
/* 3               PgUp    Down    PgDn    Alt     Space   Alt  */
   0xf303, 0xf200, 0xf118, 0xf603, 0xf119, 0xf703, 0xf020, 0xf701,
/*         0               .       Enter                        */
   0xf200, 0xf300, 0xf200, 0xf310, 0xf30e, 0xf200, 0xf700, 0xf200,
};

static u_short dnshift_map[NR_KEYS] __initdata = {
/*         ins     del     del     F1      F2      F3      F4
           mark    line    char                                 */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
/* F5      F6      F7      F8      F9      F0      Again   Read */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
/* Save    Abort   Help    Cut     Undo    Grow            ESC  */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf01b,
/* !       @       #       $       %       ^       &       *    */
   0xf021, 0xf040, 0xf023, 0xf024, 0xf025, 0xf05e, 0xf026, 0xf02a,
/* (       )       _       +       ~       Back            |<--
                                           Space                */
   0xf028, 0xf029, 0xf05f, 0xf02b, 0xf07e, 0xf07f, 0xf200, 0xf200,
/* Shell   -->|                    Tab     Q       W       E
   Cmd                                                          */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf009, 0xfb51, 0xfb57, 0xfb45,
/* R       T       Y       U       I       O       P       {    */
   0xfb52, 0xfb54, 0xfb59, 0xfb55, 0xfb49, 0xfb4f, 0xfb50, 0xf07b,
/* }               Del             7       8       9       +    */
   0xf07d, 0xf200, 0xf200, 0xf200, 0xf307, 0xf308, 0xf300, 0xf30a,
/* [<--]   Up      [-->]   Ctrl                    A       S    */
   0xf200, 0xf600, 0xf200, 0xf702, 0xf200, 0xf200, 0xfb41, 0xfb53,
/* D       F       G       H       J       K       L       :    */
   0xfb44, 0xfb46, 0xfb47, 0xfb48, 0xfb4a, 0xfb4b, 0xfb4c, 0xf03a,
/* "               Return  |               4       5       6    */
   0xf022, 0xf200, 0xf201, 0xf07c, 0xf200, 0xf304, 0xf305, 0xf306,
/* -       <--     Next    -->             Rept    Shift        
                   Window                                       */
   0xf30b, 0xf601, 0xf200, 0xf602, 0xf200, 0xf200, 0xf700, 0xf200,
/* Z       X       C       V       B       N       M       <    */
   0xfb5a, 0xfb58, 0xfb43, 0xfb56, 0xfb42, 0xfb4e, 0xfb4d, 0xf03c,
/* >       ?       Shift           Pop             1       2    */
   0xf03e, 0xf03f, 0xf700, 0xf200, 0xf200, 0xf200, 0xf301, 0xf302,
/* 3               PgUp    Down    PgDn    Alt     Space   Alt  */
   0xf303, 0xf200, 0xf118, 0xf603, 0xf119, 0xf703, 0xf020, 0xf701,
/*         0               .       Enter                        */
   0xf200, 0xf300, 0xf200, 0xf310, 0xf30e, 0xf200, 0xf708, 0xf200,
};

static u_short dnctrl_map[NR_KEYS] __initdata = {
/*         ins     del     del     F1      F2      F3      F4
           mark    line    char                                 */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
/* F5      F6      F7      F8      F9      F0      Again   Read */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
/* Save    Abort   Help    Cut     Undo    Grow            ESC  */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf01b,
/* !       @       #       $       %       ^       &       *    */
   0xf200, 0xf000, 0xf01b, 0xf01c, 0xf01d, 0xf01e, 0xf01f, 0xf07f,
/* (       )       _       +       ~       Back            |<--
                                           Space                */
   0xf200, 0xf200, 0xf01f, 0xf200, 0xf01c, 0xf200, 0xf200, 0xf200,
/* Shell   -->|                    Tab     Q       W       E
   Cmd                                                          */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf009, 0xf011, 0xf017, 0xf005,
/* R       T       Y       U       I       O       P       {    */
   0xf012, 0xf014, 0xf019, 0xf015, 0xf009, 0xf00f, 0xf010, 0xf01b,
/* }               Del             7       8       9       +    */
   0xf01d, 0xf200, 0xf200, 0xf200, 0xf307, 0xf308, 0xf300, 0xf30a,
/* [<--]   Up      [-->]   Ctrl                    A       S    */
   0xf200, 0xf600, 0xf200, 0xf702, 0xf200, 0xf200, 0xfb01, 0xfb53,
/* D       F       G       H       J       K       L       :    */
   0xf004, 0xf006, 0xf007, 0xf008, 0xf00a, 0xf00b, 0xf00c, 0xf200,
/* "               Return  |               4       5       6    */
   0xf200, 0xf200, 0xf201, 0xf01c, 0xf200, 0xf304, 0xf305, 0xf306,
/* -       <--     Next    -->             Rept    Shift        
                   Window                                       */
   0xf30b, 0xf601, 0xf200, 0xf602, 0xf200, 0xf200, 0xf704, 0xf200,
/* Z       X       C       V       B       N       M       <    */
   0xf01a, 0xf018, 0xf003, 0xf016, 0xf002, 0xf00e, 0xf01d, 0xf03c,
/* >       ?       Shift           Pop             1       2    */
   0xf03e, 0xf03f, 0xf705, 0xf200, 0xf200, 0xf200, 0xf301, 0xf302,
/* 3               PgUp    Down    PgDn    Alt     Space   Alt  */
   0xf303, 0xf200, 0xf118, 0xf603, 0xf119, 0xf703, 0xf020, 0xf701,
/*         0               .       Enter                        */
   0xf200, 0xf300, 0xf200, 0xf310, 0xf30e, 0xf200, 0xf200, 0xf200,
};

static u_short dnalt_map[NR_KEYS] __initdata = {
/*         ins     del     del     F1      F2      F3      F4  
           mark    line    char                                 */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf500, 0xf501, 0xf502, 0xf503,
/* F5      F6      F7      F8      F9      F0      Again   Read */
   0xf504, 0xf505, 0xf506, 0xf507, 0xf508, 0xf509, 0xf200, 0xf200,
/* Edit    Exit    Hold    Copy    Paste   Grow            ESC  */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf01b,
/* 1       2       3       4       5       6       7       8    */
   0xf831, 0xf832, 0xf833, 0xf834, 0xf835, 0xf836, 0xf837, 0xf838,
/* 9       0       -       =       `       Back            |<--
                                           Space                */
   0xf839, 0xf830, 0xf82d, 0xf83d, 0xf860, 0xf87f, 0xf200, 0xf200,
/* Shell   -->|                    Tab     q       w       e
   Cmd                                                          */
   0xf200, 0xf200, 0xf200, 0xf200, 0xf809, 0xf871, 0xf877, 0xf865,
/* r       t       y       u       i       o       p       [    */
   0xf872, 0xf874, 0xf879, 0xf875, 0xf869, 0xf86f, 0xf870, 0xf85b,
/* ]               Del             7       8       9       +    */
   0xf05d, 0xf200, 0xf200, 0xf200, 0xf307, 0xf308, 0xf300, 0xf30a,
/* [<--]   Up      [-->]   Ctrl                    a       s    */
   0xf200, 0xf600, 0xf200, 0xf702, 0xf200, 0xf200, 0xf861, 0xf873,
/* d       f       g       h       j       k       l       ;    */
   0xf864, 0xf866, 0xf867, 0xf868, 0xf86a, 0xf86b, 0xf86c, 0xf03b,
/* '               Return  \               4       5       6    */
   0xf027, 0xf200, 0xf201, 0xf05c, 0xf200, 0xf304, 0xf305, 0xf306,
/* -       <--     Next    -->             Rept    Shift        
                   Window                                       */
   0xf30b, 0xf601, 0xf200, 0xf602, 0xf200, 0xf200, 0xf704, 0xf200,
/* z       x       c       v       b       n       m       ,    */
   0xf87a, 0xf878, 0xf863, 0xf876, 0xf862, 0xf86e, 0xf86d, 0xf82c,
/* .       /       Shift           Pop             1       2    */
   0xf82e, 0xf82f, 0xf705, 0xf200, 0xf200, 0xf200, 0xf301, 0xf302,
/* 3               PgUp    Down    PgDn    Alt     Space   Alt  */
   0xf303, 0xf200, 0xf118, 0xf603, 0xf119, 0xf703, 0xf820, 0xf701,
/*         0               .       Enter                        */
   0xf200, 0xf300, 0xf200, 0xf310, 0xf30e, 0xf200, 0xf200, 0xf200,
};

static u_short dnaltgr_map[NR_KEYS] __initdata = {
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200
};

static u_short dnshift_ctrl_map[NR_KEYS] __initdata = {
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200
};

static u_short dnctrl_alt_map[NR_KEYS] __initdata = {
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
   0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200
};

#if 0
static void debug_keyb_timer_handler(unsigned long ignored) {

	unsigned long flags;
	u_char *swap;
	short length,i;

	if (time_after(jiffies, debug_buffer_updated + 100)) {
		save_flags(flags);
		cli();
		length=debug_buf_count;		
		swap=debug_buf;	
		debug_buf=shadow_buf;
		shadow_buf=swap;
		debug_buf_count=0;
		debug_timer_running=0;
		restore_flags(flags);
		for(i=1;length;length--,i++)	
			printk("%02x%c",*(swap++), (i % 25) ? ' ' : '\n');
		printk("\n");
	}
	else {
		debug_keyb_timer.expires=jiffies+10;
		add_timer(&debug_keyb_timer);
	}
}
#endif

static void dn_keyb_process_key_event(unsigned char scancode) {

	static unsigned char lastscancode;
	unsigned char prev_scancode=lastscancode;
	static unsigned int lastkeypress;
	
	lastscancode=scancode;

	/*  printk("scan: %02x, lastscan: %02X, prev_scancode: %02X\n",scancode,lastscancode,prev_scancode); */

	if(prev_scancode==APOLLO_KBD_MODE_CHANGE) {
		kbd_mode=scancode;
/*		printk("modechange: %d\n",scancode); */
	}
	else if((scancode & (~BREAK_FLAG)) == DNKEY_CAPS) {
    	/* printk("handle_scancode: %02x\n",DNKEY_CAPS); */
		handle_scancode(DNKEY_CAPS, 1);
		/*    printk("handle_scancode: %02x\n",BREAK_FLAG | DNKEY_CAPS); */
		handle_scancode(DNKEY_CAPS, 0);
	}
	else if( (scancode == DNKEY_REPEAT) && (prev_scancode < 0x7e) &&
   			!(prev_scancode==DNKEY_CTRL || prev_scancode==DNKEY_LSHIFT ||
       	   	prev_scancode==DNKEY_RSHIFT || prev_scancode==DNKEY_REPT ||
       	  	prev_scancode==DNKEY_LALT || prev_scancode==DNKEY_RALT)) {
			if (time_after(jiffies, lastkeypress + DNKEY_REPEAT_DELAY)) {
			/*    	printk("handle_scancode: %02x\n",prev_scancode); */
           			handle_scancode(prev_scancode, 1);
			  	}
	   			lastscancode=prev_scancode;
  			}
  	else {
	/*    	printk("handle_scancode: %02x\n",scancode);  */
   			handle_scancode(scancode & ~BREAK_FLAG, !(scancode & BREAK_FLAG));
   			lastkeypress=jiffies;
  	}
}

static void dn_keyb_process_mouse_event(unsigned char mouse_data) {

	static short mouse_byte_count=0;
	static u_char mouse_packet[3];
	short buttons;
	int dx, dy;

	mouse_packet[mouse_byte_count++]=mouse_data;

	if(mouse_byte_count==3) {
		if(mouse_packet[0]==APOLLO_KBD_MODE_CHANGE) {
			kbd_mode=mouse_packet[1];
			mouse_byte_count=0;
/*			printk("modechange: %d\n",mouse_packet[1]); */
			if(kbd_mode==APOLLO_KBD_MODE_KEYB)
				dn_keyb_process_key_event(mouse_packet[2]);
		}
		if((mouse_packet[0] & 0x8f) == 0x80) {
			buttons = (mouse_packet[0] >> 4) & 0x7;
			dx = mouse_packet[1] == 0xff ? 0 : (signed char)mouse_packet[1];
			dy = mouse_packet[2] == 0xff ? 0 : (signed char)mouse_packet[2];
			busmouse_add_movementbuttons(msedev, dx, dy, buttons);
			mouse_byte_count=0;
/*			printk("mouse: %d, %d, %x\n",mouse_x,mouse_y,buttons); */
		}
	}
}

static void dn_keyb_int(int irq, void *dummy, struct pt_regs *fp) {

	unsigned char data;
  	unsigned long flags;
  	int scn2681_ints;

	do {
		scn2681_ints=sio01.isr_imr & 3;
		if(scn2681_ints & 2) {
			data=sio01.rhra_thra;
#if 0
			if(debug_buf_count<4096) {
				debug_buf[debug_buf_count++]=data;
				debug_buffer_updated=jiffies;	
				if(!debug_timer_running) {
					debug_keyb_timer.expires=jiffies+10;
					add_timer(&debug_keyb_timer);
					debug_timer_running=1;
				}
			}
			else
				debug_buf_overrun=1;
#endif
			if(sio01.sra_csra & 0x10) {
				printk("whaa overrun !\n");
				continue;
			}

			if(kbd_mode==APOLLO_KBD_MODE_KEYB)
				dn_keyb_process_key_event(data);
			else
				dn_keyb_process_mouse_event(data);
		}
	
		if(scn2681_ints & 1) {
			save_flags(flags);
			cli();
			if(keyb_cmd_write!=keyb_cmd_read) {
				sio01.rhra_thra=keyb_cmds[keyb_cmd_read++];
				if(keyb_cmd_read==APOLLO_KEYB_CMD_ENTRIES)
					keyb_cmd_read=0;
				keyb_cmd_transmit=1;
			}
			else {
				keyb_cmd_transmit=0;
				sio01.BRGtest_cra=9;
			}
			restore_flags(flags);
		}
	} while(scn2681_ints) ;
}

void write_keyb_cmd(u_short length, u_char *cmd) {

  	unsigned long flags;

	if((keyb_cmd_write==keyb_cmd_read) && keyb_cmd_transmit)
		return;

	save_flags(flags);
	cli();
	for(;length;length--) {
		keyb_cmds[keyb_cmd_write++]=*(cmd++);
		if(keyb_cmd_write==keyb_cmd_read)
			return;
		if(keyb_cmd_write==APOLLO_KEYB_CMD_ENTRIES)
			keyb_cmd_write=0;
	}
	if(!keyb_cmd_transmit)  {
 	   sio01.BRGtest_cra=5;
	}
	restore_flags(flags);

}

static struct busmouse apollo_mouse = {
        APOLLO_MOUSE_MINOR, "apollomouse", THIS_MODULE, NULL, NULL, 7
};

int __init dn_keyb_init(void){

/*  printk("dn_keyb_init\n"); */

  memcpy(key_maps[0], dnplain_map, sizeof(plain_map));
  memcpy(key_maps[1], dnshift_map, sizeof(plain_map));
  memcpy(key_maps[2], dnaltgr_map, sizeof(plain_map));
  memcpy(key_maps[4], dnctrl_map, sizeof(plain_map));
  memcpy(key_maps[5], dnshift_ctrl_map, sizeof(plain_map));
  memcpy(key_maps[8], dnalt_map, sizeof(plain_map));
  memcpy(key_maps[12], dnctrl_alt_map, sizeof(plain_map));


  msedev=register_busmouse(&apollo_mouse);
  if (msedev < 0)
      printk(KERN_WARNING "Unable to install Apollo mouse driver.\n");
   else
      printk(KERN_INFO "Apollo mouse installed.\n");

  /* program UpDownMode */

  while(!(sio01.sra_csra & 0x4));
  sio01.rhra_thra=0xff;

  while(!(sio01.sra_csra & 0x4));
  sio01.rhra_thra=0x1;

  request_irq(1, dn_keyb_int,0,NULL,NULL);
  
  /* enable receive int on DUART */
  sio01.isr_imr=3;

  return 0;

}

int dn_dummy_kbdrate(struct kbd_repeat *k) {

  printk("dn_dummy_kbdrate\n");

  return 0;

}
