/*
 * Copyright (c) 1993 Christoph M. Robitschko
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christoph M. Robitschko
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * cf_table.c
 * The table of words that the configuration subsystem understands,
 * along with local support functions.
 */

#ifdef CONFIGURE

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ttyent.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>

#include "init.h"
#include "prototypes.h"
#include "cf_defs.h"


/* Prototypes for static functions */
static int	conf_env(char *, int, int, char **, int);
static int	conf_include(char *, int, int, char **, int);
static int	conf_waittimes(char *, int, int, char **, int);


/* static variables */
static struct rlimit	proclimits[RLIM_NLIMITS];


/* extern variables, referenced inb Commands table */
extern int              startup_single;
extern struct ttyent    RCent_auto;
extern struct ttyent    RCent_fast;
extern struct ttyent    Single_ent;
extern int              retrytime;
extern int              timeout_m2s_KILL;
extern int              timeout_m2s_TERM;
extern int              checkstatus;
extern int              checktime;



#ifdef __STDC__
#   define CURLIM(_lim_)	&proclimits[RLIMIT_##_lim_].rlim_cur
#   define MAXLIM(_lim_)	&proclimits[RLIMIT_##_lim_].rlim_max
#else
#   define CURLIM(_lim_)	&proclimits[RLIMIT_/**/_lim_].rlim_cur
#   define MAXLIM(_lim_)	&proclimits[RLIMIT_/**/_lim_].rlim_max
#endif


/*
 * The table of all known words and actions
 */

const struct Command Commands[] = {

#ifdef DEBUG
 {"debug_level",	0+SUB,	T_EX,	0,	NOVAR},
  {"LEVEL",		1,	T_INT,	MAXVAL,	&debug,			{5}},
#endif

 {"startup_state",	0+SUB,	T_EX,	0,	NOVAR},
  {"singleuser_mode",	1,	T_EX,	0,	&startup_single,	{1}},
  {"multiuser_mode",	1,	T_EX,	0,	&startup_single,	{0}},

 {"setenv",		0+SUB,	T_EX,	0,	NOVAR},
  {"VAR",		1+SUB,	T_STR,	0,	NOVAR},
   {"STRING",		2,	T_STR,	CFUNC,	conf_env,		{0}},

 {"include",		0+SUB,	T_EX,	0,	NOVAR},
  {"FILE",		1,	T_STR,	CFUNC,	conf_include,		{0}},
 
 {"singleusershell",	0+SUB,	T_EX,	0,	NOVAR},
  {"COMMAND",		1,	T_STR,	0,	&Single_ent.ty_getty,	{0}},
 {"singleuserterminal",	0+SUB,	T_EX,	0,	NOVAR},
  {"TYPE",		1,	T_STR,	0,	&Single_ent.ty_type,	{0}},
 {"singleuserdevice",	0+SUB,	T_EX,	0,	NOVAR},
  {"DEVICE",		1,	T_STR,	0,	&Single_ent.ty_name,	{0}},

 {"autobootcommand",	0+SUB,	T_EX,	0,	NOVAR},
  {"COMMAND",		1,	T_STR,	0,	&RCent_auto.ty_getty,	{0}},
 {"fastbootcommand",	0+SUB,	T_EX,	0,	NOVAR},
  {"COMMAND",		1,	T_STR,	0,	&RCent_fast.ty_getty,	{0}},

 {"timeout",		0+SUB,	T_EX,	0,	NOVAR},
  {"shutdown",		1+SUB,	T_EX,	0,	NOVAR},
   {"sigterm",		2+SUB,	T_EX,	0,	NOVAR},
    {"TIMEOUT",		3,	T_INT,	MAXVAL,	&timeout_m2s_TERM,	{300}},
   {"sigkill",		2+SUB,	T_EX,	0,	NOVAR},
    {"TIMEOUT",		3,	T_INT,	MAXVAL,	&timeout_m2s_KILL,	{300}},
  {"error-retry",	1+SUB,	T_EX,	0,	NOVAR},
   {"TIME",		2,	T_TIME,	0,	&retrytime,		{0}},

 {"respawn",		0+SUB,	T_EX,	0,	NOVAR},
  {"checkstatus",	1+SUB,	T_EX,	0,	NOVAR},
   {"yes",		2,	T_EX,	0,	&checkstatus,		{1}},
   {"on",		2,	T_EX,	0,	&checkstatus,		{1}},
   {"no",		2,	T_EX,	0,	&checkstatus,		{0}},
   {"off",		2,	T_EX,	0,	&checkstatus,		{0}},
  {"checktime",		1+SUB,	T_EX,	0,	NOVAR},
   {"yes",		2,	T_EX,	0,	&checktime,		{DEF_CHECKTIME}},
   {"on",		2,	T_EX,	0,	&checktime,		{DEF_CHECKTIME}},
   {"no",		2,	T_EX,	0,	&checktime,		{0}},
   {"off",		2,	T_EX,	0,	&checktime,		{0}},
   {"TIMEOUT",		2,	T_TIME,	MAXVAL,	&checktime,		{300}},
  {"waittimes",		1+SUB,	T_EX,	0,	NOVAR},
   {"delete_list",	2,	T_EX,	CFUNC,	conf_waittimes,		{1}},
   {"add_list",		2+SUB,	T_EX,	0,	NOVAR},
    {"TIME",		3,	T_TIME,	CFUNC,	conf_waittimes,		{2}},

 {"limit",		0+SUB,	T_EX,	0,	NOVAR},
  {"cputime",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	CURLIM(CPU),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_TIME,	0,	CURLIM(CPU),		{0}},
  {"filesize",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	CURLIM(FSIZE),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	0,	CURLIM(FSIZE),		{0}},
  {"datasize",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	CURLIM(DATA),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	0,	CURLIM(DATA),		{0}},
  {"stacksize",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	CURLIM(STACK),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	0,	CURLIM(STACK),		{0}},
  {"coredumpsize",	1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	CURLIM(CORE),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	0,	CURLIM(CORE),		{0}},
  {"memoryuse",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	CURLIM(RSS),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	0,	CURLIM(RSS),		{0}},
  {"memorylocked",	1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	CURLIM(MEMLOCK),	{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	0,	CURLIM(MEMLOCK),	{0}},
  {"maxprocesses",	1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	CURLIM(NPROC),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_INT,	0,	CURLIM(NPROC),		{0}},
  {"openfiles",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	CURLIM(OFILE),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_INT,	0,	CURLIM(OFILE),		{0}},
 
 {"hardlimit",		0+SUB,	T_EX,	0,	NOVAR},
  {"cputime",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	MAXLIM(CPU),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_TIME,	NRAISE,	MAXLIM(CPU),		{0}},
  {"filesize",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	MAXLIM(FSIZE),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	NRAISE,	MAXLIM(FSIZE),		{0}},
  {"datasize",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	MAXLIM(DATA),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	NRAISE,	MAXLIM(DATA),		{0}},
  {"stacksize",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	MAXLIM(STACK),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	NRAISE,	MAXLIM(STACK),		{0}},
  {"coredumpsize",	1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	MAXLIM(CORE),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	NRAISE,	MAXLIM(CORE),		{0}},
  {"memoryuse",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	MAXLIM(RSS),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	NRAISE,	MAXLIM(RSS),		{0}},
  {"memorylocked",	1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	MAXLIM(MEMLOCK),	{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_BYTE,	NRAISE,	MAXLIM(MEMLOCK),	{0}},
  {"maxprocesses",	1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	MAXLIM(NPROC),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_INT,	NRAISE,	MAXLIM(NPROC),		{0}},
  {"openfiles",		1+SUB,	T_EX,	0,	NOVAR},
   {"unlimited",	2,	T_EX,	0,	MAXLIM(OFILE),		{RLIM_INFINITY}},
   {"default",		2,	T_EX,	0,	NOVAR},
   {"LIMIT",		2,	T_INT,	NRAISE,	MAXLIM(OFILE),		{0}},
};
const int	NCommands = (sizeof(Commands) / sizeof(struct Command));


/*
 * INIT-specific functions for configuration
 */


/*
 * SETCONF
 * called in each new process to set up configured things
 */
void
setconf(void)
{
int		f;

	Debug (4, "Setting configuration");
	for (f=0; f< RLIM_NLIMITS; f++)
		(void) setrlimit(f, &proclimits[f]);
	Debug(4, "Setting configuration completed");
}



/*
 * GETCONF
 * prepare for configuration
 * called once before configure().
 */
void
getconf(void)
{
int		f;

	for (f=0; f< RLIM_NLIMITS; f++)
		(void) getrlimit(f, &proclimits[f]);
}



/*
 * CHECKCONF
 * check configured things etc.
 * called once after configure().
 */
void
checkconf(void)
{
int		f;

	/* Do not allow soft limits to be higher than the hard limits */
	for (f=0; f<RLIM_NLIMITS; f++)
		if (proclimits[f].rlim_cur > proclimits[f].rlim_max)
			proclimits[f].rlim_cur = proclimits[f].rlim_max;
}


/*
 * Support functions, referenced in Commands table
 */

/* Set an environment variable from the configuration file */
static int
conf_env(configfile, lineno, argc, argv, what)
char		*configfile;
int		lineno;
int		argc;
char		**argv;
int		what;
{
	iputenv(argv[1], argv[2]);
	return(0);
}

/* Include another configuration file */
static int
conf_include(configfile, lineno, argc, argv, what)
char		*configfile;
int		lineno;
int		argc;
char		**argv;
int		what;
{
	configure(argv[1]);
	return(0);
}


/* Configure the times to wait before restarting a getty that has failed */
static int
conf_waittimes(configfile, lineno, argc, argv, what)
char		*configfile;
int		lineno;
int		argc;
char		**argv;
int		what;
{
	Debug(0, "conf_waittimes not yet implemented.");
	return(0);
}

#endif
