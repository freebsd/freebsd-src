# $FreeBSD$

NCURSES=${.CURDIR}/../../contrib/ncurses

LIB=	ncurses
SHLIB_MAJOR=5
SHLIB_MINOR=0

# Should be elsewhere
AWK?=	awk
TERMINFODIR?=	${SHAREDIR}/misc

NCURSES_MAJOR!=egrep 'NCURSES_MAJOR[ 	]*=' ${NCURSES}/dist.mk | sed -e 's%^[^0-9]*%%'
NCURSES_MINOR!=egrep 'NCURSES_MINOR[ 	]*=' ${NCURSES}/dist.mk | sed -e 's%^[^0-9]*%%'
NCURSES_PATCH!=egrep 'NCURSES_PATCH[ 	]*=' ${NCURSES}/dist.mk | sed -e 's%^[^0-9]*%%'

# From autoconf (!)
NCURSES_CONST=	/* nothing */
NCURSES_XNAMES=	1
NCURSES_OSPEED=	short
BUILTIN_BOOL=	1
BOOL_TYPE=	0
TYPE_OF_BOOL=	char
TYPEOF_CHTYPE=	long
WIDEC_SHIFT=	8
SHIFT_LIMIT=	32
ONEUL=		1UL

.PATH:  ${NCURSES}/ncurses
.PATH:  ${NCURSES}/ncurses/base
.PATH:  ${NCURSES}/ncurses/tinfo
.PATH:  ${NCURSES}/ncurses/tty
.PATH:  ${NCURSES}/ncurses/trace
.PATH:  ${NCURSES}/include
.PATH:  ${NCURSES}/man

INCS=	-I. -I${.CURDIR} -I${NCURSES}/ncurses -I${NCURSES}/include
CFLAGS+=${INCS} -Wall -DFREEBSD_NATIVE -DNDEBUG -DHAVE_CONFIG_H -DTERMIOS

GENSRC= \
	codes.c \
	expanded.c \
	fallback.c \
	lib_gen.c \
	lib_keyname.c \
	names.c \
	unctrl.c

GENHDR= \
	curses.h \
	hashsize.h \
	init_keytry.h \
	ncurses_def.h \
	nomacros.h \
	parametrized.h \
	term.h \
	termcap.h \
	unctrl.h

# Installed
HEADERS=curses.h term.h termcap.h unctrl.h
SRCHDRS=ncurses_dll.h

# Components of names.c and codes.c
NAMESRC=boolnames boolfnames numnames numfnames strnames strfnames
CODESRC=boolcodes numcodes strcodes

SRCS=	${GENHDR} ${GENSRC} \
	access.c \
	add_tries.c \
	alloc_entry.c \
	alloc_ttype.c \
	captoinfo.c \
	comp_captab.c \
	comp_error.c \
	comp_expand.c \
	comp_hash.c \
	comp_parse.c \
	comp_scan.c \
	define_key.c \
	doalloc.c \
	free_ttype.c \
	getenv_num.c \
	hardscroll.c \
	hashmap.c \
	home_terminfo.c \
	init_keytry.c \
	keybound.c \
	keyok.c \
	lib_acs.c \
	lib_addch.c \
	lib_addstr.c \
	lib_baudrate.c \
	lib_beep.c \
	lib_bkgd.c \
	lib_box.c \
	lib_chgat.c \
	lib_clear.c \
	lib_clearok.c \
	lib_clrbot.c \
	lib_clreol.c \
	lib_color.c \
	lib_colorset.c \
	lib_cur_term.c \
	lib_data.c \
	lib_delch.c \
	lib_delwin.c \
	lib_dft_fgbg.c \
	lib_echo.c \
	lib_endwin.c \
	lib_erase.c \
	lib_flash.c \
	lib_freeall.c \
	lib_getch.c \
	lib_getstr.c \
	lib_has_cap.c \
	lib_hline.c \
	lib_immedok.c \
	lib_inchstr.c \
	lib_initscr.c \
	lib_insch.c \
	lib_insdel.c \
	lib_insstr.c \
	lib_instr.c \
	lib_isendwin.c \
	lib_kernel.c \
	lib_leaveok.c \
	lib_longname.c \
	lib_mouse.c \
	lib_move.c \
	lib_mvcur.c \
	lib_mvwin.c \
	lib_napms.c \
	lib_newterm.c \
	lib_newwin.c \
	lib_nl.c \
	lib_options.c \
	lib_overlay.c \
	lib_pad.c \
	lib_print.c \
	lib_printw.c \
	lib_raw.c \
	lib_redrawln.c \
	lib_refresh.c \
	lib_restart.c \
	lib_scanw.c \
	lib_screen.c \
	lib_scroll.c \
	lib_scrollok.c \
	lib_scrreg.c \
	lib_set_term.c \
	lib_setup.c \
	lib_slk.c \
	lib_slkatr_set.c \
	lib_slkatrof.c \
	lib_slkatron.c \
	lib_slkatrset.c \
	lib_slkattr.c \
	lib_slkclear.c \
	lib_slkcolor.c \
	lib_slkinit.c \
	lib_slklab.c \
	lib_slkrefr.c \
	lib_slkset.c \
	lib_slktouch.c \
	lib_termcap.c \
	lib_termname.c \
	lib_tgoto.c \
	lib_ti.c \
	lib_touch.c \
	lib_tparm.c \
	lib_tputs.c \
	lib_trace.c \
	lib_traceatr.c \
	lib_tracebits.c \
	lib_tracechr.c \
	lib_tracedmp.c \
	lib_tracemse.c \
	lib_tstp.c \
	lib_ttyflags.c \
	lib_twait.c \
	lib_ungetch.c \
	lib_vidattr.c \
	lib_vline.c \
	lib_wattroff.c \
	lib_wattron.c \
	lib_winch.c \
	lib_window.c \
	memmove.c \
	name_match.c \
	nc_panel.c \
	parse_entry.c \
	read_entry.c \
	resizeterm.c \
	safe_sprintf.c \
	setbuf.c \
	sigaction.c \
	strings.c \
	trace_buf.c \
	trace_tries.c \
	trace_xnames.c \
	tries.c \
	tty_update.c \
	version.c \
	vsscanf.c \
	wresize.c \
	write_entry.c \

# From our old libtermcap.
# Used instead of the hideous read_termcap.c abomination.
SRCS+=	termcap.c

CLEANFILES+=	${GENSRC} ${GENHDR} keys.tries make_hash term.h.new \
	make_keys MKterm.h.awk comp_captab.c \
	namehdr nameftr codeftr ${NAMESRC} ${CODESRC}

SYMLINKS+=libncurses.a ${LIBDIR}/libcurses.a
SYMLINKS+=libncurses.a ${LIBDIR}/libtermcap.a
SYMLINKS+=libncurses.a ${LIBDIR}/libtermlib.a
SYMLINKS+=libncurses.a ${LIBDIR}/libmytinfo.a
SYMLINKS+=libncurses.a ${LIBDIR}/libtinfo.a
.if !defined(NOPIC)
.if ${OBJFORMAT} == aout
SYMLINKS+=libncurses.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	  libcurses.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
SYMLINKS+=libncurses.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	  libtermcap.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
SYMLINKS+=libncurses.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	  libtermlib.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
SYMLINKS+=libncurses.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	  libmytinfo.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
SYMLINKS+=libncurses.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	  libtinfo.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
.else
# no need for major at all, it's an ld-time redirection only
SYMLINKS+=libncurses.so ${SHLIBDIR}/libcurses.so
SYMLINKS+=libncurses.so ${SHLIBDIR}/libtermcap.so
SYMLINKS+=libncurses.so ${SHLIBDIR}/libtermlib.so
SYMLINKS+=libncurses.so ${SHLIBDIR}/libmytinfo.so
SYMLINKS+=libncurses.so ${SHLIBDIR}/libtinfo.so
.endif
.endif
.if !defined(NOPROFILE)
SYMLINKS+=libncurses_p.a ${LIBDIR}/libcurses_p.a
SYMLINKS+=libncurses_p.a ${LIBDIR}/libtermcap_p.a
SYMLINKS+=libncurses_p.a ${LIBDIR}/libtermlib_p.a
SYMLINKS+=libncurses_p.a ${LIBDIR}/libmytinfo_p.a
SYMLINKS+=libncurses_p.a ${LIBDIR}/libtinfo_p.a
.endif

DOCSDIR= /usr/share/doc/ncurses
DOCS=	ncurses-intro.html hackguide.html

beforeinstall:	${HEADERS}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${HEADERS} \
	    ${DESTDIR}/usr/include
	cd ${NCURSES}/include ; \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${SRCHDRS} \
	    ${DESTDIR}/usr/include
	rm -f ${DESTDIR}/usr/include/ncurses.h
	ln -s curses.h ${DESTDIR}/usr/include/ncurses.h

.if !defined(NOHTML)
afterinstall:
.for file in ${DOCS}
	cd ${.CURDIR}/../../contrib/ncurses/doc/html ; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${file} \
		${DESTDIR}${DOCSDIR}
.endfor
.endif

# Generated source
namehdr nameftr codeftr ${NAMESRC} ${CODESRC}: MKnames.awk Caps
	${AWK} -f ${NCURSES}/ncurses/tinfo/MKnames.awk ${NCURSES}/include/Caps

.ORDER: namehdr ${NAMESRC} ${CODESRC} nameftr codeftr names.c codes.c

names.c:	 namehdr ${NAMESRC} nameftr
	cat namehdr ${NAMESRC} nameftr > $@

codes.c:	 namehdr ${CODESRC} codeftr
	cat namehdr ${CODESRC} codeftr > $@

lib_gen.c:	MKlib_gen.sh curses.h
	sh ${NCURSES}/ncurses/base/MKlib_gen.sh "${CC} -E ${INCS}" \
	    "${AWK}" < curses.h >$@

lib_keyname.c:	keys.list MKkeyname.awk
	${AWK} -f ${NCURSES}/ncurses/base/MKkeyname.awk \
	    ${NCURSES}/ncurses/tinfo/keys.list > lib_keyname.c

unctrl.c:	MKunctrl.awk
	echo | ${AWK} -f ${NCURSES}/ncurses/base/MKunctrl.awk > unctrl.c

comp_captab.c:	MKcaptab.awk Caps make_hash
	sh ${NCURSES}/ncurses/tinfo/MKcaptab.awk "${AWK}" \
	    ${NCURSES}/include/Caps > comp_captab.c

expanded.c:	MKexpanded.sh
	sh ${NCURSES}/ncurses/tty/MKexpanded.sh "${CC} -E" ${CFLAGS} >expanded.c

fallback.c:	MKfallback.sh
	sh ${NCURSES}/ncurses/tinfo/MKfallback.sh > fallback.c

# Generated headers
ncurses_def.h:	MKncurses_def.sh ncurses_defs
	AWK=${AWK} sh ${NCURSES}/include/MKncurses_def.sh \
	    ${NCURSES}/include/ncurses_defs > ncurses_def.h

nomacros.h:	MKlib_gen.sh curses.h
	sh ${NCURSES}/ncurses/base/MKlib_gen.sh "${CC} -E ${INCS}" \
	    "${AWK}" < curses.h | fgrep undef > $@

init_keytry.h:	keys.list make_keys
	./make_keys ${NCURSES}/ncurses/tinfo/keys.list > init_keytry.h

hashsize.h:	MKhashsize.sh Caps
	sh ${NCURSES}/include/MKhashsize.sh ${NCURSES}/include/Caps > $@

parametrized.h:	MKparametrized.sh Caps
	AWK=${AWK} sh ${NCURSES}/include/MKparametrized.sh \
	    ${NCURSES}/include/Caps > $@

term.h:		MKterm.h.awk edit_cfg.sh Caps
	${AWK} -f MKterm.h.awk ${NCURSES}/include/Caps > $@.new
	sh ${NCURSES}/include/edit_cfg.sh ${.CURDIR}/ncurses_cfg.h $@.new
	mv -f $@.new $@

# Build tools
build-tools: make_hash make_keys

make_keys:	make_keys.c names.c curses.h ncurses_def.h
	${CC} -o $@ ${CFLAGS} ${NCURSES}/ncurses/tinfo/make_keys.c

make_hash:	comp_hash.c hashsize.h curses.h ncurses_def.h
	${CC} -o $@ ${CFLAGS} -DMAIN_PROGRAM \
		${NCURSES}/ncurses/tinfo/comp_hash.c

# ./configure generated
MKterm.h.awk:	MKterm.h.awk.in
	sed <${NCURSES}/include/$@.in >$@ \
	    -e "/@NCURSES_MAJOR@/s%%${NCURSES_MAJOR}%" \
	    -e "/@NCURSES_MINOR@/s%%${NCURSES_MINOR}%" \
	    -e "/@NCURSES_CONST@/s%%${NCURSES_CONST}%" \
	    -e "/@NCURSES_XNAMES@/s%%${NCURSES_XNAMES}%"

termcap.h:	termcap.h.in
	sed <${NCURSES}/include/$@.in >$@ \
	    -e "/@NCURSES_MAJOR@/s%%${NCURSES_MAJOR}%" \
	    -e "/@NCURSES_MINOR@/s%%${NCURSES_MINOR}%" \
	    -e "/@NCURSES_CONST@/s%%${NCURSES_CONST}%" \
	    -e "/@NCURSES_OSPEED@/s%%${NCURSES_OSPEED}%"

curses.h:	curses.h.in
	sed <${NCURSES}/include/$@.in >$@ \
	    -e "/@NCURSES_MAJOR@/s%%${NCURSES_MAJOR}%" \
	    -e "/@NCURSES_MINOR@/s%%${NCURSES_MINOR}%" \
	    -e "/@NCURSES_PATCH@/s%%${NCURSES_PATCH}%" \
	    -e "/@NCURSES_CONST@/s%%${NCURSES_CONST}%" \
	    -e "s%@cf_cv_builtin_bool@%${BUILTIN_BOOL}%g" \
	    -e "s%@cf_cv_cc_bool_type@%${BOOL_TYPE}%g" \
	    -e "s%@cf_cv_type_of_bool@%${TYPE_OF_BOOL}%g" \
	    -e "s%@cf_cv_typeof_chtype@%${TYPEOF_CHTYPE}%g" \
	    -e "s%@cf_cv_widec_shift@%${WIDEC_SHIFT}%g" \
	    -e "s%@cf_cv_shift_limit@%${SHIFT_LIMIT}%g" \
	    -e "s%@cf_cv_1UL@%${ONEUL}%g" \
	    -e "s/ _WCHAR_T/ _BSD_WCHAR_T_/g" \
	    -e "s/ _WINT_T/ _BSD_WINT_T_/g"

unctrl.h:	unctrl.h.in
	sed <${NCURSES}/include/$@.in >$@ \
	    -e "/@NCURSES_MAJOR@/s%%${NCURSES_MAJOR}%" \
	    -e "/@NCURSES_MINOR@/s%%${NCURSES_MINOR}%"

# MAN page gunk
terminfo.5:	MKterminfo.sh terminfo.head Caps
	sh ${NCURSES}/man/MKterminfo.sh ${NCURSES}/man/terminfo.head \
	    ${NCURSES}/include/Caps ${NCURSES}/man/terminfo.tail >$@

CLEANFILES+=	terminfo.5
MANFILTER=	sed -e 's%@TERMINFO@%${TERMINFODIR}/terminfo%g'

MANx=	curs_addch.3x curs_addchstr.3x curs_addstr.3x curs_attr.3x \
	curs_beep.3x curs_bkgd.3x curs_border.3x curs_clear.3x curs_color.3x \
	curs_delch.3x curs_deleteln.3x curs_extend.3x curs_getch.3x \
	curs_getstr.3x \
	curs_getyx.3x curs_inch.3x curs_inchstr.3x curs_initscr.3x \
	curs_inopts.3x curs_insch.3x curs_insstr.3x curs_instr.3x \
	curs_kernel.3x curs_mouse.3x curs_move.3x curs_outopts.3x \
	curs_overlay.3x curs_pad.3x curs_print.3x curs_printw.3x \
	curs_refresh.3x curs_scanw.3x curs_scr_dump.3x curs_scroll.3x \
	curs_slk.3x curs_termattrs.3x curs_termcap.3x curs_terminfo.3x \
	curs_touch.3x curs_trace.3x curs_util.3x curs_window.3x \
	default_colors.3x define_key.3x \
	keybound.3x keyok.3x ncurses.3x resizeterm.3x wresize.3x
MAN=	term.5 terminfo.5
MAN+=	term.7

# Generate the MAN list from MANx
.for page in ${MANx}
CLEANFILES+=${page:T:S/x$//g}
MAN+=${page:T:S/x$//g}
${page:T:S/x$//g}: ${page}
	ln -s ${.ALLSRC} ${.TARGET}
.endfor

MLINKS+=ncurses.3 curses.3
MLINKS+=curs_addch.3 addch.3 curs_addch.3 echochar.3 curs_addch.3 mvaddch.3 \
	curs_addch.3 mvwaddch.3 curs_addch.3 waddch.3 curs_addch.3 wechochar.3
MLINKS+=curs_addchstr.3 addchnstr.3 curs_addchstr.3 addchstr.3 \
	curs_addchstr.3 mvaddchnstr.3 curs_addchstr.3 mvaddchstr.3 \
	curs_addchstr.3 mvwaddchnstr.3 curs_addchstr.3 mvwaddchstr.3 \
	curs_addchstr.3 waddchnstr.3 curs_addchstr.3 waddchstr.3
MLINKS+=curs_addstr.3 addnstr.3 curs_addstr.3 addstr.3 \
	curs_addstr.3 mvaddnstr.3 curs_addstr.3 mvaddstr.3 \
	curs_addstr.3 mvwaddnstr.3 curs_addstr.3 mvwaddstr.3 \
	curs_addstr.3 waddnstr.3 curs_addstr.3 waddstr.3
MLINKS+=curs_attr.3 PAIR_NUMBER.3 \
	curs_attr.3 attr_get.3 curs_attr.3 attr_off.3 curs_attr.3 attr_on.3 \
	curs_attr.3 attr_set.3 curs_attr.3 attroff.3 curs_attr.3 attron.3 \
	curs_attr.3 attrset.3 curs_attr.3 chgat.3 curs_attr.3 color_set.3 \
	curs_attr.3 mvchgat.3 curs_attr.3 mvwchgat.3 curs_attr.3 standend.3 \
	curs_attr.3 standout.3 curs_attr.3 wattr_get.3 curs_attr.3 wattr_off.3 \
	curs_attr.3 wattr_on.3 curs_attr.3 wattr_set.3 curs_attr.3 wattroff.3 \
	curs_attr.3 wattron.3 curs_attr.3 wattrset.3 curs_attr.3 wchgat.3 \
	curs_attr.3 wcolor_set.3 curs_attr.3 wstandend.3 \
	curs_attr.3 wstandout.3
MLINKS+=curs_beep.3 beep.3 curs_beep.3 flash.3
MLINKS+=curs_bkgd.3 bkgd.3 curs_bkgd.3 bkgdset.3 curs_bkgd.3 getbkgd.3 \
	curs_bkgd.3 wbkgd.3 curs_bkgd.3 wbkgdset.3
MLINKS+=curs_border.3 border.3 curs_border.3 box.3 curs_border.3 hline.3 \
	curs_border.3 mvhline.3 curs_border.3 mvvline.3 \
	curs_border.3 mvwhline.3 \
	curs_border.3 mvwvline.3 curs_border.3 vline.3 curs_border.3 wborder.3 \
	curs_border.3 whline.3 curs_border.3 wvline.3
MLINKS+=curs_clear.3 clear.3 curs_clear.3 clrtobot.3 curs_clear.3 clrtoeol.3 \
	curs_clear.3 erase.3 curs_clear.3 wclear.3 curs_clear.3 wclrtobot.3 \
	curs_clear.3 wclrtoeol.3 curs_clear.3 werase.3
MLINKS+=curs_color.3 COLOR_PAIR.3 \
	curs_color.3 can_change_color.3 curs_color.3 color_content.3 \
	curs_color.3 has_colors.3 curs_color.3 init_color.3 \
	curs_color.3 init_pair.3 curs_color.3 pair_content.3 \
	curs_color.3 start_color.3
MLINKS+=curs_delch.3 delch.3 curs_delch.3 mvdelch.3 curs_delch.3 mvwdelch.3 \
	curs_delch.3 wdelch.3
MLINKS+=curs_deleteln.3 deleteln.3 curs_deleteln.3 insdelln.3 \
	curs_deleteln.3 insertln.3 curs_deleteln.3 wdeleteln.3 \
	curs_deleteln.3 winsdelln.3 curs_deleteln.3 winsertln.3
MLINKS+=curs_extend.3 curses_version.3 curs_extend.3 use_extended_names.3
MLINKS+=curs_getch.3 getch.3 curs_getch.3 has_key.3 curs_getch.3 mvgetch.3 \
	curs_getch.3 mvwgetch.3 curs_getch.3 ungetch.3 curs_getch.3 wgetch.3
MLINKS+=curs_getstr.3 getnstr.3 curs_getstr.3 getstr.3 \
	curs_getstr.3 mvgetnstr.3 curs_getstr.3 mvgetstr.3 \
	curs_getstr.3 mvwgetnstr.3 curs_getstr.3 mvwgetstr.3 \
	curs_getstr.3 wgetnstr.3 curs_getstr.3 wgetstr.3
MLINKS+=curs_getyx.3 getbegyx.3 curs_getyx.3 getmaxyx.3 \
	curs_getyx.3 getparyx.3 curs_getyx.3 getyx.3
MLINKS+=curs_inch.3 inch.3 curs_inch.3 mvinch.3 curs_inch.3 mvwinch.3 \
	curs_inch.3 winch.3
MLINKS+=curs_inchstr.3 inchnstr.3 curs_inchstr.3 inchstr.3 \
	curs_inchstr.3 mvinchnstr.3 curs_inchstr.3 mvinchstr.3 \
	curs_inchstr.3 mvwinchnstr.3 curs_inchstr.3 mvwinchstr.3 \
	curs_inchstr.3 winchnstr.3 curs_inchstr.3 winchstr.3
MLINKS+=curs_initscr.3 delscreen.3 curs_initscr.3 endwin.3 \
	curs_initscr.3 initscr.3 curs_initscr.3 isendwin.3 \
	curs_initscr.3 newterm.3 curs_initscr.3 set_term.3
MLINKS+=curs_inopts.3 cbreak.3 curs_inopts.3 echo.3 curs_inopts.3 halfdelay.3 \
	curs_inopts.3 intrflush.3 curs_inopts.3 keypad.3 curs_inopts.3 meta.3 \
	curs_inopts.3 nocbreak.3 curs_inopts.3 nodelay.3 \
	curs_inopts.3 noecho.3 curs_inopts.3 noqiflush.3 curs_inopts.3 noraw.3 \
	curs_inopts.3 notimeout.3 curs_inopts.3 qiflush.3 curs_inopts.3 raw.3 \
	curs_inopts.3 timeout.3 curs_inopts.3 typeahead.3 \
	curs_inopts.3 wtimeout.3
MLINKS+=curs_insch.3 insch.3 curs_insch.3 mvinsch.3 curs_insch.3 mvwinsch.3 \
	curs_insch.3 winsch.3
MLINKS+=curs_insstr.3 insnstr.3 curs_insstr.3 insstr.3 \
	curs_insstr.3 mvinsnstr.3 curs_insstr.3 mvinsstr.3 \
	curs_insstr.3 mvwinsnstr.3 curs_insstr.3 mvwinsstr.3 \
	curs_insstr.3 winsnstr.3 curs_insstr.3 winsstr.3
MLINKS+=curs_instr.3 innstr.3 curs_instr.3 instr.3 curs_instr.3 mvinnstr.3 \
	curs_instr.3 mvinstr.3 curs_instr.3 mvwinnstr.3 \
	curs_instr.3 mvwinstr.3 curs_instr.3 winnstr.3 curs_instr.3 winstr.3
MLINKS+=curs_kernel.3 curs_set.3 curs_kernel.3 def_prog_mode.3 \
	curs_kernel.3 def_shell_mode.3 curs_kernel.3 getsyx.3 \
	curs_kernel.3 napms.3 curs_kernel.3 reset_prog_mode.3 \
	curs_kernel.3 reset_shell_mode.3 curs_kernel.3 resetty.3 \
	curs_kernel.3 ripoffline.3 curs_kernel.3 savetty.3 \
	curs_kernel.3 setsyx.3
MLINKS+=curs_mouse.3 getmouse.3 curs_mouse.3 mouse_trafo.3 \
	curs_mouse.3 mouseinterval.3 \
	curs_mouse.3 mousemask.3 curs_mouse.3 ungetmouse.3 \
	curs_mouse.3 wenclose.3 curs_mouse.3 wmouse_trafo.3
MLINKS+=curs_move.3 move.3 curs_move.3 wmove.3
MLINKS+=curs_outopts.3 clearok.3 curs_outopts.3 idcok.3 curs_outopts.3 idlok.3 \
	curs_outopts.3 immedok.3 curs_outopts.3 leaveok.3 curs_outopts.3 nl.3 \
	curs_outopts.3 nonl.3 curs_outopts.3 scrollok.3 \
	curs_outopts.3 setscrreg.3 curs_outopts.3 wsetscrreg.3
MLINKS+=curs_overlay.3 copywin.3 curs_overlay.3 overlay.3 \
	curs_overlay.3 overwrite.3
MLINKS+=curs_pad.3 newpad.3 curs_pad.3 pechochar.3 curs_pad.3 pnoutrefresh.3 \
	curs_pad.3 prefresh.3 curs_pad.3 subpad.3
MLINKS+=curs_print.3 mcprint.3
MLINKS+=curs_printw.3 mvprintw.3 curs_printw.3 mvwprintw.3 \
	curs_printw.3 printw.3 curs_printw.3 vw_printw.3 \
	curs_printw.3 vwprintw.3 curs_printw.3 wprintw.3
MLINKS+=curs_refresh.3 doupdate.3 curs_refresh.3 redrawwin.3 \
	curs_refresh.3 refresh.3 curs_refresh.3 wnoutrefresh.3 \
	curs_refresh.3 wredrawln.3 curs_refresh.3 wrefresh.3
MLINKS+=curs_scanw.3 mvscanw.3 curs_scanw.3 mvwscanw.3 curs_scanw.3 scanw.3 \
	curs_scanw.3 vw_scanw.3 curs_scanw.3 vwscanw.3 curs_scanw.3 wscanw.3
MLINKS+=curs_scr_dump.3 scr_dump.3 curs_scr_dump.3 scr_init.3 \
	curs_scr_dump.3 scr_restore.3 curs_scr_dump.3 scr_set.3
MLINKS+=curs_scroll.3 scrl.3 curs_scroll.3 scroll.3 curs_scroll.3 wscrl.3
MLINKS+=curs_slk.3 slk_attr.3 curs_slk.3 slk_attr_off.3 \
	curs_slk.3 slk_attr_on.3 curs_slk.3 slk_attr_set.3 \
	curs_slk.3 slk_attroff.3 curs_slk.3 slk_attron.3 \
	curs_slk.3 slk_attrset.3 curs_slk.3 slk_clear.3 \
	curs_slk.3 slk_color.3 curs_slk.3 slk_init.3 curs_slk.3 slk_label.3 \
	curs_slk.3 slk_noutrefresh.3 curs_slk.3 slk_refresh.3 \
	curs_slk.3 slk_restore.3 curs_slk.3 slk_set.3 curs_slk.3 slk_touch.3
MLINKS+=curs_termattrs.3 baudrate.3 curs_termattrs.3 erasechar.3 \
	curs_termattrs.3 has_ic.3 curs_termattrs.3 has_il.3 \
	curs_termattrs.3 killchar.3 curs_termattrs.3 longname.3 \
	curs_termattrs.3 termattrs.3 curs_termattrs.3 termname.3
MLINKS+=curs_termcap.3 termcap.3 curs_termcap.3 tgetent.3 \
	curs_termcap.3 tgetflag.3 curs_termcap.3 tgetnum.3 \
	curs_termcap.3 tgetstr.3 curs_termcap.3 tgoto.3 \
	curs_termcap.3 tputs.3
MLINKS+=curs_terminfo.3 del_curterm.3 curs_terminfo.3 mvcur.3 \
	curs_terminfo.3 putp.3 curs_terminfo.3 restartterm.3 \
	curs_terminfo.3 set_curterm.3 curs_terminfo.3 setterm.3 \
	curs_terminfo.3 setupterm.3 curs_terminfo.3 tigetflag.3 \
	curs_terminfo.3 tigetnum.3 curs_terminfo.3 tigetstr.3 \
	curs_terminfo.3 tparm.3 curs_terminfo.3 tputs.3 \
	curs_terminfo.3 vidattr.3 curs_terminfo.3 vidputs.3
MLINKS+=curs_touch.3 is_linetouched.3 curs_touch.3 is_wintouched.3 \
	curs_touch.3 touchline.3 curs_touch.3 touchwin.3 \
	curs_touch.3 untouchwin.3 curs_touch.3 wtouchln.3
MLINKS+=curs_util.3 delay_output.3 curs_util.3 filter.3 \
	curs_util.3 flushinp.3 curs_util.3 getwin.3 \
	curs_util.3 keyname.3 curs_util.3 putwin.3 \
	curs_util.3 unctrl.3 curs_util.3 use_env.3
MLINKS+=curs_window.3 delwin.3 curs_window.3 derwin.3 curs_window.3 dupwin.3 \
	curs_window.3 mvderwin.3 curs_window.3 mvwin.3 curs_window.3 newwin.3 \
	curs_window.3 subwin.3 curs_window.3 syncok.3 \
	curs_window.3 wcursyncup.3 curs_window.3 wsyncdown.3 \
	curs_window.3 wsyncup.3
MLINKS+=default_colors.3 assume_default_colors.3 \
	default_colors.3 use_default_colors.3

.include <bsd.lib.mk>
