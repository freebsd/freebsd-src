/*	$FreeBSD$ */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <i386/include/pc/display.h>
#include <i386/include/console.h>
#include <i386/include/apm_bios.h>
#include <i386/i386/cons.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/syscons.h>

extern scr_stat	*cur_console;
extern u_short	*Crtat;
extern u_int	crtc_addr;
extern char	scr_map[];
extern int	scrn_blanked;
extern char	palette[];
