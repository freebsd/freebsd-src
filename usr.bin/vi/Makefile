#
# $Id: Makefile,v 1.3 1996/11/01 08:34:51 peter Exp $
#
# This has most of the glue needed to compile tknvi and the perl hooks,
# but not all.
#

SRCDIR=		${.CURDIR}/../../contrib/nvi

TCLINTERP=	yes	#fine, in base tree, little cost
#TKNVI=		tknvi	#not ready, needs X11, tk
#PERLINTERP=	no	#not ready, needs perl5.002+

#if using ncurses:
#CFLAGS+=	-DSYSV_CURSES

VI=	nvi
EX=	nex
VIEW=	nview

PROG=	nvi

LINKS=	${BINDIR}/${VI} ${BINDIR}/${EX} ${BINDIR}/${VI} ${BINDIR}/${VIEW}
LINKS+= ${BINDIR}/${VI} ${BINDIR}/vi ${BINDIR}/${EX} ${BINDIR}/ex
LINKS+= ${BINDIR}/${VI} ${BINDIR}/view

MAN1=	${SRCDIR}/docs/USD.doc/vi.man/vi.1
MLINKS+=vi.1 ex.1 vi.1 view.1
MLINKS+=vi.1 nex.1 vi.1 nview.1 vi.1 nvi.1

CATALOGS= dutch english french german ru_SU.KOI8-R spanish swedish

.PATH:	${SRCDIR}/common
.PATH:	${SRCDIR}/ex
.PATH:	${SRCDIR}/cl
.PATH:	${SRCDIR}/vi

CFLAGS+=-I${.CURDIR} -I${SRCDIR} -I${SRCDIR}/include

DPADD+= ${LIBCURSES} ${LIBTERMCAP}
LDADD+=	-lcurses -ltermcap

.if defined(TKNVI)
.PATH:	${SRCDIR}/tk
#DPADD+=	?
#LDADD+=	-ltk
.endif

.if defined(TCLINTERP)
.PATH:	${SRCDIR}/tcl_api
DPADD+=	${LIBTCL} ${LIBM}
LDADD+=	-ltcl -lm
CFLAGS+= -DHAVE_TCL_INTERP
.endif

.if defined(PERLINTERP)
.PATH:	${SRCDIR}/perl_api
#DPADD+=	?
#LDADD+=	?
CFLAGS+= -DHAVE_PERL_INTERP
#CFLAGS+= -DHAVE_PERL_5_003_01	# If perl >= 5.03.01
.endif

CLEANFILES+=${EX}

# Vi curses sources
SRCS+=	cl_bsd.c cl_funcs.c cl_main.c cl_read.c cl_screen.c cl_term.c

# Vi Tk sources
.if defined(TKNVI)
SRCS+=	tk_funcs.c tk_main.c tk_read.c tk_screen.c tk_term.c tk_util.c
.endif

# Vi Tcl/Perl interpreter sources
.if defined(TCLINTERP) || defined(PERLINTERP)
SRCS+=	api.c
.endif
.if defined(TCLINTERP)
SRCS+=	tcl.c
.endif
.if defined(PERLINTERP)
# perl.c is generated
SRCS+=	perl.c perlsfio.c
.endif

# General sources.
SRCS+=	cut.c delete.c exf.c key.c line.c log.c main.c mark.c msg.c options.c \
	options_f.c put.c screen.c search.c seq.c recover.c util.c

# Ex source.
SRCS+=	ex.c ex_abbrev.c ex_append.c ex_args.c ex_argv.c ex_at.c ex_bang.c \
	ex_cd.c ex_cmd.c ex_cscope.c ex_delete.c ex_display.c \
	ex_edit.c ex_equal.c ex_file.c ex_filter.c ex_global.c \
	ex_init.c ex_join.c ex_map.c ex_mark.c ex_mkexrc.c ex_move.c \
	ex_open.c ex_perl.c ex_preserve.c ex_print.c ex_put.c ex_quit.c \
	ex_read.c ex_screen.c ex_script.c ex_set.c ex_shell.c \
	ex_shift.c ex_source.c ex_stop.c ex_subst.c ex_tag.c ex_tcl.c \
	ex_txt.c ex_undo.c ex_usage.c ex_util.c ex_version.c ex_visual.c \
	ex_write.c ex_yank.c ex_z.c

# Vi source.
SRCS+=	getc.c v_at.c v_ch.c v_cmd.c v_delete.c v_ex.c v_increment.c v_init.c \
	v_itxt.c v_left.c v_mark.c v_match.c v_paragraph.c v_put.c v_redraw.c \
	v_replace.c v_right.c v_screen.c v_scroll.c v_search.c v_section.c \
	v_sentence.c v_status.c v_txt.c v_ulcase.c v_undo.c \
	v_util.c v_word.c v_xchar.c v_yank.c v_z.c v_zexit.c vi.c

# Vi screen source.
SRCS+=	vs_line.c vs_msg.c vs_refresh.c vs_relative.c vs_smap.c vs_split.c

# Generate perl.c
.if defined(PERLINTERP)
perl.c:	perl.xs typemap
	echo "#define _PATH_PERLSCRIPTS \"/usr/share/vi/perl\"" > $@
	$(PERL) $(PERLLIB)/ExtUtils/xsubpp -typemap \
	    $(PERLLIB)/ExtUtils/typemap $(SRCDIR)/perl_api/perl.xs >> $@
	($(PERL) -ne 'print "sub $$1 {\$$curscr->$$1(\@_)}\n" \
	    if /newXS\("VI::([^":]*)"/;' $@ ; echo "1;") > VI.pm

CLEANFILES+=	VI.pm perl.c
.endif

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${CATALOGS:S;^;${SRCDIR}/catalog/;} \
		${DESTDIR}/usr/share/vi/catalog
.if defined(PERLINTERP)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${SRCDIR}/perl_scripts/*.pl VI.pm \
		${DESTDIR}/usr/share/vi/perl
.endif
.if defined(TCLINTERP)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${SRCDIR}/tcl_scripts/*.tcl \
		${DESTDIR}/usr/share/vi/tcl
.endif

.include <bsd.prog.mk>
