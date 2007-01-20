# Makefile for libpanel
# $FreeBSD$

.include	"${.CURDIR}/../config.mk"

SRCDIR=		${NCURSES_DIR}/panel

LIB=		panel
DPADD=		${LIBNCURSES}
LDADD=		-lncurses

.PATH:		${SRCDIR}
SRCS=		\
		ncurses_def.h \
		p_above.c \
		p_below.c \
		p_bottom.c \
		p_delete.c \
		p_hidden.c \
		p_hide.c \
		p_move.c \
		p_new.c \
		p_replace.c \
		p_show.c \
		p_top.c \
		p_update.c \
		p_user.c \
		p_win.c \
		panel.c

CFLAGS+=	-I${SRCDIR}

INCS=		${SRCDIR}/panel.h

CLEANFILES+=	ncurses_def.h

# generate MAN
.PATH:		${NCURSES_DIR}/man
MANx=		\
		panel.3x

# Generate the MAN list from MANx
.for page in ${MANx}
CLEANFILES+=	${page:T:S/x$//g}
MAN+=		${page:T:S/x$//g}
${page:T:S/x$//g}: ${page}
	cat ${.ALLSRC} > ${.TARGET}
.endfor

MLINKS=		panel.3 bottom_panel.3 \
		panel.3 del_panel.3 \
		panel.3 hide_panel.3 \
		panel.3 move_panel.3 \
		panel.3 new_panel.3 \
		panel.3 panel_above.3 \
		panel.3 panel_below.3 \
		panel.3 panel_hidden.3 \
		panel.3 panel_userptr.3 \
		panel.3 panel_window.3 \
		panel.3 replace_panel.3 \
		panel.3 set_panel_userptr.3 \
		panel.3 show_panel.3 \
		panel.3 top_panel.3 \
		panel.3 update_panels.3
	
.include <bsd.lib.mk>
