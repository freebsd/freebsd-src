#ifndef _FTP_H_INCLUDE
#define _FTP_H_INCLUDE

#include <sys/types.h>
#include <sys/cdefs.h>
#include <stdio.h>
#include <time.h>

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * Major Changelog:
 *
 * Jordan K. Hubbard
 * 17 Jan 1996
 *
 * Turned inside out. Now returns xfers as new file ids, not as a special
 * `state' of FTP_t
 *
 * $FreeBSD: src/lib/libftpio/ftpio.h,v 1.15.2.1 2000/07/15 07:24:03 kris Exp $
 */

/* Internal housekeeping data structure for FTP sessions */
typedef struct {
    enum { init, isopen, quit } con_state;
    int		fd_ctrl;
    int		addrtype;
    char	*host;
    char	*file;
    int		error;
    int		is_binary;
    int		is_passive;
    int		is_verbose;
} *FTP_t;

/* Structure we use to match FTP error codes with readable strings */
struct ftperr {
  const int	num;
  const char	*string;
};

__BEGIN_DECLS
extern struct	ftperr ftpErrList[];
extern int	const ftpErrListLength;

/* Exported routines - deal only with FILE* type */
extern FILE	*ftpLogin(char *host, char *user, char *passwd,	int port, int verbose, int *retcode);
extern int	ftpChdir(FILE *fp, char *dir);
extern int	ftpErrno(FILE *fp);
extern off_t	ftpGetSize(FILE *fp, char *file);
extern FILE	*ftpGet(FILE *fp, char *file, off_t *seekto);
extern FILE	*ftpPut(FILE *fp, char *file);
extern int	ftpAscii(FILE *fp);
extern int	ftpBinary(FILE *fp);
extern int	ftpPassive(FILE *fp, int status);
extern void	ftpVerbose(FILE *fp, int status);
extern FILE	*ftpGetURL(char	*url, char *user, char *passwd,	int *retcode);
extern FILE	*ftpPutURL(char	*url, char *user, char *passwd,	int *retcode);
extern time_t	ftpGetModtime(FILE *fp, char *s);
extern const	char *ftpErrString(int error);
extern FILE	*ftpLoginAf(char *host, int af, char *user, char *passwd,	int port, int verbose, int *retcode);
extern FILE	*ftpGetURLAf(char *url, int af, char *user, char *passwd, int *retcode);
extern FILE	*ftpPutURLAf(char *url, int af, char *user, char *passwd, int *retcode);
__END_DECLS

#endif	/* _FTP_H_INCLUDE */
