# Makefile for ncurses
# $Id: Makefile,v 1.17 1996/08/26 09:25:20 peter Exp $

LIB=    ncurses
SHLIB_MAJOR= 3
SHLIB_MINOR= 0
SRCS=   lib_kernel.c lib_pad.c lib_bkgd.c \
	lib_unctrl.c lib_raw.c lib_vidattr.c lib_trace.c lib_beep.c \
	lib_doupdate.c lib_refresh.c lib_initscr.c lib_newwin.c lib_addch.c \
	lib_addstr.c lib_scroll.c lib_clreol.c lib_touch.c lib_mvcur.c lib_keyname.c\
	lib_delwin.c lib_endwin.c lib_clrbot.c lib_move.c lib_printw.c \
	lib_scanw.c lib_erase.c lib_getch.c lib_options.c lib_acs.c lib_slk.c\
	lib_box.c lib_clear.c lib_delch.c lib_insch.c \
	lib_getstr.c lib_mvwin.c lib_longname.c lib_tstp.c \
	lib_newterm.c lib_set_term.c lib_overlay.c lib_scrreg.c lib_color.c \
	lib_insstr.c lib_insdel.c lib_twait.c lib_window.c copyright.c

CFLAGS+= -I. -I${.CURDIR} -Wall -DMYTINFO
LDADD+=  -lmytinfo

CLEANFILES+= lib_keyname.c keys.tries

beforedepend: keys.tries

beforeinstall:
	${INSTALL} -C -m 444 -o $(BINOWN) -g $(BINGRP) ${.CURDIR}/unctrl.h \
		${DESTDIR}/usr/include
	${INSTALL} -C -m 444 -o $(BINOWN) -g $(BINGRP) ${.CURDIR}/curses.h \
		${DESTDIR}/usr/include/ncurses.h

keys.tries: ${.CURDIR}/keys.list ${.CURDIR}/MKkeys.awk
	awk -f ${.CURDIR}/MKkeys.awk ${.CURDIR}/keys.list > keys.tries

# in case you don't run make depend
lib_options.o lib_options.so lib_options.po: keys.tries

lib_keyname.c: ${.CURDIR}/keys.list ${.CURDIR}/MKkeyname.awk
	awk -f ${.CURDIR}/MKkeyname.awk ${.CURDIR}/keys.list > lib_keyname.c

MAN3 =  curs_addch.3 \
	curs_addchst.3 \
	curs_addstr.3 \
	curs_attr.3 \
	curs_beep.3 \
	curs_bkgd.3 \
	curs_border.3 \
	curs_clear.3 \
	curs_color.3 \
	curs_delch.3 \
	curs_delln.3 \
	curs_getch.3 \
	curs_getstr.3 \
	curs_getyx.3 \
	curs_inch.3  \
	curs_inchstr.3 \
	curs_initscr.3 \
	curs_inopts.3 \
	curs_insch.3 \
	curs_insstr.3 \
	curs_instr.3 \
	curs_kernel.3 \
	curs_move.3 \
	curs_outopts.3 \
	curs_overlay.3 \
	curs_pad.3 \
	curs_printw.3 \
	curs_refresh.3 \
	curs_scanw.3 \
	curs_scr_dmp.3 \
	curs_scroll.3 \
	curs_slk.3 \
	curs_termatt.3 \
	curs_termin.3 \
	curs_touch.3 \
	curs_util.3 \
	curs_window.3 \
	ncurses.3

MAN5 =  term.5 terminfo.5

MLINKS+=curs_addch.3 addch.3 curs_addch.3 waddch.3 curs_addch.3 echochar.3 \
	curs_addch.3 wechochar.3 curs_addch.3 mvaddch.3 \
	curs_addch.3 mvwaddch.3 
MLINKS+=curs_addchst.3 addchstr.3 curs_addchst.3 addchnstr.3 \
	curs_addchst.3 waddchstr.3 curs_addchst.3 waddchnstr.3 \
	curs_addchst.3 mvaddchstr.3 curs_addchst.3 mvaddchnstr.3 \
	curs_addchst.3 mvwaddchstr.3 curs_addchst.3 mvwaddchnstr.3
MLINKS+=curs_addstr.3 addstr.3 curs_addstr.3 addnstr.3 \
	curs_addstr.3 waddstr.3 curs_addstr.3 waddnstr.3 \
	curs_addstr.3 mvaddstr.3 curs_addstr.3 mvaddnstr.3 \
	curs_addstr.3 mvwaddstr.3 curs_addstr.3 mvwaddnstr.3
MLINKS+=curs_attr.3 attroff.3 curs_attr.3 wattroff.3 \
	curs_attr.3 attron.3 curs_attr.3 wattron.3 \
	curs_attr.3 attrset.3 curs_attr.3 wattrset.3 \
	curs_attr.3 standend.3 curs_attr.3 wstandend.3 \
	curs_attr.3 standout.3 curs_attr.3 wstandout.3
MLINKS+=curs_beep.3 beep.3 curs_beep.3 flash.3
MLINKS+=curs_bkgd.3 bkgdset.3 curs_bkgd.3 wbkgdset.3 \
	curs_bkgd.3 bkgd.3 curs_bkgd.3 wbkgd.3
MLINKS+=curs_border.3 border.3 curs_border.3 wborder.3 curs_border.3 box.3 \
	curs_border.3 hline.3 curs_border.3 whline.3 \
	curs_border.3 vline.3 curs_border.3 wvline.3
MLINKS+=curs_clear.3 erase.3 curs_clear.3 werase.3 curs_clear.3 clear.3 \
	curs_clear.3 wclear.3 curs_clear.3 clrtobot.3 \
	curs_clear.3 wclrtobot.3 curs_clear.3 clrtoeol.3 \
	curs_clear.3 wclrtoeol.3
MLINKS+=curs_color.3 start_color.3 curs_color.3 init_pair.3 \
	curs_color.3 init_color.3 curs_color.3 has_colors.3 \
	curs_color.3 can_change_color.3 curs_color.3 color_content.3 \
	curs_color.3 pair_content.3
MLINKS+=curs_delch.3 delch.3 curs_delch.3 wdelch.3 curs_delch.3 mvdelch.3 \
	curs_delch.3 mvwdelch.3
MLINKS+=curs_delln.3 deleteln.3 curs_delln.3 wdeleteln.3 \
	curs_delln.3 insdelln.3 curs_delln.3 winsdelln.3 \
	curs_delln.3 insertln.3 curs_delln.3 winsertln.3
MLINKS+=curs_getch.3 getch.3 curs_getch.3 wgetch.3 curs_getch.3 mvgetch.3 \
	curs_getch.3 mvwgetch.3 curs_getch.3 ungetch.3
MLINKS+=curs_getstr.3 getstr.3 curs_getstr.3 wgetstr.3 \
	curs_getstr.3 mvgetstr.3 curs_getstr.3 mvwgetstr.3 \
	curs_getstr.3 wgetnstr.3
MLINKS+=curs_getyx.3 getyx.3 curs_getyx.3 pgetparyx.3 curs_getyx.3 getbegyx.3 \
	curs_getyx.3 getmaxyx.3
MLINKS+=curs_inch.3 inch.3 curs_inch.3 winch.3 curs_inch.3 mvinch.3 \
	curs_inch.3 mvwinch.3
MLINKS+=curs_inchstr.3 inchstr.3 curs_inchstr.3 inchnstr.3 \
	curs_inchstr.3 winchstr.3 curs_inchstr.3 winchnstr.3 \
	curs_inchstr.3 mvinchstr.3 curs_inchstr.3 mvinchnstr.3 \
	curs_inchstr.3 mvwinchstr.3 curs_inchstr.3 mvwinchnstr.3
MLINKS+=curs_initscr.3 initscr.3 curs_initscr.3 newterm.3 \
	curs_initscr.3 endwin.3 curs_initscr.3 isendwin.3 \
	curs_initscr.3 set_term.3 curs_initscr.3 delscreen.3
MLINKS+=curs_inopts.3 cbreak.3 curs_inopts.3 nocbreak.3 curs_inopts.3 echo.3 \
	curs_inopts.3 noecho.3 curs_inopts.3 halfdelay.3 \
	curs_inopts.3 intrflush.3 curs_inopts.3 keypad.3 curs_inopts.3 meta.3 \
	curs_inopts.3 nodelay.3 curs_inopts.3 notimeout.3 curs_inopts.3 raw.3 \
	curs_inopts.3 noraw.3 curs_inopts.3 noqiflush.3 \
	curs_inopts.3 qiflush.3 curs_inopts.3 timeout.3 \
	curs_inopts.3 wtimeout.3 curs_inopts.3 typeahead.3
MLINKS+=curs_insch.3 insch.3 curs_insch.3 winsch.3 curs_insch.3 mvinsch.3 \
	curs_insch.3 mvwinsch.3
MLINKS+=curs_insstr.3 insstr.3 curs_insstr.3 insnstr.3 \
	curs_insstr.3 winsstr.3 curs_insstr.3 winsnstr.3 \
	curs_insstr.3 mvinsstr.3 curs_insstr.3 mvinsnstr.3 \
	curs_insstr.3 mvwinsstr.3 curs_insstr.3 mvwinsnstr.3
MLINKS+=curs_instr.3 instr.3 curs_instr.3 innstr.3 curs_instr.3 winstr.3 \
	curs_instr.3 winnstr.3 curs_instr.3 mvinstr.3 curs_instr.3 mvinnstr.3 \
	curs_instr.3 mvwinstr.3 curs_instr.3 mvwinnstr.3
MLINKS+=curs_kernel.3 def_prog_mode.3 curs_kernel.3 def_shell_mode.3 \
	curs_kernel.3 reset_prog_mode.3 curs_kernel.3 reset_shell_mode.3 \
	curs_kernel.3 resetty.3 curs_kernel.3 savetty.3 \
	curs_kernel.3 getsyx.3 curs_kernel.3 setsyx.3 \
	curs_kernel.3 ripoffline.3 curs_kernel.3 curs_set.3 \
	curs_kernel.3 napms.3
MLINKS+=curs_move.3 move.3 curs_move.3 wmove.3
MLINKS+=curs_outopts.3 clearok.3 curs_outopts.3 idlok.3 \
	curs_outopts.3 idcok.3 curs_outopts.3 immedok.3 \
	curs_outopts.3 leaveok.3 curs_outopts.3 setscrreg.3 \
	curs_outopts.3 wsetscrreg.3 curs_outopts.3 scrollok.3 \
	curs_outopts.3 nl.3 curs_outopts.3 nonl.3
MLINKS+=curs_overlay.3 overlay.3 curs_overlay.3 overwrite.3 \
	curs_overlay.3 copywin.3
MLINKS+=curs_pad.3 newpad.3 curs_pad.3 subpad.3 curs_pad.3 prefresh.3 \
	curs_pad.3 pnoutrefresh.3 curs_pad.3 pechochar.3
MLINKS+=curs_printw.3 printw.3 curs_printw.3 wprintw.3 \
	curs_printw.3 mvprintw.3 curs_printw.3 mvwprintw.3 \
	curs_printw.3 vwprintw.3
MLINKS+=curs_refresh.3 refresh.3 curs_refresh.3 wrefresh.3 \
	curs_refresh.3 wnoutrefresh.3 curs_refresh.3 doupdate.3 \
	curs_refresh.3 redrawwin.3 curs_refresh.3 wredrawln.3
MLINKS+=curs_scanw.3 scanw.3 curs_scanw.3 wscanw.3 curs_scanw.3 mvscanw.3 \
	curs_scanw.3 mvwscanw.3 curs_scanw.3 vwscanw.3
MLINKS+=curs_scr_dmp.3 scr_dump.3 curs_scr_dmp.3 scr_restore.3 \
	curs_scr_dmp.3 scr_init.3 curs_scr_dmp.3 scr_set.3
MLINKS+=curs_scroll.3 scroll.3 curs_scroll.3 scrl.3 curs_scroll.3 wscrl.3
MLINKS+=curs_slk.3 slk_init.3 curs_slk.3 slk_set.3 curs_slk.3 slk_refresh.3 \
	curs_slk.3 slk_noutrefresh.3 curs_slk.3 slk_label.3 \
	curs_slk.3 slk_clear.3 curs_slk.3 slk_restore.3 \
	curs_slk.3 slk_touch.3 curs_slk.3 slk_attron.3 \
	curs_slk.3 slk_attrset.3 curs_slk.3 slk_attroff.3
MLINKS+=curs_termatt.3 baudrate.3 curs_termatt.3 erasechar.3 \
	curs_termatt.3 has_ic.3 curs_termatt.3 has_il.3 \
	curs_termatt.3 killchar.3 curs_termatt.3 longname.3 \
	curs_termatt.3 termattrs.3 curs_termatt.3 termname.3

# XXX name clash with termcap(3)
# MLINKS+= curs_termin.3 tparm.3 
MLINKS+=curs_termin.3 setupterm.3 curs_termin.3 setterm.3 \
	curs_termin.3 set_curterm.3 curs_termin.3 del_curterm.3 \
	curs_termin.3 restartterm.3 \
	curs_termin.3 putp.3 curs_termin.3 vidputs.3 \
	curs_termin.3 vidattr.3 curs_termin.3 mvcur.3 \
	curs_termin.3 tigetflag.3 curs_termin.3 tigetnum.3 \
	curs_termin.3 tigetstr.3
MLINKS+=curs_touch.3 touchwin.3 curs_touch.3 touchline.3 \
	curs_touch.3 untouchwin.3 curs_touch.3 wtouchln.3 \
	curs_touch.3 is_linetouched.3 curs_touch.3 is_wintouched.3
MLINKS+=curs_util.3 unctrl.3 curs_util.3 keyname.3 \
	curs_util.3 filter.3 curs_util.3 use_env.3 \
	curs_util.3 putwin.3 curs_util.3 getwin.3 \
	curs_util.3 delay_output.3 curs_util.3 flushinp.3
MLINKS+=curs_window.3 newwin.3 curs_window.3 delwin.3 \
	curs_window.3 mvwin.3 curs_window.3 subwin.3 \
	curs_window.3 derwin.3 curs_window.3 mvderwin.3 \
	curs_window.3 dupwin.3 curs_window.3 wsyncup.3 \
	curs_window.3 syncok.3 curs_window.3 wcursyncup.3 \
	curs_window.3 wsyncdown.3

.include <bsd.lib.mk>
