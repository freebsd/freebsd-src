#
#	Makefile for the tp documents:
#   design: TP design/source guide
#   appendix_a: index of tp kernel routines & macros by macro/routine name
#   appendix_b: index of tp kernel routines & macros by file name
#
PRINTER = 3a
TAGS	= ../../sys/tags
SRCS	= ../../sys/netargo/tp_*.c ../../sys/netargo/tp_*.h ../../sys/netargo/tp*.trans
TROFF = /usr/local/lib/troff

#
#	Print via speedy for cycles sake...
#	(assumes postscript printer...)
#
program: 
	@echo printer is $(PRINTER)
	(cd figs; make)
	format -P$(PRINTER) -t program.nr | rsh speedy psdit \| lpr -P$(PRINTER)

parts:
	@echo printer is $(PRINTER)
	(cd figs; make)
	format -P$(PRINTER) -t parts.nr   | rsh speedy psdit \| lpr -P$(PRINTER)
#	format -P$(PRINTER) -t parts.nr > /dev/null
#	soelim parts.nr | grn -P$(PRINTER) |\
#		$(TROFF) -Tpsc | rsh speedy psdit \> /tmp/test
#	soelim parts.nr | tbl > /tmp/parts.nr

clean:
	/bin/rm -f core junk* a.out *.o spell_errs made
	touch spell_errs

spell:
	(cd figs; make)
	(cd ../icon; make)
	/usr/ucb/soelim program.nr | /usr/bin/spell -d hlista > spell_errs

newdict:
	cat spell_errs | spellin /usr/dict/hlista > hlista

all: program appendix_a appendix_b appendix_c


appendix_c:
	format -P$(PRINTER) appendix_c.nr
	tbl ../man/man4/table1.src > ../man/man4/table1.nr
	tbl ../man/man4/table2.src > ../man/man4/table2.nr
	tbl ../man/man4/table3.src > ../man/man4/table3.nr
	soelim ../man/man4/tp.4p.src > ../man/man4/tp.4p
	ditroff -man -P$(PRINTER) ../man/man1/xebec.1
	ditroff -man -P$(PRINTER) ../man/man2/sendv.2
	ditroff -man -P$(PRINTER) ../man/man2/recvv.2
	ditroff -man -P$(PRINTER) ../man/man3/libtp.3
	ditroff -man -P$(PRINTER) ../man/man4/tp.4p
	ditroff -man -P$(PRINTER) ../man/man8/tppt.8
	ditroff -man -P$(PRINTER) ../man/man8/tpdebug.8
	ditroff -man -P$(PRINTER) ../man/man8/tpstat.8

appendix_a:
	ctags -x $(SRCS) | awk '{printf("%s	%s	%s\n",  $$1, $$3, $$2)}'\
		| sed -e 's-../../sys/netargo/--' > index_by_func.nr
	format -P$(PRINTER) appendix_a.nr

appendix_b:
	ctags -x $(SRCS) | awk '{printf("%s	%s	%s\n",  $$3, $$1, $$2)}'\
		| sed -e 's-../../sys/netargo/--' \
		| sort \
		| fmtxref -w 80 \
		| sed	-e 's/					/		/' \
				-e 's/			/	/' \
		> index_by_file.nr
	format -P$(PRINTER) appendix_b.nr
