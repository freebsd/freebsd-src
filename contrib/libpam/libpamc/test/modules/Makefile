CFLAGS = -g -fPIC -I"../../include"

pam_secret.so: pam_secret.o
	ld -x --shared -o pam_secret.so pam_secret.o -lc

.o.c:

clean:
	rm -f *.so *.o
