SHLIBDIR?=	/lib

.include <src.opts.mk>

.include "${.CURDIR:H}/config.mk"

LIB=		ncursesw
SHLIB_MAJOR=	9

GENSRCS=	\
	expanded.c \
	lib_gen.c

SRCS=	${GENSRCS}

.PATH: ${NCURSES_DIR}/ncurses/base
SRCS+=	\
	new_pair.c \
	lib_dft_fgbg.c \
	legacy_coding.c \
	lib_addch.c \
	lib_addstr.c \
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
	lib_delch.c \
	lib_delwin.c \
	lib_echo.c \
	lib_endwin.c \
	lib_erase.c \
	lib_flash.c \
	lib_getch.c \
	lib_getstr.c \
	lib_hline.c \
	lib_immedok.c \
	lib_inchstr.c \
	lib_initscr.c \
	lib_insch.c \
	lib_insdel.c \
	lib_insnstr.c \
	lib_instr.c \
	lib_isendwin.c \
	lib_leaveok.c \
	lib_mouse.c \
	lib_move.c \
	lib_mvwin.c \
	lib_newterm.c \
	lib_newwin.c \
	lib_nl.c \
	lib_overlay.c \
	lib_pad.c \
	lib_print.c \
	lib_printw.c \
	lib_redrawln.c \
	lib_refresh.c \
	lib_restart.c \
	lib_scanw.c \
	lib_screen.c \
	lib_scroll.c \
	lib_scrollok.c \
	lib_scrreg.c \
	lib_set_term.c \
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
	lib_touch.c \
	lib_ungetch.c \
	lib_vline.c \
	lib_wattroff.c \
	lib_wattron.c \
	lib_winch.c \
	lib_window.c \
	nc_panel.c \
	resizeterm.c \
	safe_sprintf.c \
	vsscanf.c \
	wresize.c \
	lib_freeall.c \

.PATH: ${NCURSES_DIR}/ncurses/tinfo
SRCS+=	\
	use_screen.c \
	use_window.c

.PATH: ${NCURSES_DIR}/ncurses/trace
SRCS+=	\
	varargs.c

# DONE
.PATH: ${NCURSES_DIR}/ncurses/tty
SRCS+=	\
	hardscroll.c \
	hashmap.c \
	lib_mvcur.c \
	lib_tstp.c \
	lib_vidattr.c \
	tty_update.c

.PATH: ${NCURSES_DIR}/ncurses/widechar
SRCS+=	\
	charable.c \
	lib_add_wch.c \
	lib_box_set.c \
	lib_cchar.c \
	lib_erasewchar.c \
	lib_get_wch.c \
	lib_get_wstr.c \
	lib_hline_set.c \
	lib_in_wch.c \
	lib_in_wchnstr.c \
	lib_ins_wch.c \
	lib_inwstr.c \
	lib_key_name.c \
	lib_pecho_wchar.c \
	lib_slk_wset.c \
	lib_unget_wch.c \
	lib_vid_attr.c \
	lib_vline_set.c \
	lib_wacs.c \
	lib_wunctrl.c

# Currently unused, for debugging libncurses itself.
DBGSRCS= \
	varargs.c

CLEANFILES=	${GENSRCS}

CWARNFLAGS.lib_set_term.c= ${NO_WUNUSED_BUT_SET_VARIABLE}

.if ${MK_INSTALLLIB} != "no"
SYMLINKS+=	libncursesw.a ${LIBDIR}/libcursesw.a
# backward compat
SYMLINKS+=	libncursesw.a ${LIBDIR}/libncurses.a
SYMLINKS+=	libncursesw.a ${LIBDIR}/libcurses.a
.endif
.if !defined(NO_PIC)
# no need for major at all, it's an ld-time redirection only
SYMLINKS+=	libncursesw.so ${LIBDIR}/libcursesw.so
# backward compat
SYMLINKS+=	libncursesw.so ${LIBDIR}/libncurses.so
SYMLINKS+=	libncursesw.so ${LIBDIR}/libcurses.so
.endif

LIBADD+=	tinfow
SHLIB_LDSCRIPT=	libncursesw.ldscript
STATIC_LDSCRIPT= libncursesw.aldscript
CLEANFILES+= libncursesw.ald
# Keep this for a while to ensure the file is removed during make clean
CLEANFILES+= libncursesw.a

libncursesw.ald: ${.CURDIR}/${STATIC_LDSCRIPT}
	sed -e 's,@@LIB@@,${LIB},g' \
	    -e 's,@@STATICLIB_SUFFIX@@,${_STATICLIB_SUFFIX},g' \
	    ${.ALLSRC} > ${.TARGET}

lib_gen.c: MKlib_gen.sh ${TINFO_OBJDIR}/curses.h ncurses_dll.h
	LC_ALL=C sh ${NCURSES_DIR}/ncurses/base/MKlib_gen.sh "${CPP:N${CCACHE_BIN}} ${CFLAGS}" \
	       "${AWK}" generated < ${TINFO_OBJDIR}/curses.h >$@

expanded.c: MKexpanded.sh
	sh ${NCURSES_DIR}/ncurses/tty/MKexpanded.sh "${CC:N${CCACHE_BIN}} -E" ${CFLAGS} >expanded.c

all: ${STATIC_LDSCRIPT} libncursesw.ald

install-libncursesw.a: libncursesw.ald
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},dev} -S -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} libncursesw.ald ${DESTDIR}${_LIBDIR}/lib${LIB}.a

realinstall: install-libncursesw.a


.include <bsd.lib.mk>
