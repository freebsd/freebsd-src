/* This file is in the public domain */

#define	abort()								\
	panic("libsodium error at %s:%d", __FILE__, __LINE__)
