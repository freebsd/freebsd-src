#	@(#)Makefile	8.51 (Berkeley) 8/17/94

VI=	nvi
EX=	nex
VIEW=	nview
PROG=	nvi
LINKS=	${BINDIR}/${VI} ${BINDIR}/${EX} ${BINDIR}/${VI} ${BINDIR}/${VIEW}
LINKS+= ${BINDIR}/${VI} ${BINDIR}/vi ${BINDIR}/${EX} ${BINDIR}/ex
LINKS+= ${BINDIR}/${VI} ${BINDIR}/view
MAN1=	${.CURDIR}/../USD.doc/vi.man/vi.1
MLINKS+=vi.1 ex.1 vi.1 view.1
MLINKS+=vi.1 nex.1 vi.1 nview.1 vi.1 nvi.1


CFLAGS+=-I. -I${.CURDIR}
DPADD+= ${LIBCURSES} ${LIBTERMCAP} ${LIBUTIL}
LDADD+=	-lcurses -ltermcap -lutil

.PATH:	${.CURDIR}/../common ${.CURDIR}/../ex ${.CURDIR}/../sex \
	${.CURDIR}/../vi ${.CURDIR}/../svi ${.CURDIR}/../xaw

SPECHDR=compat.h excmd.h options.h

CLEANFILES+=${SPECHDR} ${EX}

# General sources.
SRCS=	cut.c delete.c exf.c line.c log.c main.c mark.c msg.c options.c \
	options_f.c put.c screen.c search.c seq.c signal.c recover.c \
	term.c trace.c util.c ${SPECHDR}

# Ex source.
SRCS+=	ex.c ex_abbrev.c ex_append.c ex_args.c ex_argv.c ex_at.c ex_bang.c \
	ex_cd.c ex_delete.c ex_digraph.c ex_display.c ex_edit.c ex_equal.c \
	ex_exit.c ex_file.c ex_global.c ex_init.c ex_join.c ex_map.c \
	ex_mark.c ex_mkexrc.c ex_move.c ex_open.c ex_preserve.c ex_print.c \
	ex_put.c ex_read.c ex_screen.c ex_script.c ex_set.c ex_shell.c \
	ex_shift.c ex_source.c ex_stop.c ex_subst.c ex_tag.c ex_undo.c \
	ex_usage.c ex_util.c ex_version.c ex_visual.c ex_write.c ex_yank.c \
	ex_z.c excmd.c filter.c

# Ex screen source.
SRCS+=	sex_confirm.c sex_get.c sex_refresh.c sex_screen.c sex_term.c \
	sex_util.c sex_window.c

# Vi source.
SRCS+=	getc.c v_ch.c v_delete.c v_ex.c v_increment.c v_init.c v_left.c \
	v_mark.c v_match.c v_ntext.c v_paragraph.c v_put.c v_redraw.c \
	v_replace.c v_right.c v_screen.c v_scroll.c v_search.c v_section.c \
	v_sentence.c v_status.c v_stop.c v_text.c v_ulcase.c v_undo.c \
	v_util.c v_word.c v_xchar.c v_yank.c v_z.c v_zexit.c vcmd.c vi.c

# Vi curses screen source.
SRCS+=	svi_confirm.c svi_curses.c svi_ex.c svi_get.c svi_line.c \
	svi_refresh.c svi_relative.c svi_screen.c svi_smap.c svi_split.c \
	svi_term.c svi_util.c

# Athena widget set screen source.
SRCS+=	xaw_screen.c

all: ${VI} ${EX}

all: nvi nex
nex: nvi
	rm -f nex
	ln nvi nex

${EX}: ${VI}
	rm -f ${EX}
	ln ${VI} ${EX}

compat.h:
	:> compat.h

excmd.h: excmd.h.stub excmd.c excmd.awk
	rm -f excmd.h
	cat ${.CURDIR}/../ex/excmd.h.stub > excmd.h
	awk -f ${.CURDIR}/../ex/excmd.awk ${.CURDIR}/../ex/excmd.c >> excmd.h

options.h: options.h.stub options.c options.awk
	rm -f options.h
	cat ${.CURDIR}/options.h.stub > options.h
	awk -f ${.CURDIR}/options.awk ${.CURDIR}/options.c >> options.h

excmd.h: excmd.h.stub excmd.c excmd.awk
	rm -f excmd.h
	cat ${.CURDIR}/../ex/excmd.h.stub > excmd.h
	awk -f ${.CURDIR}/../ex/excmd.awk ${.CURDIR}/../ex/excmd.c >> excmd.h

tags::
	-(cd ${.CURDIR} && rm -f tags && \
	    ctags ../common/*.[ch] ../common/*.stub ../ex/*.[ch] ../ex/*.stub \
	    ../vi/*.[ch] ../sex/*.[ch] ../svi/*.[ch] ../xaw/*.[ch])

warn:: ${SRCS}
	-(cd ${.CURDIR} && gcc -Wall -O4 -DDEBUG \
	    -Iobj -I. ${.ALLSRC} -lcurses -ltermcap 2>&1 | \
	    sed -e "/warning: .*sccsid.*defined but not used/d" \
	    	-e "/warning: suggest parentheses around/d" \
	    	-e "/In function /d" \
	    	-e "/At top level:/d" \
	    	-e "/warning: .*inline call to/d" \
	    	-e "/warning: comparison is always 1 due /d") > \
		${.CURDIR}/WARN.OUT

.include "../../Makefile.inc"
.include <bsd.prog.mk>

.depend: ${SPECHDR}
