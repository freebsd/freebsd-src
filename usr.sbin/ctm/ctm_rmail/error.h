/* $FreeBSD: src/usr.sbin/ctm/ctm_rmail/error.h,v 1.2.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */

extern	void	err_set_log(char *log_file);
extern	void	err_prog_name(char *name);
extern	void	err(const char *fmt, ...) __printflike(1, 2);
