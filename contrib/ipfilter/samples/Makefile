CC=gcc

sunos5:
	$(CC) -I.. userauth.c -o userauth -lsocket -lnsl
	$(CC) -I.. proxy.c -o proxy -lsocket -lnsl

freebsd freebsd22 netbsd bsd bsdi sunos4:
	$(CC) -I.. userauth.c -o userauth
	$(CC) -I.. proxy.c -o proxy

clean:
	/bin/rm -f userauth proxy
