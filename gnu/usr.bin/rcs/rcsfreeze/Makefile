# Do nothing for the following
obj clean cleandir depend rcsfreeze all:
	@echo No need to make $@ for rcsfreeze\; ignored

install:
	install -c -o bin -g bin -m 555 rcsfreeze.sh /usr/bin/rcsfreeze
	install -c -o bin -g bin -m 444 rcsfreeze.1 /usr/share/man/man1
