/*
 * $Id: support.h,v 1.3 2000/12/20 05:15:05 vorlon Exp $
 */

#ifndef _PAM_UNIX_SUPPORT_H
#define _PAM_UNIX_SUPPORT_H


/*
 * here is the string to inform the user that the new passwords they
 * typed were not the same.
 */

#define MISTYPED_PASS "Sorry, passwords do not match"

/* type definition for the control options */

typedef struct {
	const char *token;
	unsigned int mask;	/* shall assume 32 bits of flags */
	unsigned int flag;
} UNIX_Ctrls;

/*
 * macro to determine if a given flag is on
 */

#define on(x,ctrl)  (unix_args[x].flag & ctrl)

/*
 * macro to determine that a given flag is NOT on
 */

#define off(x,ctrl) (!on(x,ctrl))

/*
 * macro to turn on/off a ctrl flag manually
 */

#define set(x,ctrl)   (ctrl = ((ctrl)&unix_args[x].mask)|unix_args[x].flag)
#define unset(x,ctrl) (ctrl &= ~(unix_args[x].flag))

/* the generic mask */

#define _ALL_ON_  (~0U)

/* end of macro definitions definitions for the control flags */

/* ****************************************************************** *
 * ctrl flags proper..
 */

/*
 * here are the various options recognized by the unix module. They
 * are enumerated here and then defined below. Internal arguments are
 * given NULL tokens.
 */

#define UNIX__OLD_PASSWD          0	/* internal */
#define UNIX__VERIFY_PASSWD       1	/* internal */
#define UNIX__IAMROOT             2	/* internal */

#define UNIX_AUDIT                3	/* print more things than debug..
					   some information may be sensitive */
#define UNIX_USE_FIRST_PASS       4
#define UNIX_TRY_FIRST_PASS       5
#define UNIX_NOT_SET_PASS         6	/* don't set the AUTHTOK items */

#define UNIX__PRELIM              7	/* internal */
#define UNIX__UPDATE              8	/* internal */
#define UNIX__NONULL              9	/* internal */
#define UNIX__QUIET              10	/* internal */
#define UNIX_USE_AUTHTOK         11	/* insist on reading PAM_AUTHTOK */
#define UNIX_SHADOW              12	/* signal shadow on */
#define UNIX_MD5_PASS            13	/* force the use of MD5 passwords */
#define UNIX__NULLOK             14	/* Null token ok */
#define UNIX_DEBUG               15	/* send more info to syslog(3) */
#define UNIX_NODELAY             16	/* admin does not want a fail-delay */
#define UNIX_NIS                 17	/* wish to use NIS for pwd */
#define UNIX_BIGCRYPT            18	/* use DEC-C2 crypt()^x function */
#define UNIX_LIKE_AUTH           19	/* need to auth for setcred to work */
#define UNIX_REMEMBER_PASSWD     20	/* Remember N previous passwords */
/* -------------- */
#define UNIX_CTRLS_              21	/* number of ctrl arguments defined */


static const UNIX_Ctrls unix_args[UNIX_CTRLS_] =
{
/* symbol                  token name          ctrl mask             ctrl     *
 * ----------------------- ------------------- --------------------- -------- */

/* UNIX__OLD_PASSWD */     {NULL,              _ALL_ON_,                  01},
/* UNIX__VERIFY_PASSWD */  {NULL,              _ALL_ON_,                  02},
/* UNIX__IAMROOT */        {NULL,              _ALL_ON_,                  04},
/* UNIX_AUDIT */           {"audit",           _ALL_ON_,                 010},
/* UNIX_USE_FIRST_PASS */  {"use_first_pass",  _ALL_ON_^(060),           020},
/* UNIX_TRY_FIRST_PASS */  {"try_first_pass",  _ALL_ON_^(060),           040},
/* UNIX_NOT_SET_PASS */    {"not_set_pass",    _ALL_ON_,                0100},
/* UNIX__PRELIM */         {NULL,              _ALL_ON_^(0600),         0200},
/* UNIX__UPDATE */         {NULL,              _ALL_ON_^(0600),         0400},
/* UNIX__NONULL */         {NULL,              _ALL_ON_,               01000},
/* UNIX__QUIET */          {NULL,              _ALL_ON_,               02000},
/* UNIX_USE_AUTHTOK */     {"use_authtok",     _ALL_ON_,               04000},
/* UNIX_SHADOW */          {"shadow",          _ALL_ON_,              010000},
/* UNIX_MD5_PASS */        {"md5",             _ALL_ON_^(0400000),    020000},
/* UNIX__NULLOK */         {"nullok",          _ALL_ON_^(01000),           0},
/* UNIX_DEBUG */           {"debug",           _ALL_ON_,              040000},
/* UNIX_NODELAY */         {"nodelay",         _ALL_ON_,             0100000},
/* UNIX_NIS */             {"nis",             _ALL_ON_^(010000),    0200000},
/* UNIX_BIGCRYPT */        {"bigcrypt",        _ALL_ON_^(020000),    0400000},
/* UNIX_LIKE_AUTH */       {"likeauth",        _ALL_ON_,            01000000},
/* UNIX_REMEMBER_PASSWD */ {"remember=",       _ALL_ON_,            02000000},
};

#define UNIX_DEFAULTS  (unix_args[UNIX__NONULL].flag)


/* use this to free strings. ESPECIALLY password strings */

#define _pam_delete(xx)		\
{				\
	_pam_overwrite(xx);	\
	_pam_drop(xx);		\
}

extern char *PAM_getlogin(void);
extern void _log_err(int err, pam_handle_t *pamh, const char *format,...);
extern int _make_remark(pam_handle_t * pamh, unsigned int ctrl
		       ,int type, const char *text);
extern int _set_ctrl(pam_handle_t * pamh, int flags, int *remember, int argc,
		     const char **argv);
extern int _unix_blankpasswd(unsigned int ctrl, const char *name);
extern int _unix_verify_password(pam_handle_t * pamh, const char *name
			  ,const char *p, unsigned int ctrl);
extern int _unix_read_password(pam_handle_t * pamh
			,unsigned int ctrl
			,const char *comment
			,const char *prompt1
			,const char *prompt2
			,const char *data_name
			,const char **pass);

#endif /* _PAM_UNIX_SUPPORT_H */

