/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 */

#ifndef lint
static char sccsid[] = "@(#)excmd.c	8.41 (Berkeley) 3/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * This array maps ex command names to command functions.
 *
 * The order in which command names are listed below is important --
 * ambiguous abbreviations are resolved to be the first possible match,
 * e.g. "r" means "read", not "rewind", because "read" is listed before
 * "rewind".
 *
 * The syntax of the ex commands is unbelievably irregular, and a special
 * case from beginning to end.  Each command has an associated "syntax
 * script" which describes the "arguments" that are possible.  The script
 * syntax is as follows:
 *
 *	!		-- ! flag
 *	1		-- flags: [+-]*[pl#][+-]*
 *	2		-- flags: [-.+^]
 *	3		-- flags: [-.+^=]
 *	b		-- buffer
 *	c[01+a]		-- count (0-N, 1-N, signed 1-N, address offset)
 *	f[N#][or]	-- file (a number or N, optional or required)
 *	l		-- line
 *	S		-- string with file name expansion
 *	s		-- string
 *	W		-- word string
 *	w[N#][or]	-- word (a number or N, optional or required)
 */
EXCMDLIST const cmds[] = {
/* C_SCROLL */
	{"\004",	ex_pr,		E_ADDR2|E_NORC,
	    "",
	    "^D",
	    "scroll lines"},
/* C_BANG */
	{"!",		ex_bang,	E_ADDR2_NONE|E_NORC,
	    "S",
	    "[line [,line]] ! command",
	    "filter lines through commands or run commands"},
/* C_HASH */
	{"#",		ex_number,	E_ADDR2|E_F_PRCLEAR|E_NORC|E_SETLAST,
	    "ca1",
	    "[line [,line]] # [count] [l]",
	    "display numbered lines"},
/* C_SUBAGAIN */
	{"&",		ex_subagain,	E_ADDR2|E_NORC,
	    "s",
	    "[line [,line]] & [cgr] [count] [#lp]",
	    "repeat the last subsitution"},
/* C_STAR */
	{"*",		ex_at,		0,
	    "b",
	    "* [buffer]",
	    "execute a buffer"},
/* C_SHIFTL */
	{"<",		ex_shiftl,	E_ADDR2|E_AUTOPRINT|E_NORC,
	    "ca1",
	    "[line [,line]] <[<...] [count] [flags]",
	    "shift lines left"},
/* C_EQUAL */
	{"=",		ex_equal,	E_ADDR1|E_NORC,
	    "1",
	    "[line] = [flags]",
	    "display line number"},
/* C_SHIFTR */
	{">",		ex_shiftr,	E_ADDR2|E_AUTOPRINT|E_NORC,
	    "ca1",
	    "[line [,line]] >[>...] [count] [flags]",
	    "shift lines right"},
/* C_AT */
	{"@",		ex_at,		0,
	    "b",
	    "@ [buffer]",
	    "execute a buffer"},
/* C_APPEND */
	{"append",	ex_append,	E_ADDR1|E_NORC|E_ZERO|E_ZERODEF,
	    "!",
	    "[line] a[ppend][!]",
	    "append input to a line"},
/* C_ABBR */
	{"abbreviate", 	ex_abbr,	E_NOGLOBAL,
	    "W",
	    "ab[brev] word replace",
	    "specify an input abbreviation"},
/* C_ARGS */
	{"args",	ex_args,	E_NOGLOBAL|E_NORC,
	    "",
	    "ar[gs]",
	    "display file argument list"},
/* C_BG */
	{"bg",		ex_bg,		E_NOGLOBAL|E_NORC,
	    "",
	    "bg",
	    "background the current screen"},
/* C_CHANGE */
	{"change",	ex_change,	E_ADDR2|E_NORC|E_ZERODEF,
	    "!ca",
	    "[line [,line]] c[hange][!] [count]",
	    "change lines to input"},
/* C_CD */
	{"cd",		ex_cd,		E_NOGLOBAL,
	    "!f1o",
	    "cd[!] [directory]",
	    "change the current directory"},
/* C_CHDIR */
	{"chdir",	ex_cd,		E_NOGLOBAL,
	    "!f1o",
	    "chd[ir][!] [directory]",
	    "change the current directory"},
/* C_COPY */
	{"copy",	ex_copy,	E_ADDR2|E_AUTOPRINT|E_NORC,
	    "l1",
	    "[line [,line]] co[py] line [flags]",
	    "copy lines elsewhere in the file"},
/* C_DELETE */
	{"delete",	ex_delete,	E_ADDR2|E_AUTOPRINT|E_NORC,
	    "bca1",
	    "[line [,line]] d[elete] [buffer] [count] [flags]",
	    "delete lines from the file"},
/* C_DISPLAY */
	{"display",	ex_display,	E_NOGLOBAL|E_NORC,
	    "w1r",
	    "display b[uffers] | s[creens] | t[ags]",
	    "display buffers, screens or tags"},
/* C_DIGRAPH */
	{"digraph",	ex_digraph,	E_NOGLOBAL|E_NOPERM|E_NORC,
	    "",
	    "digraph",
	    "specify digraphs (not implemented)"},
/* C_EDIT */
	{"edit",	ex_edit,	E_NOGLOBAL|E_NORC,
	    "!f1o",
	    "e[dit][!] [+cmd] [file]",
	    "begin editing another file"},
/* C_EX */
	{"ex",		ex_edit,	E_NOGLOBAL|E_NORC,
	    "!f1o",
	    "ex[!] [+cmd] [file]",
	    "begin editing another file"},
/* C_EXUSAGE */
	{"exusage",	ex_usage,	E_NOGLOBAL|E_NORC,
	    "w1o",
	    "[exu]sage [command]",
	    "display ex command usage statement"},
/* C_FILE */
	{"file",	ex_file,	E_NOGLOBAL|E_NORC,
	    "f1o",
	    "f[ile] [name]",
	    "display (and optionally set) file name"},
/* C_FG */
	{"fg",		ex_fg,		E_NOGLOBAL|E_NORC,
	    "f1o",
	    "fg [file]",
	    "switch the current screen and a backgrounded screen"},
/* C_GLOBAL */
	{"global",	ex_global,	E_ADDR2_ALL|E_NOGLOBAL|E_NORC,
	    "!s",
	    "[line [,line]] g[lobal][!] [;/]RE[;/] [commands]",
	    "execute a global command on lines matching an RE"},
/* C_HELP */
	{"help",	ex_help,	E_NOGLOBAL|E_NORC,
	    "",
	    "he[lp]",
	    "display help statement"},
/* C_INSERT */
	{"insert",	ex_insert,	E_ADDR1|E_NORC,
	    "!",
	    "[line] i[nsert][!]",
	    "insert input before a line"},
/* C_JOIN */
	{"join",	ex_join,	E_ADDR2|E_AUTOPRINT|E_NORC,
	    "!ca1",
	    "[line [,line]] j[oin][!] [count] [flags]",
	    "join lines into a single line"},
/* C_K */
	{"k",		ex_mark,	E_ADDR1|E_NORC,
	    "w1r",
	    "[line] k key",
	    "mark a line position"},
/* C_LIST */
	{"list",	ex_list,	E_ADDR2|E_F_PRCLEAR|E_NORC|E_SETLAST,
	    "ca1",
	    "[line [,line]] l[ist] [count] [#]",
	    "display lines in an unambiguous form"},
/* C_MOVE */
	{"move",	ex_move,	E_ADDR2|E_AUTOPRINT|E_NORC,
	    "l",
	    "[line [,line]] m[ove] line",
	    "move lines elsewhere in the file"},
/* C_MARK */
	{"mark",	ex_mark,	E_ADDR1|E_NORC,
	    "w1r",
	    "[line] ma[rk] key",
	    "mark a line position"},
/* C_MAP */
	{"map",		ex_map,		0,
	    "!W",
	    "map[!] [keys replace]",
	    "map input or commands to one or more keys"},
/* C_MKEXRC */
	{"mkexrc",	ex_mkexrc,	E_NOGLOBAL|E_NORC,
	    "!f1r",
	    "mkexrc[!] file",
	    "write a .exrc file"},
/* C_NEXT */
	{"next",	ex_next,	E_NOGLOBAL|E_NORC,
	    "!fN",
	    "n[ext][!] [file ...]",
	    "edit (and optionally specify) the next file"},
/* C_NUMBER */
	{"number",	ex_number,	E_ADDR2|E_F_PRCLEAR|E_NORC|E_SETLAST,
	    "ca1",
	    "[line [,line]] nu[mber] [count] [l]",
	    "change display to number lines"},
/* C_OPEN */
	{"open",	ex_open,	E_ADDR1,
	    "s",
	    "[line] o[pen] [/RE/] [flags]",
	    "enter \"open\" mode (not implemented)"},
/* C_PRINT */
	{"print",	ex_pr,		E_ADDR2|E_F_PRCLEAR|E_NORC|E_SETLAST,
	    "ca1",
	    "[line [,line]] p[rint] [count] [#l]",
	    "display lines"},
/* C_PRESERVE */
	{"preserve",	ex_preserve,	E_NOGLOBAL|E_NORC,
	    "",
	    "pre[serve]",
	    "preserve an edit session for recovery"},
/* C_PREVIOUS */
	{"previous",	ex_prev,	E_NOGLOBAL|E_NORC,
	    "!",
	    "prev[ious][!]",
	    "edit the previous file in the file argument list"},
/* C_PUT */
	{"put",		ex_put,		E_ADDR1|E_AUTOPRINT|E_NORC|E_ZERO,
	    "b",
	    "[line] pu[t] [buffer]",
	    "append a cut buffer to the line"},
/* C_QUIT */
	{"quit",	ex_quit,	E_NOGLOBAL,
	    "!",
	    "q[uit][!]",
	    "exit ex/vi"},
/* C_READ */
	{"read",	ex_read,	E_ADDR1|E_NORC|E_ZERO|E_ZERODEF,
	    "!s",
	    "[line] r[ead] [!cmd | [file]]",
	    "append input from a command or file to the line"},
/* C_RESIZE */
	{"resize",	ex_resize,	E_NOGLOBAL|E_NORC,
	    "c+",
	    "resize [+-]rows",
	    "grow or shrink the current screen"},
/* C_REWIND */
	{"rewind",	ex_rew,		E_NOGLOBAL|E_NORC,
	    "!",
	    "rew[ind][!]",
	    "re-edit all the files in the file argument list"},
/* C_SUBSTITUTE */
	{"substitute",	ex_substitute,	E_ADDR2|E_NORC,
	    "s",
"[line [,line]] s[ubstitute] [[/;]RE[/;]/repl[/;] [cgr] [count] [#lp]]",
	    "substitute on lines matching an RE"},
/* C_SCRIPT */
	{"script",	ex_script,	E_NOGLOBAL|E_NORC,
	    "!f1o",
	    "sc[ript][!] [file]",
	    "run a shell in a screen"},
/* C_SET */
	{"set",		ex_set,		E_NOGLOBAL,
	    "wN",
	    "se[t] [option[=[value]]...] [nooption ...] [option? ...] [all]",
	    "set options (use \":set all\" to see all options)"},
/* C_SHELL */
	{"shell",	ex_shell,	E_NOGLOBAL|E_NORC,
	    "",
	    "sh[ell]",
	    "suspend editing and run a shell"},
/* C_SOURCE */
	{"source",	ex_source,	E_NOGLOBAL,
	    "f1r",
	    "so[urce] file",
	    "read a file of ex commands"},
/* C_SPLIT */
	{"split",	ex_split,	E_NOGLOBAL|E_NORC,
	    "fNo",
	    "sp[lit] [file ...]",
	    "split the current screen into two screens"},
/* C_STOP */
	{"stop",	ex_stop,	E_NOGLOBAL|E_NORC,
	    "!",
	    "st[op][!]",
	    "suspend the edit session"},
/* C_SUSPEND */
	{"suspend",	ex_stop,	E_NOGLOBAL|E_NORC,
	    "!",
	    "su[spend][!]",
	    "suspend the edit session"},
/* C_T */
	{"t",		ex_copy,	E_ADDR2|E_AUTOPRINT|E_NORC,
	    "l1",
	    "[line [,line]] t line [flags]",
	    "move lines elsewhere in the file"},
/* C_TAG */
	{"tag",		ex_tagpush,	E_NOGLOBAL,
	    "!w1o",
	    "ta[g][!] [string]",
	    "edit the file containing the tag"},
/* C_TAGPOP */
	{"tagpop",	ex_tagpop,	E_NOGLOBAL|E_NORC,
	    "!w1o",
	    "tagp[op][!] [number | file]",
	    "return to a previous tag"},
/* C_TAGTOP */
	{"tagtop",	ex_tagtop,	E_NOGLOBAL|E_NORC,
	    "!",
	    "tagt[op][!]",
	    "return to the first tag"},
/* C_UNDOL */
	{"Undo",	ex_undol,	E_AUTOPRINT|E_NOGLOBAL|E_NORC,
	    "",
	    "U[ndo]",
	    "undo all the changes to this line"},
/* C_UNDO */
	{"undo",	ex_undo,	E_AUTOPRINT|E_NOGLOBAL|E_NORC,
	    "",
	    "u[ndo]",
	    "undo the most recent change"},
/* C_UNABBREVIATE */
	{"unabbreviate",ex_unabbr,	E_NOGLOBAL,
	    "w1r",
	    "una[bbrev] word",
	    "delete an abbreviation"},
/* C_UNMAP */
	{"unmap",	ex_unmap,	E_NOGLOBAL,
	    "!w1r",
	    "unm[ap][!] word",
	    "delete an input or command map"},
/* C_VGLOBAL */
	{"vglobal",	ex_vglobal,	E_ADDR2_ALL|E_NOGLOBAL|E_NORC,
	    "s",
	    "[line [,line]] v[global] [;/]RE[;/] [commands]",
	    "execute a global command on lines NOT matching an RE"},
/* C_VERSION */
	{"version",	ex_version,	E_NOGLOBAL|E_NORC,
	    "",
	    "version",
	    "display the program version information"},
/* C_VISUAL_EX */
	{"visual",	ex_visual,	E_ADDR1|E_NOGLOBAL|E_NORC|E_ZERODEF,
	    "2c11",
	    "[line] vi[sual] [-|.|+|^] [window_size] [flags]",
	    "enter visual (vi) mode from ex mode"},
/* C_VISUAL_VI */
	{"visual",	ex_edit,	E_NOGLOBAL|E_NORC,
	    "!f1o",
	    "vi[sual][!] [+cmd] [file]",
	    "edit another file (from vi mode only)"},
/* C_VIUSAGE */
	{"viusage",	ex_viusage,	E_NOGLOBAL|E_NORC,
	    "w1o",
	    "[viu]sage [key]",
	    "display vi key usage statement"},
/* C_WRITE */
	{"write",	ex_write,	E_ADDR2_ALL|E_NOGLOBAL|E_NORC|E_ZERODEF,
	    "!s",
	    "[line [,line]] w[rite][!] [!cmd | [>>] [file]]",
	    "write the file"},
/* C_WQ */
	{"wq",		ex_wq,		E_ADDR2_ALL|E_NOGLOBAL|E_NORC|E_ZERODEF,
	    "!s",
	    "[line [,line]] wq[!] [>>] [file]",
	    "write the file and exit"},
/* C_XIT */
	{"xit",		ex_xit,		E_ADDR2_ALL|E_NOGLOBAL|E_NORC|E_ZERODEF,
	    "!f1o",
	    "[line [,line]] x[it][!] [file]",
	    "exit"},
/* C_YANK */
	{"yank",	ex_yank,	E_ADDR2|E_NORC,
	    "bca",
	    "[line [,line]] ya[nk] [buffer] [count]",
	    "copy lines to a cut buffer"},
/* C_Z */
	{"z",		ex_z,		E_ADDR1|E_NOGLOBAL|E_NORC,
	    "3c01",
	    "[line] z [-|.|+|^|=] [count] [flags]",
	    "display different screens of the file"},
/* C_SUBTILDE */
	{"~",		ex_subtilde,	E_ADDR2|E_NORC,
	    "s",
	    "[line [,line]] ~ [cgr] [count] [#lp]",
	    "replace previous RE with previous replacement string,"},
	{NULL},
};
