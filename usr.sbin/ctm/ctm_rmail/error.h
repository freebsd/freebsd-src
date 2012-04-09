/* $FreeBSD: src/usr.sbin/ctm/ctm_rmail/error.h,v 1.2.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $ */

extern	void	err_set_log(char *log_file);
extern	void	err_prog_name(char *name);
extern	void	err(const char *fmt, ...) __printflike(1, 2);
