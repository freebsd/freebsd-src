/* cmdtab.c */

/*  $RCSfile: cmdtab.c,v $
 *  $Revision: 14020.11 $
 *  $Date: 93/07/09 11:04:56 $
 */

#include "sys.h"
#include "util.h"
#include "cmds.h"
#include "main.h"
#include "ftp.h"
#include "ftprc.h"
#include "glob.h"
#include "open.h"
#include "set.h"
#include "copyright.h"

#define REMOTEFILE " remote-file-name"
#define REMOTEFILES " remote-file-names and/or UNIX-style-wildcards"
#define LOCALFILE " local-file-name"
#define LOCALFILES " local-file-names and/or UNIX-style-wildcards"
#define LDIRNAME " local-directory-name"
#define RMTDIRNAME " remote-directory-name"
#define EMPTYSTR ""
#define TOGGLE " [on | off] (no argument toggles the switch)"

#define BINARYHELP "transfer files as binary files, without CR/LF translation"
#define BINARYUSAGE EMPTYSTR

#define CHDIRHELP "changes the current remote working directory"
#define CHDIRUSAGE RMTDIRNAME

#define CLOSEHELP "closes FTP connection to current remote host"
#define CLOSEUSAGE EMPTYSTR

#define DELETEHELP "deletes the specified file on the remote host"
#define DELETEUSAGE REMOTEFILE

#define DIRUSAGE " \
[flags] [remote-items] [>outfile or \"|pipecmd [cmd-args]\"]\n\
    Note that there must be no whitespace between > and outfile, or | and\n\
    pipecmd, and if the pipe-command needs arguments, you must enclose the\n\
    whole thing with double quotes.\n\
Examples:\n\
    dir -s\n\
    dir remoteFile\n\
    dir /pub/mac \"|head -20\"\n\
    dir -rtR file1 file2 dir1 >contents.txt"

#define GETUSAGE " remote-file-name [local-file-name or |pipecommand]\n\
Examples:\n\
    get myfile.txt\n\
    get MYFILE.ZIP myfile.zip\n\
	get myfile.txt |head\n\
	get myfile.txt \"|head -20\"\n\
    get ./help/newuser.txt    (./newuser.txt will be local-file-name)\n\
    get ./help/newuser.txt ./docs/newbie.help\n\
    get my*.txt  (pseudo-filename-completion if match is unique, i.e. myfile.txt)"

#define	HELPHELP "shows commands, and optionally tell you how to use a specific one"
#define	HELPUSAGE " [command-name | showall (shows hidden commands) | helpall"

#define LSHELP "prints remote directory contents (short-mode)"
#define LSUSAGE " \
[flags] [remote-items] [>outfile or \"|pipecmd [cmd-args]\"]\n\
    Note that there must be no whitespace between > and outfile, or | and\n\
    pipecmd, and if the pipe-command needs arguments, you must enclose the\n\
    whole thing with double quotes.\n\
Examples:\n\
    ls -s\n\
    ls remoteFile\n\
    ls /pub/mac \"|head -20\"\n\
    ls -lrtR file1 file2 dir1 >contents.txt"

#define OPENHELP "connects to a new remote host, and optionally fetches a file\n\
    or sets the current remote working directory"
#define OPENUSAGE " \
[-a | -u] [-i] [-p N] [-r [-d N] [-g N]] hostname[:pathname]\n\
    -a     : Open anonymously (this is the default).\n\
    -u     : Open, specify user/password.\n\
    -i     : Ignore machine entry in your .netrc.\n\
    -p N   : Use port #N for connection.\n\
    -r     : \"Redial\" until connected.\n\
    -d N   : Redial, pausing N seconds between tries.\n\
    -g N   : Redial, giving up after N tries.\n\
    :path  : Open site, then retrieve file \"path.\"  WWW-style paths are\n\
             also acceptable, i.e. 'ftp://cse.unl.edu/mgleason/README.'"

#define PAGEHELP "view a file on the remote host with your $PAGER"
#define PAGEUSAGE REMOTEFILE

#define PASSIVEHELP "enter passive transfer mode"

#define PDIRUSAGE " [flags] [remote-files]"

#define PUTHELP "sends a local file to the current remote host"
#define PUTUSAGE " local-file-name [remote-file-name]"

#define QUITHELP "quits the program"
#define QUITUSAGE EMPTYSTR

#define	RESTRICTHELP "toggle restriction of data port range"

#define RHELPHELP "asks the remote-server for help"
#define RHELPUSAGE " [help-topic (i.e. FTP command)]"

#define UNIMPLHELP "this command is not supported"
#define UNIMPLUSAGE (NULL)

struct cmd cmdtab[] = {
	/* name ; must-be-connected ; hidden ; help-string ; usage-string */
	{ "!", 			   0,  0,  shell,
		"spawns a shell for you to run other commands",
		" [single-command-and-arguments]" },
	{ "$", 			   0,  0,  domacro,
		"runs a macro previously defined in your NETRC, or with the macdef cmd",
		"macro-number" },
	{ "account",       0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "append",        0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "ascii", 		   1,  1,  setascii,
		"transfer files as text files, with proper CR/LF translation",
		"" },
	{ "bell",          0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "binary",		   1,  1,  setbinary, BINARYHELP, BINARYUSAGE },
	{ "bye",   		   0,  1,  quit, QUITHELP, QUITUSAGE },
	{ "case",          0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "cd",			   1,  0,  cd, CHDIRHELP, CHDIRUSAGE },
	{ "cdup",  		   1,  0,  cdup,
		"changes the current remote working directory to it's parent",
		"" },
	{ "chdir", 		   1,  1,  cd, CHDIRHELP, CHDIRUSAGE },
	{ "close", 		   1,  1,  disconnect, CLOSEHELP, CLOSEUSAGE },
	{ "connect",   	   0,  1,  cmdOpen, OPENHELP, OPENUSAGE },
	{ "cr",            0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "create",		   1,  0,  create,
		"create an empty file on the remote host",
		REMOTEFILE },
	{ "delete",		   1,  0,  do_delete, DELETEHELP, DELETEUSAGE },
	{ "debug", 		   0,  1,  setdebug,
		"to print debugging messages during execution of the program",
		TOGGLE },
	{ "dir",   		   1,  0,  ls,
		"prints remote directory contents (long-mode)",
		DIRUSAGE },
	{ "erase", 		   1,  1,  do_delete, DELETEHELP, DELETEUSAGE },
	{ "exit",  		   0,  1,  quit, QUITHELP, QUITUSAGE },
	{ "form",          0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "get",   		   1,  0,  get,
		"fetches a file from the current remote host", GETUSAGE },
	{ "glob",          0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "hash",  		   0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "help",  		   0,  0,  help, HELPHELP, HELPUSAGE },
	{ "idle",  		   0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "image", 		   1,  1,  setbinary, BINARYHELP, BINARYUSAGE },
	{ "lcd",   		   0,  0,  lcd,
		"changes the current local directory", LDIRNAME },
	{ "lookup",		   0,  0,  lookup,
		"uses the name-server to tell you a host's IP number given it's\n\
    name, or it's name given it's IP number",
    	" hostname | host-IP-number" },
	{ "ls",			   1,  0,  ls, LSHELP, LSUSAGE },
	{ "macdef",		   0,  0,  macdef,
		"defines a macro which is expanded when you use the $ command",
		" new-macro-name" },
	{ "mdelete",   	   1,  0,  mdelete,
		"deletes multiple files on the remote host", REMOTEFILES  },
	{ "mdir",  		   1,  1,  ls, LSHELP, LSUSAGE },
#if LIBMALLOC != LIBC_MALLOC
	{ "memchk",        0,  0,  MallocStatusCmd,
		"show debugging information about memory usage.", EMPTYSTR },
#endif
	{ "mget",  		   1,  0,  mget,
		"fetches multiple files from the remote host", REMOTEFILES },
	{ "mkdir", 		   1,  0,  makedir,
		"creates a new sub-directory on the current remote host",
		RMTDIRNAME },
	{ "mls",   		   1,  0,  ls, LSHELP, LSUSAGE },
	{ "mode",          0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "modtime",   	   1,  0,  modtime,
		"shows the last modification date for a remote file",
		REMOTEFILE },
	{ "more",  		   1,  1,  get, PAGEHELP, PAGEUSAGE },
	{ "mput",  		   1,  0,  mput,
		"sends multiple local files to the current remote host",
		LOCALFILES },
	{ "newer",         0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "nlist", 		   1,  1,  ls, LSHELP, LSUSAGE },
	{ "nmap",          0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "ntrans",        0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "open",  		   0,  0,  cmdOpen, OPENHELP, OPENUSAGE },
	{ "p",  		   1,  1,  get, PAGEHELP, PAGEUSAGE },
	{ "passive",	   0,  0,  setpassive, PASSIVEHELP, EMPTYSTR },
	{ "page",  		   1,  0,  get, PAGEHELP, PAGEUSAGE },
	{ "pdir",  		   1,  0,  ls,
		"view a remote directory listing (long mode) with your $PAGER",
		PDIRUSAGE },
	{ "pls",   		   1,  0,  ls,
		"view a remote directory listing (short mode) with your $PAGER",
		PDIRUSAGE },
	{ "predir",		   1,  0,  ShowLineBuffer,
		"view the last remote directory listing with your $PAGER",
		EMPTYSTR },
	{ "prompt",		   0,  1,  setprompt,
		"toggle interactive prompting on multiple commands",
		TOGGLE },
	{ "proxy",         0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "put",   		   1,  0,  put, PUTHELP, PUTUSAGE },
	{ "pwd",   		   1,  0,  pwd,
		"prints the name of the current remote directory",
		EMPTYSTR },
	{ "quit",  		   0,  0,  quit, QUITHELP, QUITUSAGE },
	{ "quote", 		   1,  0,  quote,
		"allows advanced users to directly enter FTP commands verbatim",
		" FTP-commands" },
	{ "redir", 		   1,  0,  ShowLineBuffer,
		"re-prints the last directory listing",
		EMPTYSTR },
	{ "reget",         0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "remotehelp",	   1,  0,  rmthelp, RHELPHELP, RHELPUSAGE },
	{ "reset",         0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "restart",       0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "restrict",	   0,  0,  setrestrict, RESTRICTHELP, EMPTYSTR },
	{ "rm",			   1,  1,  do_delete, DELETEHELP, DELETEUSAGE },
	{ "rstatus",   	   1,  0,  rmtstatus,
		"asks the remote-server for it's status",
		EMPTYSTR },
	{ "rhelp", 		   1,  1,  rmthelp, RHELPHELP, RHELPUSAGE },
	{ "rename",		   1,  0,  renamefile,
		"changes the name of a file on the current remote host",
		" old-name new-name" },
	{ "rmdir", 		   1,  0,  removedir,
		"deletes a directory on the current remote host",
		RMTDIRNAME },
	{ "runique",       0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "send",  		   1,  1,  put, PUTHELP, PUTUSAGE },
	{ "sendport",      0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "show",  		   0,  0,  do_show,
		"prints the value of some or all program variables",
		" all | variable-names" },
	{ "set",   		   0,  0,  set,
		"changes the value of a program variable; for numeric/boolean\n\
    variables sets them to 1/true",
		" variable-name [= new-value]" },
	{ "site", 		   1,  0,  quote,
		"allows advanced users to send site-specific commands to the host",
		" site-specific-commands\n\
Example (to try on wuarchive.wustl.edu):\n\
	site locate emacs" },
	{ "size",  		   1,  0,  sizecmd,
		"shows the size of a remote file",
		REMOTEFILE },
	{ "struct",        0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "sunique",       0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "system",		   1,  0,  syst,
		"tells you what type of machine the current remote host is",
		EMPTYSTR },
	{ "tenex",         0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "umask",         0,  1,  unimpl, UNIMPLHELP, UNIMPLUSAGE },
	{ "unset", 		   0,  0,  set,
		"resets the value of a program variable to it's default state, or for\n\
    numeric/boolean variables, sets them to 0/false",
		" variable-name" },
	{ "user",  		   1,  0,  do_user,
		"lets you login as a new user (with appropriate password)",
		" new-user-name [new-password]" },
	{ "type",  		   1,  0,  settype,
		"changes the current file transfer method",
		" ascii | binary | ebcdic | tenex" },
	{ "verbose",   	   0,  0,  setverbose,
		"controls how many message the program prints in response to commands",
		" -1 (quiet) | 0 (errs) | 1 (terse) | 2 (verbose)" },
	{ "version",   	   0,  0,  show_version,
		"prints information about the program",
		EMPTYSTR },
	{ "?", 			   0,  1,  help, HELPHELP, HELPUSAGE },
	{ NULL,			   0,  0,  NULL, NULL, NULL }
};

/* eof cmdtab.c */
