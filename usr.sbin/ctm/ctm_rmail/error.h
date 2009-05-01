/* $FreeBSD: src/usr.sbin/ctm/ctm_rmail/error.h,v 1.2.34.1 2009/04/15 03:14:26 kensmith Exp $ */

extern	void	err_set_log(char *log_file);
extern	void	err_prog_name(char *name);
extern	void	err(const char *fmt, ...) __printflike(1, 2);
