/* Set.h */

#ifndef _set_h_
#define _set_h_

/*  $RCSfile: set.h,v $
 *  $Revision: 14020.11 $
 *  $Date: 93/06/26 06:21:32 $
 */

/* Variable types. */
#define INT		1
#define LONG	2
#define STR		3
#define BOOL	4

typedef void (*setvarproc)(char *, int);
struct var {
	char			*name;
	short			nmlen;
	short			type;
	short			conn_required;
	void			*var;
	setvarproc		proc;
};

#define VARENTRY(n,t,c,v,p)	{ (n), (short)(sizeof(n) - 1), (t), (c), (v), (setvarproc)(p) }
#define NVARS ((int) (sizeof(vars)/sizeof(struct var)))

void set_prompt(char *new, int unset);
void set_log(char *fname, int unset);
void set_ldir(char *ldir, int unset);
#ifdef GATEWAY
void set_gateway(char *, int);
void set_gatelogin(char *, int);
#endif
void set_pager(char *new, int unset);
void set_verbose(char *new, int unset);
void set_type(char *newtype, int unset);
struct var *match_var(char *varname);
void show_var(struct var *v);
void show(char *varname);
int do_show(int argc, char **argv);
int set(int argc, char **argv);

#endif	/* _set_h_ */
