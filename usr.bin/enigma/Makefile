all:	enigma makekey

install:	enigma makekey
	cp enigma makekey /usr/brlcad/bin/.
	cp makekey.8 /usr/brlcad/man/man1/.
	ln -s enigma /usr/brlcad/bin/crypt

clean:
	rm -f enigma makekey *.o
