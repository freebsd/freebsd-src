/*
 * Copyright (c) 1997-1999 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      %W% (Berkeley) %G%
 *
 * $Id: info_file.c,v 1.2 1999/01/10 21:53:45 ezk Exp $
 *
 */

/*
 * Get info from file
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

#define	MAX_LINE_LEN	1500

/* forward declarations */
int file_init(mnt_map *m, char *map, time_t *tp);
int file_reload(mnt_map *m, char *map, void (*fn) (mnt_map *, char *, char *));
int file_search(mnt_map *m, char *map, char *key, char **pval, time_t *tp);
int file_mtime(mnt_map *m, char *map, time_t *tp);


static int
read_line(char *buf, int size, FILE * fp)
{
  int done = 0;

  do {
    while (fgets(buf, size, fp)) {
      int len = strlen(buf);
      done += len;
      if (len > 1 && buf[len - 2] == '\\' &&
	  buf[len - 1] == '\n') {
	int ch;
	buf += len - 2;
	size -= len - 2;
	*buf = '\n';
	buf[1] = '\0';
	/*
	 * Skip leading white space on next line
	 */
	while ((ch = getc(fp)) != EOF &&
	       isascii(ch) && isspace(ch)) ;
	(void) ungetc(ch, fp);
      } else {
	return done;
      }
    }
  } while (size > 0 && !feof(fp));

  return done;
}


/*
 * Try to locate a key in a file
 */
static int
search_or_reload_file(FILE * fp, char *map, char *key, char **val, mnt_map *m, void (*fn) (mnt_map *m, char *, char *))
{
  char key_val[MAX_LINE_LEN];
  int chuck = 0;
  int line_no = 0;

  while (read_line(key_val, sizeof(key_val), fp)) {
    char *kp;
    char *cp;
    char *hash;
    int len = strlen(key_val);
    line_no++;

    /*
     * Make sure we got the whole line
     */
    if (key_val[len - 1] != '\n') {
      plog(XLOG_WARNING, "line %d in \"%s\" is too long", line_no, map);
      chuck = 1;
    } else {
      key_val[len - 1] = '\0';
    }

    /*
     * Strip comments
     */
    hash = strchr(key_val, '#');
    if (hash)
      *hash = '\0';

    /*
     * Find start of key
     */
    for (kp = key_val; *kp && isascii(*kp) && isspace((int)*kp); kp++) ;

    /*
     * Ignore blank lines
     */
    if (!*kp)
      goto again;

    /*
     * Find end of key
     */
    for (cp = kp; *cp && (!isascii(*cp) || !isspace((int)*cp)); cp++) ;

    /*
     * Check whether key matches
     */
    if (*cp)
      *cp++ = '\0';

    if (fn || (*key == *kp && STREQ(key, kp))) {
      while (*cp && isascii(*cp) && isspace((int)*cp))
	cp++;
      if (*cp) {
	/*
	 * Return a copy of the data
	 */
	char *dc = strdup(cp);
	if (fn) {
	  (*fn) (m, strdup(kp), dc);
	} else {
	  *val = dc;
#ifdef DEBUG
	  dlog("%s returns %s", key, dc);
#endif /* DEBUG */
	}
	if (!fn)
	  return 0;
      } else {
	plog(XLOG_USER, "%s: line %d has no value field", map, line_no);
      }
    }

  again:
    /*
     * If the last read didn't get a whole line then
     * throw away the remainder before continuing...
     */
    if (chuck) {
      while (fgets(key_val, sizeof(key_val), fp) &&
	     !strchr(key_val, '\n')) ;
      chuck = 0;
    }
  }

  return fn ? 0 : ENOENT;
}


static FILE *
file_open(char *map, time_t *tp)
{
  FILE *mapf = fopen(map, "r");

  if (mapf && tp) {
    struct stat stb;
    if (fstat(fileno(mapf), &stb) < 0)
      *tp = clocktime();
    else
      *tp = stb.st_mtime;
  }
  return mapf;
}


int
file_init(mnt_map *m, char *map, time_t *tp)
{
  FILE *mapf = file_open(map, tp);

  if (mapf) {
    fclose(mapf);
    return 0;
  }
  return errno;
}


int
file_reload(mnt_map *m, char *map, void (*fn) (mnt_map *, char *, char *))
{
  FILE *mapf = file_open(map, (time_t *) 0);

  if (mapf) {
    int error = search_or_reload_file(mapf, map, 0, 0, m, fn);
    (void) fclose(mapf);
    return error;
  }
  return errno;
}


int
file_search(mnt_map *m, char *map, char *key, char **pval, time_t *tp)
{
  time_t t;
  FILE *mapf = file_open(map, &t);

  if (mapf) {
    int error;
    if (*tp < t) {
      *tp = t;
      error = -1;
    } else {
      error = search_or_reload_file(mapf, map, key, pval, 0, 0);
    }
    (void) fclose(mapf);
    return error;
  }
  return errno;
}


int
file_mtime(mnt_map *m, char *map, time_t *tp)
{
  FILE *mapf = file_open(map, tp);

  if (mapf) {
    (void) fclose(mapf);
    return 0;
  }
  return errno;
}
