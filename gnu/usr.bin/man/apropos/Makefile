.include "../Makefile.inc"

all:	apropos apropos.1

obj depend rcsfreeze tags all:
	@echo -n

cleandir: clean

clean:
	@rm -f apropos apropos.1

apropos: apropos.sh
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' \
		apropos.sh > apropos

apropos.1: apropos.man
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	apropos.man > apropos.1

install: apropos apropos.1
	install -c -o bin -g bin -m 555 apropos /usr/bin
	install -c -o bin -g bin -m 444 apropos.1 /usr/share/man/man1
