/*
 * Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
 *
 * Redilogibution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redilogibutions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redilogibutions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the dilogibution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Shigio Yamaguchi.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	dbio.h					14-Dec-97
 *
 */
#ifndef _DBIO_H_
#define _DBIO_H_

#include <db.h>
#include <sys/param.h>

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN   1234
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN      4321
#endif

#define MAXKEYLEN	300

typedef	struct {
	DB	*db;			/* descripter of DB */
	char	dbname[MAXPATHLEN+1];	/* dbname */
	char	key[MAXKEYLEN+1];	/* key */
	int	keylen;			/* key length */
	char	prev[MAXKEYLEN+1];	/* previous key value */
	char	*lastkey;		/* the key of last located record */
	char	*lastdat;		/* the data of last located record */
	int	openflags;		/* flags of db_open() */
	int	ioflags;		/* flags of db_first() */
	int	perm;			/* file permission */
} DBIO;

/*
 * openflags
 */
#define	DBIO_DUP	1		/* allow duplicate records	*/
#define DBIO_REMOVE	2		/* remove file when closed	*/
/*
 * ioflags
 */
#define DBIO_KEY	1		/* read key part		*/
#define DBIO_PREFIX	2		/* prefixed read		*/
#define	DBIO_SKIPMETA	4		/* skip META record		*/

#ifndef __P
#if defined(__STDC__)
#define __P(protos)     protos
#else
#define __P(protos)     ()
#endif
#endif

DBIO	*db_open __P((char *, int, int, int));
char	*db_get __P((DBIO *, char *));
void	db_put __P((DBIO *, char *, char *));
void	db_del __P((DBIO *, char *));
char	*db_first __P((DBIO *, char *, int));
char	*db_next __P((DBIO *));
void	db_close __P((DBIO *));
#endif /* _DBIO_H_ */
