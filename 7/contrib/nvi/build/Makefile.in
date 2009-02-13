#	@(#)Makefile.in	8.75 (Berkeley) 10/23/96

srcdir=	@srcdir@/..
CC=	@CC@
OPTFLAG=@OPTFLAG@
CFLAGS=	-c $(OPTFLAG) @CFLAGS@ -I. -I$(srcdir)/include @CPPFLAGS@ -DGTAGS
LDFLAGS=@LDFLAGS@
PERL=	@vi_cv_path_perl@
PERLLIB=@vi_cv_perllib@
SHRPENV=@shrpenv@

# Objects
COBJS=	addbytes.o addch.o box.o clear.o clrtobot.o clrtoeol.o \
	cr_put.o ctrace.o cur_hash.o curses.o delch.o deleteln.o delwin.o \
	erase.o fullname.o getch.o getstr.o id_subwins.o idlok.o initscr.o \
	insch.o insertln.o longname.o move.o mvwin.o newwin.o overlay.o \
	overwrite.o putchar.o refresh.o scroll.o setterm.o standout.o \
	toucholap.o touchwin.o tscroll.o tstp.o tty.o unctrl.o waddnstr.o
CLOBJS=	cl_bsd.o cl_funcs.o cl_main.o cl_read.o cl_screen.o cl_term.o
DBOBJS=	db.o mpool.o \
	bt_close.o bt_conv.o bt_debug.o bt_delete.o bt_get.o bt_open.o \
	bt_overflow.o bt_page.o bt_put.o bt_search.o bt_seq.o \
	bt_split.o bt_utils.o \
	rec_close.o rec_delete.o rec_get.o rec_open.o rec_put.o \
	rec_search.o rec_seq.o rec_utils.o
REOBJS=	regcomp.o regerror.o regexec.o regfree.o
TKOBJS=	tk_funcs.o tk_main.o tk_read.o tk_screen.o tk_term.o tk_util.o
VIOBJS= cut.o delete.o ex.o ex_abbrev.o ex_append.o ex_args.o ex_argv.o \
	ex_at.o ex_bang.o ex_cd.o ex_cmd.o ex_cscope.o ex_delete.o \
	ex_display.o ex_edit.o ex_equal.o ex_file.o ex_filter.o \
	ex_global.o ex_init.o ex_join.o ex_map.o ex_mark.o ex_mkexrc.o \
	ex_move.o ex_open.o ex_perl.o ex_preserve.o ex_print.o ex_put.o \
	ex_quit.o ex_read.o ex_screen.o ex_script.o ex_set.o ex_shell.o \
	ex_shift.o ex_source.o ex_stop.o ex_subst.o ex_tag.o ex_tcl.o \
	ex_txt.o ex_undo.o ex_usage.o ex_util.o ex_version.o ex_visual.o \
	ex_write.o ex_yank.o ex_z.o exf.o getc.o key.o line.o log.o main.o \
	mark.o msg.o options.o options_f.o put.o recover.o screen.o \
	search.o seq.o util.o v_at.o v_ch.o v_cmd.o v_delete.o v_ex.o \
	v_increment.o v_init.o v_itxt.o v_left.o v_mark.o v_match.o \
	v_paragraph.o v_put.o v_redraw.o v_replace.o v_right.o v_screen.o \
	v_scroll.o v_search.o v_section.o v_sentence.o v_status.o v_txt.o \
	v_ulcase.o v_undo.o v_util.o v_word.o v_xchar.o v_yank.o v_z.o \
	v_zexit.o vi.o vs_line.o vs_msg.o vs_refresh.o vs_relative.o \
	vs_smap.o vs_split.o

all: nvi @tknvi@

NVIALL=	$(CLOBJS) $(VIOBJS) @cobjs@ @LIBOBJS@
nvi nex: $(NVIALL)
	$(SHRPENV) $(CC) $(LDFLAGS) -o $@ $(NVIALL) @LIBS@
	-rm -f nex
	ln $@ nex

TKALL=	$(TKOBJS) $(VIOBJS) @LIBOBJS@
tknvi: $(TKALL)
	$(SHRPENV) $(CC) $(LDFLAGS) -o $@ $(TKALL) @TKLIBS@

chmod=	@vi_cv_path_chmod@
cp=	@vi_cv_path_cp@
ln=	@vi_cv_path_ln@
mkdir=	@vi_cv_path_mkdir@
rm=	@vi_cv_path_rm@
strip=	@vi_cv_path_strip@

prefix=	@prefix@
bindir=	@bindir@
datadir=@datadir@
mandir=	@mandir@
exec_prefix=@exec_prefix@

dmode=	755
emode=	555
fmode=	444

transform=@program_transform_name@

install: all install_common
	@echo "Installing vi, ex, view: $(bindir) ..."
	[ -d $(bindir) ] || \
	    ($(mkdir) $(bindir) && $(chmod) $(dmode) $(bindir))
	cd $(bindir) && $(rm) -f `echo vi | sed '$(transform)'`
	$(cp) nvi $(bindir)/`echo vi | sed '$(transform)'`
	cd $(bindir) && [ -f $(strip) ] && \
	    $(strip) `echo vi | sed '$(transform)'`
	cd $(bindir) && $(chmod) $(emode) `echo vi | sed '$(transform)'`
	cd $(bindir) && $(rm) -f `echo ex | sed '$(transform)'`
	cd $(bindir) && $(rm) -f `echo view | sed '$(transform)'`
	cd $(bindir) && $(ln) \
		`echo vi | sed '$(transform)'` `echo ex | sed '$(transform)'`
	cd $(bindir) && $(ln) \
		`echo vi | sed '$(transform)'` `echo view | sed '$(transform)'`
	[ -d $(mandir) ] || \
	    ($(mkdir) $(mandir) && $(chmod) $(dmode) $(mandir))
	[ -d $(mandir)/cat1 ] || \
	    ($(mkdir) $(mandir)/cat1 && $(chmod) $(dmode) $(mandir)/cat1)
	@echo "Installing man pages: $(mandir) ..."
	cd $(mandir)/cat1 && $(rm) -f `echo vi.0 | sed '$(transform)'`
	$(cp) $(srcdir)/docs/USD.doc/vi.man/vi.0 \
	    $(mandir)/cat1/`echo vi.0 | sed '$(transform)'`
	cd $(mandir)/cat1 && $(chmod) $(fmode) `echo vi.0 | sed '$(transform)'`
	cd $(mandir)/cat1 && $(rm) -f `echo ex.0 | sed '$(transform)'`
	cd $(mandir)/cat1 && $(rm) -f `echo view.0 | sed '$(transform)'`
	cd $(mandir)/cat1 && $(ln) \
	    `echo vi.0 | sed '$(transform)'` `echo ex.0 | sed '$(transform)'`
	cd $(mandir)/cat1 && $(ln) \
	    `echo vi.0 | sed '$(transform)'` `echo view.0 | sed '$(transform)'`
	[ -d $(mandir)/man1 ] || \
	    ($(mkdir) $(mandir)/man1 && $(chmod) $(dmode) $(mandir)/man1)
	cd $(mandir)/man1 && $(rm) -f `echo vi.1 | sed '$(transform)'`
	$(cp) $(srcdir)/docs/USD.doc/vi.man/vi.1 \
	    $(mandir)/man1/`echo vi.1 | sed '$(transform)'`
	cd $(mandir)/man1 && $(chmod) $(fmode) `echo vi.1 | sed '$(transform)'`
	cd $(mandir)/man1 && $(rm) -f `echo ex.1 | sed '$(transform)'`
	cd $(mandir)/man1 && $(rm) -f `echo view.1 | sed '$(transform)'`
	cd $(mandir)/man1 && $(ln) \
	    `echo vi.1 | sed '$(transform)'` `echo ex.1 | sed '$(transform)'`
	cd $(mandir)/man1 && $(ln) \
	    `echo vi.1 | sed '$(transform)'` `echo view.1 | sed '$(transform)'`

cat=	dutch english french german ru_SU.KOI8-R spanish swedish
install_common:
	[ -f $(chmod) ]
	[ -f $(cp) ]
	[ -f $(ln) ]
	[ -f $(mkdir) ]
	[ -f $(rm) ]
	[ -d $(prefix) ] || \
	    ($(mkdir) $(prefix) && $(chmod) $(dmode) $(prefix))
	[ -d $(exec_prefix) ] || \
	    ($(mkdir) $(exec_prefix) && $(chmod) $(dmode) $(exec_prefix))
	[ -d $(datadir) ] || \
	    ($(mkdir) $(datadir) && $(chmod) $(dmode) $(datadir))
	$(rm) -rf $(datadir)/vi
	$(mkdir) $(datadir)/vi && $(chmod) $(dmode) $(datadir)/vi
	@echo "Installing message catalogs: $(datadir)/vi/catalog ..."
	$(mkdir) $(datadir)/vi/catalog && \
	    $(chmod) $(dmode) $(datadir)/vi/catalog
	(cd $(srcdir)/catalog && $(cp) $(cat) $(datadir)/vi/catalog && \
	    cd $(datadir)/vi/catalog && $(chmod) $(fmode) *)
	@echo "Installing Perl scripts: $(datadir)/vi/perl ..."
	$(mkdir) $(datadir)/vi/perl && $(chmod) $(dmode) $(datadir)/vi/perl
	[ -f VI.pm ] && $(cp) VI.pm $(datadir)/vi/perl && \
	    cd $(datadir)/vi/perl && $(chmod) $(fmode) VI.pm)
	(cd $(srcdir)/perl_scripts && $(cp) *.pl $(datadir)/vi/perl && \
	    cd $(datadir)/vi/perl && $(chmod) $(fmode) *.pl)
	@echo "Installing Tcl scripts: $(datadir)/vi/tcl ..."
	$(mkdir) $(datadir)/vi/tcl && $(chmod) $(dmode) $(datadir)/vi/tcl
	(cd $(srcdir)/tcl_scripts && $(cp) *.tcl $(datadir)/vi/tcl && \
	    cd $(datadir)/vi/tcl && $(chmod) $(fmode) *.tcl)
	@echo "Installing recover script: $(datadir)/vi/recover ..."
	($(cp) recover $(datadir)/vi/recover && \
	    $(chmod) $(emode) $(datadir)/vi/recover)

uninstall:
	$(rm) -rf $(datadir)/vi
	cd $(bindir) && $(rm) -f `echo ex | sed '$(transform)'`
	cd $(bindir) && $(rm) -f `echo vi | sed '$(transform)'`
	cd $(bindir) && $(rm) -f `echo view | sed '$(transform)'`
	cd $(mandir)/cat1 && $(rm) -f `echo ex.0 | sed '$(transform)'`
	cd $(mandir)/cat1 && $(rm) -f `echo vi.0 | sed '$(transform)'`
	cd $(mandir)/cat1 && $(rm) -f `echo view.0 | sed '$(transform)'`
	cd $(mandir)/man1 && $(rm) -f `echo ex.1 | sed '$(transform)'`
	cd $(mandir)/man1 && $(rm) -f `echo vi.1 | sed '$(transform)'`
	cd $(mandir)/man1 && $(rm) -f `echo view.1 | sed '$(transform)'`

docs:
	cd $(srcdir)/docs/USD.doc/vi.ref && $(MAKE)
	cd $(srcdir)/docs/USD.doc/vi.man && $(MAKE)
	cd $(srcdir)/docs/USD.doc/edit && $(MAKE)
	cd $(srcdir)/docs/USD.doc/exref && $(MAKE)
	cd $(srcdir)/docs/USD.doc/vitut && $(MAKE)

clean:
	-rm -f *.core *.o memcpy.c perl.c
	-rm -f nex nvi tknvi core
	-rm -f $(COBJS) $(CLOBJS) $(DBOBJS) $(REOBJS)
	-rm -f $(TKOBJS) $(VIOBJS)

cleandocs:
	cd $(srcdir)/docs/USD.doc/vi.ref && $(MAKE) clean
	cd $(srcdir)/docs/USD.doc/vi.man && $(MAKE) clean
	cd $(srcdir)/docs/USD.doc/edit && $(MAKE) clean
	cd $(srcdir)/docs/USD.doc/exref && $(MAKE) clean
	cd $(srcdir)/docs/USD.doc/vitut && $(MAKE) clean

distclean maintainer-clean realclean: clean cleandocs
	-rm -f Makefile config.cache config.h config.log config.status
	-rm -f pathnames.h port.h

# Vi curses sources.
cl_bsd.o: $(srcdir)/cl/cl_bsd.c
	$(CC) $(CFLAGS) $?
cl_funcs.o: $(srcdir)/cl/cl_funcs.c
	$(CC) $(CFLAGS) $?
cl_main.o: $(srcdir)/cl/cl_main.c
	$(CC) $(CFLAGS) $?
cl_read.o: $(srcdir)/cl/cl_read.c
	$(CC) $(CFLAGS) $?
cl_screen.o: $(srcdir)/cl/cl_screen.c
	$(CC) $(CFLAGS) $?
cl_term.o: $(srcdir)/cl/cl_term.c
	$(CC) $(CFLAGS) $?

# Vi Tk sources.
tk_funcs.o: $(srcdir)/tk/tk_funcs.c
	$(CC) $(CFLAGS) @XINCS@ $?
tk_main.o: $(srcdir)/tk/tk_main.c
	$(CC) $(CFLAGS) @XINCS@ $?
tk_read.o: $(srcdir)/tk/tk_read.c
	$(CC) $(CFLAGS) @XINCS@ $?
tk_screen.o: $(srcdir)/tk/tk_screen.c
	$(CC) $(CFLAGS) @XINCS@ $?
tk_term.o: $(srcdir)/tk/tk_term.c
	$(CC) $(CFLAGS) @XINCS@ $?
tk_util.o: $(srcdir)/tk/tk_util.c
	$(CC) $(CFLAGS) @XINCS@ $?

# Vi Tcl/Perl interpreter sources.
api.o: $(srcdir)/common/api.c
	$(CC) $(CFLAGS) $?
perl.c: $(srcdir)/perl_api/perl.xs $(srcdir)/perl_api/typemap
	echo "#define _PATH_PERLSCRIPTS \"$(datadir)/vi/perl\"" > $@
	$(PERL) $(PERLLIB)/ExtUtils/xsubpp -typemap \
	    $(PERLLIB)/ExtUtils/typemap $(srcdir)/perl_api/perl.xs >> $@
	($(PERL) -ne 'print "sub $$1 {\$$curscr->$$1(\@_)}\n" \
	    if /newXS\("VI::([^":]*)"/;' $@ ; echo "1;") > VI.pm
perl.o: perl.c
	$(CC) $(CFLAGS) $?
perlsfio.o: $(srcdir)/perl_api/perlsfio.c
	$(CC) $(CFLAGS) $?
tcl.o: $(srcdir)/tcl_api/tcl.c
	$(CC) $(CFLAGS) $?

# Vi sources.
cut.o: $(srcdir)/common/cut.c
	$(CC) $(CFLAGS) $?
delete.o: $(srcdir)/common/delete.c
	$(CC) $(CFLAGS) $?
exf.o: $(srcdir)/common/exf.c
	$(CC) $(CFLAGS) $?
key.o: $(srcdir)/common/key.c
	$(CC) $(CFLAGS) $?
line.o: $(srcdir)/common/line.c
	$(CC) $(CFLAGS) $?
log.o: $(srcdir)/common/log.c
	$(CC) $(CFLAGS) $?
main.o: $(srcdir)/common/main.c
	$(CC) $(CFLAGS) $?
mark.o: $(srcdir)/common/mark.c
	$(CC) $(CFLAGS) $?
msg.o: $(srcdir)/common/msg.c
	$(CC) $(CFLAGS) $?
options.o: $(srcdir)/common/options.c
	$(CC) $(CFLAGS) $?
options_f.o: $(srcdir)/common/options_f.c
	$(CC) $(CFLAGS) $?
put.o: $(srcdir)/common/put.c
	$(CC) $(CFLAGS) $?
screen.o: $(srcdir)/common/screen.c
	$(CC) $(CFLAGS) $?
search.o: $(srcdir)/common/search.c
	$(CC) $(CFLAGS) $?
seq.o: $(srcdir)/common/seq.c
	$(CC) $(CFLAGS) $?
recover.o: $(srcdir)/common/recover.c
	$(CC) $(CFLAGS) $?
util.o: $(srcdir)/common/util.c
	$(CC) $(CFLAGS) $?
ex.o: $(srcdir)/ex/ex.c
	$(CC) $(CFLAGS) $?
ex_abbrev.o: $(srcdir)/ex/ex_abbrev.c
	$(CC) $(CFLAGS) $?
ex_append.o: $(srcdir)/ex/ex_append.c
	$(CC) $(CFLAGS) $?
ex_args.o: $(srcdir)/ex/ex_args.c
	$(CC) $(CFLAGS) $?
ex_argv.o: $(srcdir)/ex/ex_argv.c
	$(CC) $(CFLAGS) $?
ex_at.o: $(srcdir)/ex/ex_at.c
	$(CC) $(CFLAGS) $?
ex_bang.o: $(srcdir)/ex/ex_bang.c
	$(CC) $(CFLAGS) $?
ex_cd.o: $(srcdir)/ex/ex_cd.c
	$(CC) $(CFLAGS) $?
ex_cmd.o: $(srcdir)/ex/ex_cmd.c
	$(CC) $(CFLAGS) $?
ex_cscope.o: $(srcdir)/ex/ex_cscope.c
	$(CC) $(CFLAGS) $?
ex_delete.o: $(srcdir)/ex/ex_delete.c
	$(CC) $(CFLAGS) $?
ex_digraph.o: $(srcdir)/ex/ex_digraph.c
	$(CC) $(CFLAGS) $?
ex_display.o: $(srcdir)/ex/ex_display.c
	$(CC) $(CFLAGS) $?
ex_edit.o: $(srcdir)/ex/ex_edit.c
	$(CC) $(CFLAGS) $?
ex_equal.o: $(srcdir)/ex/ex_equal.c
	$(CC) $(CFLAGS) $?
ex_file.o: $(srcdir)/ex/ex_file.c
	$(CC) $(CFLAGS) $?
ex_filter.o: $(srcdir)/ex/ex_filter.c
	$(CC) $(CFLAGS) $?
ex_global.o: $(srcdir)/ex/ex_global.c
	$(CC) $(CFLAGS) $?
ex_init.o: $(srcdir)/ex/ex_init.c
	$(CC) $(CFLAGS) $?
ex_join.o: $(srcdir)/ex/ex_join.c
	$(CC) $(CFLAGS) $?
ex_map.o: $(srcdir)/ex/ex_map.c
	$(CC) $(CFLAGS) $?
ex_mark.o: $(srcdir)/ex/ex_mark.c
	$(CC) $(CFLAGS) $?
ex_mkexrc.o: $(srcdir)/ex/ex_mkexrc.c
	$(CC) $(CFLAGS) $?
ex_move.o: $(srcdir)/ex/ex_move.c
	$(CC) $(CFLAGS) $?
ex_open.o: $(srcdir)/ex/ex_open.c
	$(CC) $(CFLAGS) $?
ex_perl.o: $(srcdir)/ex/ex_perl.c
	$(CC) $(CFLAGS) $?
ex_preserve.o: $(srcdir)/ex/ex_preserve.c
	$(CC) $(CFLAGS) $?
ex_print.o: $(srcdir)/ex/ex_print.c
	$(CC) $(CFLAGS) $?
ex_put.o: $(srcdir)/ex/ex_put.c
	$(CC) $(CFLAGS) $?
ex_quit.o: $(srcdir)/ex/ex_quit.c
	$(CC) $(CFLAGS) $?
ex_read.o: $(srcdir)/ex/ex_read.c
	$(CC) $(CFLAGS) $?
ex_screen.o: $(srcdir)/ex/ex_screen.c
	$(CC) $(CFLAGS) $?
ex_script.o: $(srcdir)/ex/ex_script.c
	$(CC) $(CFLAGS) $?
ex_set.o: $(srcdir)/ex/ex_set.c
	$(CC) $(CFLAGS) $?
ex_shell.o: $(srcdir)/ex/ex_shell.c
	$(CC) $(CFLAGS) $?
ex_shift.o: $(srcdir)/ex/ex_shift.c
	$(CC) $(CFLAGS) $?
ex_source.o: $(srcdir)/ex/ex_source.c
	$(CC) $(CFLAGS) $?
ex_stop.o: $(srcdir)/ex/ex_stop.c
	$(CC) $(CFLAGS) $?
ex_subst.o: $(srcdir)/ex/ex_subst.c
	$(CC) $(CFLAGS) $?
ex_tag.o: $(srcdir)/ex/ex_tag.c
	$(CC) $(CFLAGS) $?
ex_tcl.o: $(srcdir)/ex/ex_tcl.c
	$(CC) $(CFLAGS) $?
ex_txt.o: $(srcdir)/ex/ex_txt.c
	$(CC) $(CFLAGS) $?
ex_undo.o: $(srcdir)/ex/ex_undo.c
	$(CC) $(CFLAGS) $?
ex_usage.o: $(srcdir)/ex/ex_usage.c
	$(CC) $(CFLAGS) $?
ex_util.o: $(srcdir)/ex/ex_util.c
	$(CC) $(CFLAGS) $?
ex_version.o: $(srcdir)/ex/ex_version.c
	$(CC) $(CFLAGS) $?
ex_visual.o: $(srcdir)/ex/ex_visual.c
	$(CC) $(CFLAGS) $?
ex_write.o: $(srcdir)/ex/ex_write.c
	$(CC) $(CFLAGS) $?
ex_yank.o: $(srcdir)/ex/ex_yank.c
	$(CC) $(CFLAGS) $?
ex_z.o: $(srcdir)/ex/ex_z.c
	$(CC) $(CFLAGS) $?
getc.o: $(srcdir)/vi/getc.c
	$(CC) $(CFLAGS) $?
v_at.o: $(srcdir)/vi/v_at.c
	$(CC) $(CFLAGS) $?
v_ch.o: $(srcdir)/vi/v_ch.c
	$(CC) $(CFLAGS) $?
v_cmd.o: $(srcdir)/vi/v_cmd.c
	$(CC) $(CFLAGS) $?
v_delete.o: $(srcdir)/vi/v_delete.c
	$(CC) $(CFLAGS) $?
v_ex.o: $(srcdir)/vi/v_ex.c
	$(CC) $(CFLAGS) $?
v_increment.o: $(srcdir)/vi/v_increment.c
	$(CC) $(CFLAGS) $?
v_init.o: $(srcdir)/vi/v_init.c
	$(CC) $(CFLAGS) $?
v_itxt.o: $(srcdir)/vi/v_itxt.c
	$(CC) $(CFLAGS) $?
v_left.o: $(srcdir)/vi/v_left.c
	$(CC) $(CFLAGS) $?
v_mark.o: $(srcdir)/vi/v_mark.c
	$(CC) $(CFLAGS) $?
v_match.o: $(srcdir)/vi/v_match.c
	$(CC) $(CFLAGS) $?
v_paragraph.o: $(srcdir)/vi/v_paragraph.c
	$(CC) $(CFLAGS) $?
v_put.o: $(srcdir)/vi/v_put.c
	$(CC) $(CFLAGS) $?
v_redraw.o: $(srcdir)/vi/v_redraw.c
	$(CC) $(CFLAGS) $?
v_replace.o: $(srcdir)/vi/v_replace.c
	$(CC) $(CFLAGS) $?
v_right.o: $(srcdir)/vi/v_right.c
	$(CC) $(CFLAGS) $?
v_screen.o: $(srcdir)/vi/v_screen.c
	$(CC) $(CFLAGS) $?
v_scroll.o: $(srcdir)/vi/v_scroll.c
	$(CC) $(CFLAGS) $?
v_search.o: $(srcdir)/vi/v_search.c
	$(CC) $(CFLAGS) $?
v_section.o: $(srcdir)/vi/v_section.c
	$(CC) $(CFLAGS) $?
v_sentence.o: $(srcdir)/vi/v_sentence.c
	$(CC) $(CFLAGS) $?
v_status.o: $(srcdir)/vi/v_status.c
	$(CC) $(CFLAGS) $?
v_txt.o: $(srcdir)/vi/v_txt.c
	$(CC) -c @no_op_OPTFLAG@ @CFLAGS@ -I. -I$(srcdir)/include @CPPFLAGS@ $?
v_ulcase.o: $(srcdir)/vi/v_ulcase.c
	$(CC) $(CFLAGS) $?
v_undo.o: $(srcdir)/vi/v_undo.c
	$(CC) $(CFLAGS) $?
v_util.o: $(srcdir)/vi/v_util.c
	$(CC) $(CFLAGS) $?
v_word.o: $(srcdir)/vi/v_word.c
	$(CC) $(CFLAGS) $?
v_xchar.o: $(srcdir)/vi/v_xchar.c
	$(CC) $(CFLAGS) $?
v_yank.o: $(srcdir)/vi/v_yank.c
	$(CC) $(CFLAGS) $?
v_z.o: $(srcdir)/vi/v_z.c
	$(CC) $(CFLAGS) $?
v_zexit.o: $(srcdir)/vi/v_zexit.c
	$(CC) $(CFLAGS) $?
vi.o: $(srcdir)/vi/vi.c
	$(CC) $(CFLAGS) $?
vs_line.o: $(srcdir)/vi/vs_line.c
	$(CC) $(CFLAGS) $?
vs_msg.o: $(srcdir)/vi/vs_msg.c
	$(CC) $(CFLAGS) $?
vs_refresh.o: $(srcdir)/vi/vs_refresh.c
	$(CC) $(CFLAGS) $?
vs_relative.o: $(srcdir)/vi/vs_relative.c
	$(CC) $(CFLAGS) $?
vs_smap.o: $(srcdir)/vi/vs_smap.c
	$(CC) $(CFLAGS) $?
vs_split.o: $(srcdir)/vi/vs_split.c
	$(CC) $(CFLAGS) $?

addbytes.o: $(srcdir)/curses/addbytes.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
addch.o: $(srcdir)/curses/addch.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
box.o: $(srcdir)/curses/box.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
clear.o: $(srcdir)/curses/clear.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
clrtobot.o: $(srcdir)/curses/clrtobot.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
clrtoeol.o: $(srcdir)/curses/clrtoeol.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
cr_put.o: $(srcdir)/curses/cr_put.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
ctrace.o: $(srcdir)/curses/ctrace.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
cur_hash.o: $(srcdir)/curses/cur_hash.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
curses.o: $(srcdir)/curses/curses.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
delch.o: $(srcdir)/curses/delch.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
deleteln.o: $(srcdir)/curses/deleteln.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
delwin.o: $(srcdir)/curses/delwin.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
erase.o: $(srcdir)/curses/erase.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
fullname.o: $(srcdir)/curses/fullname.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
getch.o: $(srcdir)/curses/getch.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
getstr.o: $(srcdir)/curses/getstr.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
id_subwins.o: $(srcdir)/curses/id_subwins.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
idlok.o: $(srcdir)/curses/idlok.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
initscr.o: $(srcdir)/curses/initscr.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
insch.o: $(srcdir)/curses/insch.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
insertln.o: $(srcdir)/curses/insertln.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
longname.o: $(srcdir)/curses/longname.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
move.o: $(srcdir)/curses/move.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
mvwin.o: $(srcdir)/curses/mvwin.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
newwin.o: $(srcdir)/curses/newwin.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
overlay.o: $(srcdir)/curses/overlay.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
overwrite.o: $(srcdir)/curses/overwrite.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
putchar.o: $(srcdir)/curses/putchar.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
refresh.o: $(srcdir)/curses/refresh.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
scroll.o: $(srcdir)/curses/scroll.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
setterm.o: $(srcdir)/curses/setterm.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
standout.o: $(srcdir)/curses/standout.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
toucholap.o: $(srcdir)/curses/toucholap.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
touchwin.o: $(srcdir)/curses/touchwin.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
tscroll.o: $(srcdir)/curses/tscroll.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
tstp.o: $(srcdir)/curses/tstp.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
tty.o: $(srcdir)/curses/tty.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
unctrl.o: $(srcdir)/curses/unctrl.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?
waddnstr.o: $(srcdir)/curses/waddnstr.c
	$(CC) -D_CURSES_PRIVATE $(CFLAGS) $?

# DB sources.
db.o: $(srcdir)/db/db/db.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) $?
mpool.o: $(srcdir)/db/mpool/mpool.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/mpool $?
bt_close.o: $(srcdir)/db/btree/bt_close.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_conv.o: $(srcdir)/db/btree/bt_conv.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_debug.o: $(srcdir)/db/btree/bt_debug.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_delete.o: $(srcdir)/db/btree/bt_delete.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_get.o: $(srcdir)/db/btree/bt_get.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_open.o: $(srcdir)/db/btree/bt_open.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_overflow.o: $(srcdir)/db/btree/bt_overflow.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_page.o: $(srcdir)/db/btree/bt_page.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_put.o: $(srcdir)/db/btree/bt_put.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_search.o: $(srcdir)/db/btree/bt_search.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_seq.o: $(srcdir)/db/btree/bt_seq.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_split.o: $(srcdir)/db/btree/bt_split.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
bt_utils.o: $(srcdir)/db/btree/bt_utils.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/btree $?
rec_close.o: $(srcdir)/db/recno/rec_close.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/recno $?
rec_delete.o: $(srcdir)/db/recno/rec_delete.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/recno $?
rec_get.o: $(srcdir)/db/recno/rec_get.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/recno $?
rec_open.o: $(srcdir)/db/recno/rec_open.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/recno $?
rec_put.o: $(srcdir)/db/recno/rec_put.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/recno $?
rec_search.o: $(srcdir)/db/recno/rec_search.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/recno $?
rec_seq.o: $(srcdir)/db/recno/rec_seq.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/recno $?
rec_utils.o: $(srcdir)/db/recno/rec_utils.c
	$(CC) -D__DBINTERFACE_PRIVATE $(CFLAGS) -I$(srcdir)/db/recno $?

# Regular Expressions sources.
regcomp.o: $(srcdir)/regex/regcomp.c
	$(CC) -D__REGEX_PRIVATE $(CFLAGS) $?
regerror.o: $(srcdir)/regex/regerror.c
	$(CC) -D__REGEX_PRIVATE $(CFLAGS) $?
regexec.o: $(srcdir)/regex/regexec.c
	$(CC) -D__REGEX_PRIVATE $(CFLAGS) $?
regfree.o: $(srcdir)/regex/regfree.c
	$(CC) -D__REGEX_PRIVATE $(CFLAGS) $?

# Random replacement and workaround sources.
addnstr.o: $(srcdir)/clib/addnstr.c
	$(CC) $(CFLAGS) $?
bsearch.o: $(srcdir)/clib/bsearch.c
	$(CC) $(CFLAGS) $?
env.o: $(srcdir)/clib/env.c
	$(CC) $(CFLAGS) $?
fchmod.o: $(srcdir)/clib/fchmod.c
	$(CC) $(CFLAGS) $(INC) $?
gethostname.o: $(srcdir)/clib/gethostname.c
	$(CC) $(CFLAGS) $(INC) $?
getopt.o: $(srcdir)/clib/getopt.c
	$(CC) $(CFLAGS) $(INC) $?
memchr.o: $(srcdir)/clib/memchr.c
	$(CC) $(CFLAGS) $?
memcpy.o: $(srcdir)/clib/memmove.c
	$(cp) $? memcpy.c
	$(CC) $(CFLAGS) -DMEMCOPY memcpy.c
memmove.o: $(srcdir)/clib/memmove.c
	$(CC) $(CFLAGS) -DMEMMOVE $?
memset.o: $(srcdir)/clib/memset.c
	$(CC) $(CFLAGS) $?
mkstemp.o: $(srcdir)/clib/mkstemp.c
	$(CC) $(CFLAGS) $?
mmap.o: $(srcdir)/clib/mmap.c
	$(CC) $(CFLAGS) $?
realloc.o: $(srcdir)/clib/realloc.c
	$(CC) $(CFLAGS) $?
snprintf.o: $(srcdir)/clib/snprintf.c
	$(CC) $(CFLAGS) $?
strdup.o: $(srcdir)/clib/strdup.c
	$(CC) $(CFLAGS) $?
strerror.o: $(srcdir)/clib/strerror.c
	$(CC) $(CFLAGS) $?
strpbrk.o: $(srcdir)/clib/strpbrk.c
	$(CC) $(CFLAGS) $?
strsep.o: $(srcdir)/clib/strsep.c
	$(CC) $(CFLAGS) $?
strtol.o: $(srcdir)/clib/strtol.c
	$(CC) $(CFLAGS) $?
strtoul.o: $(srcdir)/clib/strtoul.c
	$(CC) $(CFLAGS) $?
vsnprintf.o: $(srcdir)/clib/vsnprintf.c
	$(CC) $(CFLAGS) $?
