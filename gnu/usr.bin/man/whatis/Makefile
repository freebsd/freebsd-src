.include "../Makefile.inc"

all:	whatis whatis.1

obj depend rcsfreeze all:
	@echo -n

cleandir: clean

clean:
	@rm -f whatis whatis.1

whatis: whatis.sh
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' \
		whatis.sh > whatis

whatis.1: whatis.man
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	whatis.man > whatis.1

install: whatis whatis.1
	install -c -o bin -g bin -m 555 whatis /usr/bin
	install -c -o bin -g bin -m 444 whatis.1 /usr/share/man/man1
