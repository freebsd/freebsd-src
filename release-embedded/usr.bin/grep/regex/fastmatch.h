/* $FreeBSD$ */

#ifndef FASTMATCH_H
#define FASTMATCH_H 1

#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <wchar.h>

typedef struct {
  size_t	 wlen;
  size_t	 len;
  wchar_t	*wpattern;
  bool		*wescmap;
  unsigned int	 qsBc[UCHAR_MAX + 1];
  unsigned int	*bmGs;
  char		*pattern;
  bool		*escmap;
  unsigned int	 defBc;
  void		*qsBc_table;
  unsigned int	*sbmGs;
  const char	*re_endp;

  /* flags */
  bool		 hasdot;
  bool		 bol;
  bool		 eol;
  bool		 word;
  bool		 icase;
  bool		 newline;
  bool		 nosub;
  bool		 matchall;
  bool		 reversed;
} fastmatch_t;

extern int
tre_fixcomp(fastmatch_t *preg, const char *regex, int cflags);

extern int
tre_fastcomp(fastmatch_t *preg, const char *regex, int cflags);

extern int
tre_fastexec(const fastmatch_t *preg, const char *string, size_t nmatch,
  regmatch_t pmatch[], int eflags);

extern void
tre_fastfree(fastmatch_t *preg);

extern int
tre_fixwcomp(fastmatch_t *preg, const wchar_t *regex, int cflags);

extern int
tre_fastwcomp(fastmatch_t *preg, const wchar_t *regex, int cflags);

extern int
tre_fastwexec(const fastmatch_t *preg, const wchar_t *string,
         size_t nmatch, regmatch_t pmatch[], int eflags);

/* Versions with a maximum length argument and therefore the capability to
   handle null characters in the middle of the strings. */
extern int
tre_fixncomp(fastmatch_t *preg, const char *regex, size_t len, int cflags);

extern int
tre_fastncomp(fastmatch_t *preg, const char *regex, size_t len, int cflags);

extern int
tre_fastnexec(const fastmatch_t *preg, const char *string, size_t len,
  size_t nmatch, regmatch_t pmatch[], int eflags);

extern int
tre_fixwncomp(fastmatch_t *preg, const wchar_t *regex, size_t len, int cflags);

extern int
tre_fastwncomp(fastmatch_t *preg, const wchar_t *regex, size_t len, int cflags);

extern int
tre_fastwnexec(const fastmatch_t *preg, const wchar_t *string, size_t len,
  size_t nmatch, regmatch_t pmatch[], int eflags);

#define fixncomp	tre_fixncomp
#define fastncomp	tre_fastncomp
#define fixcomp		tre_fixcomp
#define fastcomp	tre_fastcomp
#define fixwncomp	tre_fixwncomp
#define fastwncomp	tre_fastwncomp
#define fixwcomp	tre_fixwcomp
#define fastwcomp	tre_fastwcomp
#define fastfree	tre_fastfree
#define fastnexec	tre_fastnexec
#define fastexec	tre_fastexec
#define fastwnexec	tre_fastwnexec
#define fastwexec	tre_fastwexec
#define fixcomp		tre_fixcomp
#define fastcomp	tre_fastcomp
#define fastexec	tre_fastexec
#define fastfree	tre_fastfree
#define fixwcomp	tre_fixwcomp
#define fastwcomp	tre_fastwcomp
#define fastwexec	tre_fastwexec
#define fixncomp	tre_fixncomp
#define fastncomp	tre_fastncomp
#define fastnexec	tre_fastnexec
#define fixwncomp	tre_fixwncomp
#define fastwncomp	tre_fastwncomp
#define fastwnexec	tre_fastwnexec
#endif		/* FASTMATCH_H */
