CC=gcc
all:
	@echo "Please do one of the following:"
	@echo "make bsd"
	@echo "make bsdi"
	@echo "make freebsd"
	@echo "make freebsd22"
	@echo "make netbsd"
	@echo "make openbsd"
	@echo "make sunos4"
	@echo "make sunos5"

sunos5:
	$(CC) -DSOLARIS2=`uname -r | sh -c 'IFS=. read j n x; echo $$n'` \
		-I.. userauth.c -o userauth -lsocket -lnsl
	$(CC) -DSOLARIS2=`uname -r | sh -c 'IFS=. read j n x; echo $$n'` \
		-I.. proxy.c -o proxy -lsocket -lnsl

freebsd freebsd22 netbsd bsd bsdi sunos4 openbsd:
	$(CC) -I.. userauth.c -o userauth
	$(CC) -I.. proxy.c -o proxy

clean:
	/bin/rm -f userauth proxy
