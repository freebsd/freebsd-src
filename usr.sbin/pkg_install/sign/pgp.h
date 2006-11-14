/* $FreeBSD$ */
/* $OpenBSD: pgp.h,v 1.2 1999/10/04 21:46:28 espie Exp $ */
/* Estimate size of pgp signature */
#define MAXPGPSIGNSIZE	1024

#ifndef PGP
#define PGP "/usr/local/bin/pgp"
#endif

struct mygzip_header;
struct signature;

extern void *new_pgp_checker __P((struct mygzip_header *h, \
	struct signature *sign, const char *userid, char *envp[], \
	const char *filename));

extern void pgp_add __P((void *arg, const char *buffer, \
	size_t length));

extern int pgp_sign_ok __P((void *arg));

extern void handle_pgp_passphrase __P((void));

extern int retrieve_pgp_signature __P((const char *filename, \
struct signature **sign, const char *userid, char *envp[]));
