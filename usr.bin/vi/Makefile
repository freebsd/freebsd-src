#	@(#)Makefile	8.30 (Berkeley) 3/22/94
#	$Id: Makefile,v 1.17 1994/06/02 00:27:52 phk Exp $

PROG=	nvi
#CFLAGS=-g -DDEBUG
#CFLAGS+=-pg
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/ex -I${.CURDIR}/vi
#STRIP=
.PATH:	${.CURDIR}/ex ${.CURDIR}/sex ${.CURDIR}/vi \
	${.CURDIR}/svi ${.CURDIR}/xaw
#CLEANFILES+=ex
CLEANFILES+=vi.1 vi-ref.1
MANSRC= `pwd`
MAN1+=	vi.1 vi-ref.1

MLINKS+=vi.1 nvi.1

# General sources.
SRCS=	ascii.c cut.c delete.c exf.c intr.c line.c log.c main.c \
	mark.c options.c options_f.c put.c screen.c search.c seq.c \
	recover.c term.c timer.c trace.c util.c

# Ex source.
SRCS+=	ex.c ex_abbrev.c ex_append.c ex_args.c ex_argv.c ex_at.c \
	ex_bang.c ex_cd.c ex_delete.c ex_digraph.c ex_display.c \
	ex_edit.c ex_equal.c ex_exit.c ex_file.c ex_global.c ex_init.c \
	ex_join.c ex_map.c ex_mark.c ex_mkexrc.c ex_move.c ex_open.c \
	ex_preserve.c ex_print.c ex_put.c ex_read.c ex_screen.c \
	ex_script.c ex_set.c ex_shell.c ex_shift.c ex_source.c ex_stop.c \
	ex_subst.c ex_tag.c ex_undo.c ex_usage.c ex_util.c ex_version.c \
	ex_visual.c ex_write.c ex_yank.c ex_z.c excmd.c filter.c

# Ex screen source.
SRCS+=	sex_confirm.c sex_get.c sex_refresh.c sex_screen.c sex_term.c \
	sex_util.c

# Vi source.
SRCS+=	getc.c v_again.c v_at.c v_ch.c v_delete.c v_ex.c v_filter.c \
	v_increment.c v_init.c v_join.c v_left.c v_mark.c v_match.c \
	v_ntext.c v_paragraph.c v_put.c v_redraw.c v_replace.c v_right.c \
	v_screen.c v_scroll.c v_search.c v_section.c v_sentence.c \
	v_shift.c v_status.c v_stop.c v_switch.c v_tag.c v_text.c \
	v_ulcase.c v_undo.c v_util.c v_word.c v_xchar.c v_yank.c \
	v_z.c v_zexit.c vcmd.c vi.c

# Vi curses screen source.
SRCS+=	svi_confirm.c svi_ex.c svi_get.c svi_line.c svi_refresh.c \
	svi_relative.c svi_screen.c svi_smap.c svi_split.c svi_util.c

# Athena widget set screen source.
SRCS+=	xaw_screen.c

#LDADD+=-pg
DPADD+= ${LIBCURSES} ${LIBTERM} ${LIBUTIL}
LDADD+=	-lcurses -ltermlib -lutil
SPECHDR=excmd.h options.h
CLEANFILES+=${SPECHDR}
LINKS=	${BINDIR}/nvi ${BINDIR}/vi ${BINDIR}/nvi ${BINDIR}/ex \
	${BINDIR}/nvi ${BINDIR}/view

all: excmd.h options.h nvi vi.1 vi-ref.1
	- rm -f vi
	- rm -f ex
	- if [ ! -d vi ]; then ln nvi vi; fi
	- if [ ! -d ex ]; then ln nvi ex; fi

warn:: ${SRCS}
	-(cd ${.CURDIR} && \
	    gcc -Wall -O -DDEBUG -Iobj -Ivi -Iex -I. ${.ALLSRC} \
	    -lcurses -ltermlib 2>&1 | \
	    sed -e "/warning: .*sccsid.*defined but not used/d" \
	    	-e "/warning: suggest parentheses around/d" \
	    	-e "/In function /d" \
	    	-e "/At top level:/d" \
	    	-e "/warning: .*inline call to/d" \
	    	-e "/warning: comparison is always 1 due /d") > \
		${.CURDIR}/WARN.OUT

options.h: options.h.stub options.c # Makefile
	rm -f options.h
	cp ${.CURDIR}/options.h.stub options.h
	chmod 664 options.h
	(echo '/^\/\* O_[0-9A-Z_]*/ {'; \
	 echo 'printf("#define %s %d\n", $$2, cnt++)'; \
	 echo 'next'; \
	 echo '}'; \
	 echo 'END {'; \
	 echo 'printf("#define O_OPTIONCOUNT %d\n", cnt)'; \
	 echo '}') > /tmp/__vi.options.h
	 awk -f /tmp/__vi.options.h ${.CURDIR}/options.c >> options.h
	 rm -f /tmp/__vi.options.h

excmd.h: excmd.h.stub excmd.c # Makefile
	rm -f excmd.h
	cp ${.CURDIR}/ex/excmd.h.stub excmd.h
	chmod 664 excmd.h
	(echo '/^\/\* C_[0-9A-Z_]* \*\/$$/ {'; \
	 echo 'printf("#define %s %d\n", $$2, cnt++)'; \
	 echo 'next'; \
	 echo '}') > /tmp/__vi.excmd.h
	 awk -f /tmp/__vi.excmd.h ${.CURDIR}/ex/excmd.c >> excmd.h
	 rm -f /tmp/__vi.excmd.h

docs: vi.0.txt vi.0.ps vi.ref.txt vi.ref.ps
vi.0.txt: vi.1 set.opt.roff
	cpp -DMANUAL -I${.CURDIR}/docs ${.CURDIR}/docs/vi.1 | \
	    sed -e '/^#/d' -e '/^$$/d' | groff -man -Tascii > $@
vi.0.ps: vi.1 set.opt.roff
	cpp -DMANUAL -I${.CURDIR}/docs ${.CURDIR}/docs/vi.1 | \
	    sed -e '/^#/d' -e '/^$$/d' | groff -man > $@
vi.ref.txt: vi.ref set.opt.roff
	cpp -DREFERENCE -I${.CURDIR}/docs ${.CURDIR}/docs/vi.ref | \
	    sed -e '/^#/d' -e '/^$$/d' | groff -man -Tascii > $@
vi.ref.ps: vi.ref set.opt.roff
	cpp -DREFERENCE -I${.CURDIR}/docs ${.CURDIR}/docs/vi.ref | \
	    sed -e '/^#/d' -e '/^$$/d' | groff -man > $@

vi.1:	$(.CURDIR)/docs/vi.1 $(.CURDIR)/docs/set.opt.roff
	cpp -DMANUAL -P -I$(.CURDIR)/docs $(.CURDIR)/docs/vi.1 > vi.1

vi-ref.1:	$(.CURDIR)/docs/vi.ref $(.CURDIR)/docs/set.opt.roff
	cpp -DREFERENCE -P -I$(.CURDIR)/docs $(.CURDIR)/docs/vi.ref > vi-ref.1
.include <bsd.prog.mk>

.depend: compat.h ${SPECHDR}
