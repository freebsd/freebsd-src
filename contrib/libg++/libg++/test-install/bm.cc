#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

extern int f();

int main()
{
	// We (mis-)use errno supposedly to check that we got a good errno.h
	// and libc.  I don't quite buy it, but what the hell ... --Per
	errno = f();
	fprintf(stderr, "Return-code: %d (should be 1)\n", errno);
	exit(0);
}
