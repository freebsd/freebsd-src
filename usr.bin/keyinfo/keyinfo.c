/*-
 * Copyright (c) 2000 Warner Losh.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <skey.h>
#include <unistd.h>
#include <err.h>

int
main(int argc, char *argv[])
{
 	struct skey skey;
 	char *login;
 	char *name;
	int serr;
 
 	login = getlogin();
 	if (login == NULL)
 		errx(1, "Cannot find login name");
 	if (getuid() != 0 && argc > 1 && strcmp(login, argv[1]) != 0)
 		errx(1, "Only superuser may get another user's keys");
 	name = argc > 1 ? argv[1] : login;
	serr = skeylookup(&skey, name);
	if (serr == -1)
		err(1, "skeylookup os failure");
 	fclose(skey.keyfile);
	if (serr != 0)
		errx(1, "skeylookup: user %s not found", name);
 	printf("%d %s\n", skey.n - 1, skey.seed);
 	return (0);
}
