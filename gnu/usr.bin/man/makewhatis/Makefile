.include "../Makefile.inc"

all:	makewhatis

obj depend rcsfreeze tags all:
	@echo -n

cleandir: clean

clean:
	@rm -f makewhatis

install:
	install -c -o bin -g bin -m 444 makewhatis /usr/bin

makewhatis: makewhatis.sh
	sed -e 's/%sections%/ "1", "n", "l", "6", "8", "2", "3", "4", "5", "7", "p", "o", NULL/' makewhatis.sh > makewhatis
