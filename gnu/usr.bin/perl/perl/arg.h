/* $RCSfile: arg.h,v $$Revision: 1.1.1.1 $$Date: 1994/09/10 06:27:34 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: arg.h,v $
 * Revision 1.1.1.1  1994/09/10  06:27:34  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:34  nate
 * PERL!
 *
 * Revision 4.0.1.3  92/06/08  11:44:06  lwall
 * patch20: O_PIPE conflicted with Atari
 * patch20: clarified debugging output for literals and double-quoted strings
 * 
 * Revision 4.0.1.2  91/11/05  15:51:05  lwall
 * patch11: added eval {}
 * patch11: added sort {} LIST
 * 
 * Revision 4.0.1.1  91/06/07  10:18:30  lwall
 * patch4: length($`), length($&), length($') now optimized to avoid string copy
 * patch4: new copyright notice
 * patch4: many, many itty-bitty portability fixes
 * 
 * Revision 4.0  91/03/20  01:03:09  lwall
 * 4.0 baseline.
 * 
 */

#define O_NULL 0
#define O_RCAT 1
#define O_ITEM 2
#define O_SCALAR 3
#define O_ITEM2 4
#define O_ITEM3 5
#define O_CONCAT 6
#define O_REPEAT 7
#define O_MATCH 8
#define O_NMATCH 9
#define O_SUBST 10
#define O_NSUBST 11
#define O_ASSIGN 12
#define O_LOCAL 13
#define O_AASSIGN 14
#define O_SASSIGN 15
#define O_CHOP 16
#define O_DEFINED 17
#define O_UNDEF 18
#define O_STUDY 19
#define O_POW 20
#define O_MULTIPLY 21
#define O_DIVIDE 22
#define O_MODULO 23
#define O_ADD 24
#define O_SUBTRACT 25
#define O_LEFT_SHIFT 26
#define O_RIGHT_SHIFT 27
#define O_LT 28
#define O_GT 29
#define O_LE 30
#define O_GE 31
#define O_EQ 32
#define O_NE 33
#define O_NCMP 34
#define O_BIT_AND 35
#define O_XOR 36
#define O_BIT_OR 37
#define O_AND 38
#define O_OR 39
#define O_COND_EXPR 40
#define O_COMMA 41
#define O_NEGATE 42
#define O_NOT 43
#define O_COMPLEMENT 44
#define O_SELECT 45
#define O_WRITE 46
#define O_DBMOPEN 47
#define O_DBMCLOSE 48
#define O_OPEN 49
#define O_TRANS 50
#define O_NTRANS 51
#define O_CLOSE 52
#define O_EACH 53
#define O_VALUES 54
#define O_KEYS 55
#define O_LARRAY 56
#define O_ARRAY 57
#define O_AELEM 58
#define O_DELETE 59
#define O_LHASH 60
#define O_HASH 61
#define O_HELEM 62
#define O_LAELEM 63
#define O_LHELEM 64
#define O_LSLICE 65
#define O_ASLICE 66
#define O_HSLICE 67
#define O_LASLICE 68
#define O_LHSLICE 69
#define O_SPLICE 70
#define O_PUSH 71
#define O_POP 72
#define O_SHIFT 73
#define O_UNPACK 74
#define O_SPLIT 75
#define O_LENGTH 76
#define O_SPRINTF 77
#define O_SUBSTR 78
#define O_PACK 79
#define O_GREP 80
#define O_JOIN 81
#define O_SLT 82
#define O_SGT 83
#define O_SLE 84
#define O_SGE 85
#define O_SEQ 86
#define O_SNE 87
#define O_SCMP 88
#define O_SUBR 89
#define O_DBSUBR 90
#define O_CALLER 91
#define O_SORT 92
#define O_REVERSE 93
#define O_WARN 94
#define O_DIE 95
#define O_PRTF 96
#define O_PRINT 97
#define O_CHDIR 98
#define O_EXIT 99
#define O_RESET 100
#define O_LIST 101
#define O_EOF 102
#define O_GETC 103
#define O_TELL 104
#define O_RECV 105
#define O_READ 106
#define O_SYSREAD 107
#define O_SYSWRITE 108
#define O_SEND 109
#define O_SEEK 110
#define O_RETURN 111
#define O_REDO 112
#define O_NEXT 113
#define O_LAST 114
#define O_DUMP 115
#define O_GOTO 116
#define O_INDEX 117
#define O_RINDEX 118
#define O_TIME 119
#define O_TMS 120
#define O_LOCALTIME 121
#define O_GMTIME 122
#define O_TRUNCATE 123
#define O_LSTAT 124
#define O_STAT 125
#define O_CRYPT 126
#define O_ATAN2 127
#define O_SIN 128
#define O_COS 129
#define O_RAND 130
#define O_SRAND 131
#define O_EXP 132
#define O_LOG 133
#define O_SQRT 134
#define O_INT 135
#define O_ORD 136
#define O_ALARM 137
#define O_SLEEP 138
#define O_RANGE 139
#define O_F_OR_R 140
#define O_FLIP 141
#define O_FLOP 142
#define O_FORK 143
#define O_WAIT 144
#define O_WAITPID 145
#define O_SYSTEM 146
#define O_EXEC_OP 147
#define O_HEX 148
#define O_OCT 149
#define O_CHOWN 150
#define O_KILL 151
#define O_UNLINK 152
#define O_CHMOD 153
#define O_UTIME 154
#define O_UMASK 155
#define O_MSGGET 156
#define O_SHMGET 157
#define O_SEMGET 158
#define O_MSGCTL 159
#define O_SHMCTL 160
#define O_SEMCTL 161
#define O_MSGSND 162
#define O_MSGRCV 163
#define O_SEMOP 164
#define O_SHMREAD 165
#define O_SHMWRITE 166
#define O_RENAME 167
#define O_LINK 168
#define O_MKDIR 169
#define O_RMDIR 170
#define O_GETPPID 171
#define O_GETPGRP 172
#define O_SETPGRP 173
#define O_GETPRIORITY 174
#define O_SETPRIORITY 175
#define O_CHROOT 176
#define O_FCNTL 177
#define O_IOCTL 178
#define O_FLOCK 179
#define O_UNSHIFT 180
#define O_REQUIRE 181
#define O_DOFILE 182
#define O_EVAL 183
#define O_FTRREAD 184
#define O_FTRWRITE 185
#define O_FTREXEC 186
#define O_FTEREAD 187
#define O_FTEWRITE 188
#define O_FTEEXEC 189
#define O_FTIS 190
#define O_FTEOWNED 191
#define O_FTROWNED 192
#define O_FTZERO 193
#define O_FTSIZE 194
#define O_FTMTIME 195
#define O_FTATIME 196
#define O_FTCTIME 197
#define O_FTSOCK 198
#define O_FTCHR 199
#define O_FTBLK 200
#define O_FTFILE 201
#define O_FTDIR 202
#define O_FTPIPE 203
#define O_FTLINK 204
#define O_SYMLINK 205
#define O_READLINK 206
#define O_FTSUID 207
#define O_FTSGID 208
#define O_FTSVTX 209
#define O_FTTTY 210
#define O_FTTEXT 211
#define O_FTBINARY 212
#define O_SOCKET 213
#define O_BIND 214
#define O_CONNECT 215
#define O_LISTEN 216
#define O_ACCEPT 217
#define O_GHBYNAME 218
#define O_GHBYADDR 219
#define O_GHOSTENT 220
#define O_GNBYNAME 221
#define O_GNBYADDR 222
#define O_GNETENT 223
#define O_GPBYNAME 224
#define O_GPBYNUMBER 225
#define O_GPROTOENT 226
#define O_GSBYNAME 227
#define O_GSBYPORT 228
#define O_GSERVENT 229
#define O_SHOSTENT 230
#define O_SNETENT 231
#define O_SPROTOENT 232
#define O_SSERVENT 233
#define O_EHOSTENT 234
#define O_ENETENT 235
#define O_EPROTOENT 236
#define O_ESERVENT 237
#define O_SOCKPAIR 238
#define O_SHUTDOWN 239
#define O_GSOCKOPT 240
#define O_SSOCKOPT 241
#define O_GETSOCKNAME 242
#define O_GETPEERNAME 243
#define O_SSELECT 244
#define O_FILENO 245
#define O_BINMODE 246
#define O_VEC 247
#define O_GPWNAM 248
#define O_GPWUID 249
#define O_GPWENT 250
#define O_SPWENT 251
#define O_EPWENT 252
#define O_GGRNAM 253
#define O_GGRGID 254
#define O_GGRENT 255
#define O_SGRENT 256
#define O_EGRENT 257
#define O_GETLOGIN 258
#define O_OPEN_DIR 259
#define O_READDIR 260
#define O_TELLDIR 261
#define O_SEEKDIR 262
#define O_REWINDDIR 263
#define O_CLOSEDIR 264
#define O_SYSCALL 265
#define O_PIPE_OP 266
#define O_TRY 267
#define O_EVALONCE 268
#define MAXO 269

#ifndef DOINIT
extern char *opname[];
#else
char *opname[] = {
    "NULL",
    "RCAT",
    "ITEM",
    "SCALAR",
    "ITEM2",
    "ITEM3",
    "CONCAT",
    "REPEAT",
    "MATCH",
    "NMATCH",
    "SUBST",
    "NSUBST",
    "ASSIGN",
    "LOCAL",
    "AASSIGN",
    "SASSIGN",
    "CHOP",
    "DEFINED",
    "UNDEF",
    "STUDY",
    "POW",
    "MULTIPLY",
    "DIVIDE",
    "MODULO",
    "ADD",
    "SUBTRACT",
    "LEFT_SHIFT",
    "RIGHT_SHIFT",
    "LT",
    "GT",
    "LE",
    "GE",
    "EQ",
    "NE",
    "NCMP",
    "BIT_AND",
    "XOR",
    "BIT_OR",
    "AND",
    "OR",
    "COND_EXPR",
    "COMMA",
    "NEGATE",
    "NOT",
    "COMPLEMENT",
    "SELECT",
    "WRITE",
    "DBMOPEN",
    "DBMCLOSE",
    "OPEN",
    "TRANS",
    "NTRANS",
    "CLOSE",
    "EACH",
    "VALUES",
    "KEYS",
    "LARRAY",
    "ARRAY",
    "AELEM",
    "DELETE",
    "LHASH",
    "HASH",
    "HELEM",
    "LAELEM",
    "LHELEM",
    "LSLICE",
    "ASLICE",
    "HSLICE",
    "LASLICE",
    "LHSLICE",
    "SPLICE",
    "PUSH",
    "POP",
    "SHIFT",
    "UNPACK",
    "SPLIT",
    "LENGTH",
    "SPRINTF",
    "SUBSTR",
    "PACK",
    "GREP",
    "JOIN",
    "SLT",
    "SGT",
    "SLE",
    "SGE",
    "SEQ",
    "SNE",
    "SCMP",
    "SUBR",
    "DBSUBR",
    "CALLER",
    "SORT",
    "REVERSE",
    "WARN",
    "DIE",
    "PRINTF",
    "PRINT",
    "CHDIR",
    "EXIT",
    "RESET",
    "LIST",
    "EOF",
    "GETC",
    "TELL",
    "RECV",
    "READ",
    "SYSREAD",
    "SYSWRITE",
    "SEND",
    "SEEK",
    "RETURN",
    "REDO",
    "NEXT",
    "LAST",
    "DUMP",
    "GOTO",/* shudder */
    "INDEX",
    "RINDEX",
    "TIME",
    "TIMES",
    "LOCALTIME",
    "GMTIME",
    "TRUNCATE",
    "LSTAT",
    "STAT",
    "CRYPT",
    "ATAN2",
    "SIN",
    "COS",
    "RAND",
    "SRAND",
    "EXP",
    "LOG",
    "SQRT",
    "INT",
    "ORD",
    "ALARM",
    "SLEEP",
    "RANGE",
    "FLIP_OR_RANGE",
    "FLIP",
    "FLOP",
    "FORK",
    "WAIT",
    "WAITPID",
    "SYSTEM",
    "EXEC",
    "HEX",
    "OCT",
    "CHOWN",
    "KILL",
    "UNLINK",
    "CHMOD",
    "UTIME",
    "UMASK",
    "MSGGET",
    "SHMGET",
    "SEMGET",
    "MSGCTL",
    "SHMCTL",
    "SEMCTL",
    "MSGSND",
    "MSGRCV",
    "SEMOP",
    "SHMREAD",
    "SHMWRITE",
    "RENAME",
    "LINK",
    "MKDIR",
    "RMDIR",
    "GETPPID",
    "GETPGRP",
    "SETPGRP",
    "GETPRIORITY",
    "SETPRIORITY",
    "CHROOT",
    "FCNTL",
    "SYSIOCTL",
    "FLOCK",
    "UNSHIFT",
    "REQUIRE",
    "DOFILE",
    "EVAL",
    "FTRREAD",
    "FTRWRITE",
    "FTREXEC",
    "FTEREAD",
    "FTEWRITE",
    "FTEEXEC",
    "FTIS",
    "FTEOWNED",
    "FTROWNED",
    "FTZERO",
    "FTSIZE",
    "FTMTIME",
    "FTATIME",
    "FTCTIME",
    "FTSOCK",
    "FTCHR",
    "FTBLK",
    "FTFILE",
    "FTDIR",
    "FTPIPE",
    "FTLINK",
    "SYMLINK",
    "READLINK",
    "FTSUID",
    "FTSGID",
    "FTSVTX",
    "FTTTY",
    "FTTEXT",
    "FTBINARY",
    "SOCKET",
    "BIND",
    "CONNECT",
    "LISTEN",
    "ACCEPT",
    "GHBYNAME",
    "GHBYADDR",
    "GHOSTENT",
    "GNBYNAME",
    "GNBYADDR",
    "GNETENT",
    "GPBYNAME",
    "GPBYNUMBER",
    "GPROTOENT",
    "GSBYNAME",
    "GSBYPORT",
    "GSERVENT",
    "SHOSTENT",
    "SNETENT",
    "SPROTOENT",
    "SSERVENT",
    "EHOSTENT",
    "ENETENT",
    "EPROTOENT",
    "ESERVENT",
    "SOCKPAIR",
    "SHUTDOWN",
    "GSOCKOPT",
    "SSOCKOPT",
    "GETSOCKNAME",
    "GETPEERNAME",
    "SSELECT",
    "FILENO",
    "BINMODE",
    "VEC",
    "GPWNAM",
    "GPWUID",
    "GPWENT",
    "SPWENT",
    "EPWENT",
    "GGRNAM",
    "GGRGID",
    "GGRENT",
    "SGRENT",
    "EGRENT",
    "GETLOGIN",
    "OPENDIR",
    "READDIR",
    "TELLDIR",
    "SEEKDIR",
    "REWINDDIR",
    "CLOSEDIR",
    "SYSCALL",
    "PIPE",
    "TRY",
    "EVALONCE",
    "269"
};
#endif

#define A_NULL 0
#define A_EXPR 1
#define A_CMD 2
#define A_STAB 3
#define A_LVAL 4
#define A_SINGLE 5
#define A_DOUBLE 6
#define A_BACKTICK 7
#define A_READ 8
#define A_SPAT 9
#define A_LEXPR 10
#define A_ARYLEN 11
#define A_ARYSTAB 12
#define A_LARYLEN 13
#define A_GLOB 14
#define A_WORD 15
#define A_INDREAD 16
#define A_LARYSTAB 17
#define A_STAR 18
#define A_LSTAR 19
#define A_WANTARRAY 20
#define A_LENSTAB 21

#define A_MASK 31
#define A_DONT 32		/* or this into type to suppress evaluation */

#ifndef DOINIT
extern char *argname[];
#else
char *argname[] = {
    "A_NULL",
    "EXPR",
    "CMD",
    "STAB",
    "LVAL",
    "LITERAL",
    "DOUBLEQUOTE",
    "BACKTICK",
    "READ",
    "SPAT",
    "LEXPR",
    "ARYLEN",
    "ARYSTAB",
    "LARYLEN",
    "GLOB",
    "WORD",
    "INDREAD",
    "LARYSTAB",
    "STAR",
    "LSTAR",
    "WANTARRAY",
    "LENSTAB",
    "22"
};
#endif

#ifndef DOINIT
extern bool hoistable[];
#else
bool hoistable[] =
  {0,	/* A_NULL */
   0,	/* EXPR */
   1,	/* CMD */
   1,	/* STAB */
   0,	/* LVAL */
   1,	/* SINGLE */
   0,	/* DOUBLE */
   0,	/* BACKTICK */
   0,	/* READ */
   0,	/* SPAT */
   0,	/* LEXPR */
   1,	/* ARYLEN */
   1,	/* ARYSTAB */
   0,	/* LARYLEN */
   0,	/* GLOB */
   1,	/* WORD */
   0,	/* INDREAD */
   0,	/* LARYSTAB */
   1,	/* STAR */
   1,	/* LSTAR */
   1,	/* WANTARRAY */
   0,	/* LENSTAB */
   0,	/* 21 */
};
#endif

union argptr {
    ARG		*arg_arg;
    char	*arg_cval;
    STAB	*arg_stab;
    SPAT	*arg_spat;
    CMD		*arg_cmd;
    STR		*arg_str;
    HASH	*arg_hash;
};

struct arg {
    union argptr arg_ptr;
    short	arg_len;
    unsigned short arg_type;
    unsigned short arg_flags;
};

#define AF_ARYOK 1		/* op can handle multiple values here */
#define AF_POST 2		/* post *crement this item */
#define AF_PRE 4		/* pre *crement this item */
#define AF_UP 8			/* increment rather than decrement */
#define AF_COMMON 16		/* left and right have symbols in common */
#define AF_DEPR 32		/* an older form of the construct */
#define AF_LISTISH 64		/* turn into list if important */
#define AF_LOCAL_XX 128		/* list of local variables */

/*
 * Most of the ARG pointers are used as pointers to arrays of ARG.  When
 * so used, the 0th element is special, and represents the operator to
 * use on the list of arguments following.  The arg_len in the 0th element
 * gives the maximum argument number, and the arg_str is used to store
 * the return value in a more-or-less static location.  Sorry it's not
 * re-entrant (yet), but it sure makes it efficient.  The arg_type of the
 * 0th element is an operator (O_*) rather than an argument type (A_*).
 */

#define Nullarg Null(ARG*)

#ifndef DOINIT
EXT unsigned short opargs[MAXO+1];
#else
#define A(e1,e2,e3)        (e1+(e2<<2)+(e3<<4))
#define A5(e1,e2,e3,e4,e5) (e1+(e2<<2)+(e3<<4)+(e4<<6)+(e5<<8))
unsigned short opargs[MAXO+1] = {
	A(0,0,0),	/* NULL */
	A(1,1,0),	/* RCAT */
	A(1,0,0),	/* ITEM */
	A(1,0,0),	/* SCALAR */
	A(0,0,0),	/* ITEM2 */
	A(0,0,0),	/* ITEM3 */
	A(1,1,0),	/* CONCAT */
	A(3,1,0),	/* REPEAT */
	A(1,0,0),	/* MATCH */
	A(1,0,0),	/* NMATCH */
	A(1,0,0),	/* SUBST */
	A(1,0,0),	/* NSUBST */
	A(1,1,0),	/* ASSIGN */
	A(1,0,0),	/* LOCAL */
	A(3,3,0),	/* AASSIGN */
	A(0,0,0),	/* SASSIGN */
	A(3,0,0),	/* CHOP */
	A(1,0,0),	/* DEFINED */
	A(1,0,0),	/* UNDEF */
	A(1,0,0),	/* STUDY */
	A(1,1,0),	/* POW */
	A(1,1,0),	/* MULTIPLY */
	A(1,1,0),	/* DIVIDE */
	A(1,1,0),	/* MODULO */
	A(1,1,0),	/* ADD */
	A(1,1,0),	/* SUBTRACT */
	A(1,1,0),	/* LEFT_SHIFT */
	A(1,1,0),	/* RIGHT_SHIFT */
	A(1,1,0),	/* LT */
	A(1,1,0),	/* GT */
	A(1,1,0),	/* LE */
	A(1,1,0),	/* GE */
	A(1,1,0),	/* EQ */
	A(1,1,0),	/* NE */
	A(1,1,0),	/* NCMP */
	A(1,1,0),	/* BIT_AND */
	A(1,1,0),	/* XOR */
	A(1,1,0),	/* BIT_OR */
	A(1,0,0),	/* AND */
	A(1,0,0),	/* OR */
	A(1,0,0),	/* COND_EXPR */
	A(1,1,0),	/* COMMA */
	A(1,0,0),	/* NEGATE */
	A(1,0,0),	/* NOT */
	A(1,0,0),	/* COMPLEMENT */
	A(1,0,0),	/* SELECT */
	A(1,0,0),	/* WRITE */
	A(1,1,1),	/* DBMOPEN */
	A(1,0,0),	/* DBMCLOSE */
	A(1,1,0),	/* OPEN */
	A(1,0,0),	/* TRANS */
	A(1,0,0),	/* NTRANS */
	A(1,0,0),	/* CLOSE */
	A(0,0,0),	/* EACH */
	A(0,0,0),	/* VALUES */
	A(0,0,0),	/* KEYS */
	A(0,0,0),	/* LARRAY */
	A(0,0,0),	/* ARRAY */
	A(0,1,0),	/* AELEM */
	A(0,1,0),	/* DELETE */
	A(0,0,0),	/* LHASH */
	A(0,0,0),	/* HASH */
	A(0,1,0),	/* HELEM */
	A(0,1,0),	/* LAELEM */
	A(0,1,0),	/* LHELEM */
	A(0,3,3),	/* LSLICE */
	A(0,3,0),	/* ASLICE */
	A(0,3,0),	/* HSLICE */
	A(0,3,0),	/* LASLICE */
	A(0,3,0),	/* LHSLICE */
	A(0,3,1),	/* SPLICE */
	A(0,3,0),	/* PUSH */
	A(0,0,0),	/* POP */
	A(0,0,0),	/* SHIFT */
	A(1,1,0),	/* UNPACK */
	A(1,0,1),	/* SPLIT */
	A(1,0,0),	/* LENGTH */
	A(3,0,0),	/* SPRINTF */
	A(1,1,1),	/* SUBSTR */
	A(1,3,0),	/* PACK */
	A(0,3,0),	/* GREP */
	A(1,3,0),	/* JOIN */
	A(1,1,0),	/* SLT */
	A(1,1,0),	/* SGT */
	A(1,1,0),	/* SLE */
	A(1,1,0),	/* SGE */
	A(1,1,0),	/* SEQ */
	A(1,1,0),	/* SNE */
	A(1,1,0),	/* SCMP */
	A(0,3,0),	/* SUBR */
	A(0,3,0),	/* DBSUBR */
	A(1,0,0),	/* CALLER */
	A(1,3,0),	/* SORT */
	A(0,3,0),	/* REVERSE */
	A(0,3,0),	/* WARN */
	A(0,3,0),	/* DIE */
	A(1,3,0),	/* PRINTF */
	A(1,3,0),	/* PRINT */
	A(1,0,0),	/* CHDIR */
	A(1,0,0),	/* EXIT */
	A(1,0,0),	/* RESET */
	A(3,0,0),	/* LIST */
	A(1,0,0),	/* EOF */
	A(1,0,0),	/* GETC */
	A(1,0,0),	/* TELL */
	A5(1,1,1,1,0),	/* RECV */
	A(1,1,3),	/* READ */
	A(1,1,3),	/* SYSREAD */
	A(1,1,3),	/* SYSWRITE */
	A(1,1,3),	/* SEND */
	A(1,1,1),	/* SEEK */
	A(0,3,0),	/* RETURN */
	A(0,0,0),	/* REDO */
	A(0,0,0),	/* NEXT */
	A(0,0,0),	/* LAST */
	A(0,0,0),	/* DUMP */
	A(0,0,0),	/* GOTO */
	A(1,1,1),	/* INDEX */
	A(1,1,1),	/* RINDEX */
	A(0,0,0),	/* TIME */
	A(0,0,0),	/* TIMES */
	A(1,0,0),	/* LOCALTIME */
	A(1,0,0),	/* GMTIME */
	A(1,1,0),	/* TRUNCATE */
	A(1,0,0),	/* LSTAT */
	A(1,0,0),	/* STAT */
	A(1,1,0),	/* CRYPT */
	A(1,1,0),	/* ATAN2 */
	A(1,0,0),	/* SIN */
	A(1,0,0),	/* COS */
	A(1,0,0),	/* RAND */
	A(1,0,0),	/* SRAND */
	A(1,0,0),	/* EXP */
	A(1,0,0),	/* LOG */
	A(1,0,0),	/* SQRT */
	A(1,0,0),	/* INT */
	A(1,0,0),	/* ORD */
	A(1,0,0),	/* ALARM */
	A(1,0,0),	/* SLEEP */
	A(1,1,0),	/* RANGE */
	A(1,0,0),	/* F_OR_R */
	A(1,0,0),	/* FLIP */
	A(0,1,0),	/* FLOP */
	A(0,0,0),	/* FORK */
	A(0,0,0),	/* WAIT */
	A(1,1,0),	/* WAITPID */
	A(1,3,0),	/* SYSTEM */
	A(1,3,0),	/* EXEC */
	A(1,0,0),	/* HEX */
	A(1,0,0),	/* OCT */
	A(0,3,0),	/* CHOWN */
	A(0,3,0),	/* KILL */
	A(0,3,0),	/* UNLINK */
	A(0,3,0),	/* CHMOD */
	A(0,3,0),	/* UTIME */
	A(1,0,0),	/* UMASK */
	A(1,1,0),	/* MSGGET */
	A(1,1,1),	/* SHMGET */
	A(1,1,1),	/* SEMGET */
	A(1,1,1),	/* MSGCTL */
	A(1,1,1),	/* SHMCTL */
	A5(1,1,1,1,0),	/* SEMCTL */
	A(1,1,1),	/* MSGSND */
	A5(1,1,1,1,1),	/* MSGRCV */
	A(1,1,1),	/* SEMOP */
	A5(1,1,1,1,0),	/* SHMREAD */
	A5(1,1,1,1,0),	/* SHMWRITE */
	A(1,1,0),	/* RENAME */
	A(1,1,0),	/* LINK */
	A(1,1,0),	/* MKDIR */
	A(1,0,0),	/* RMDIR */
	A(0,0,0),	/* GETPPID */
	A(1,0,0),	/* GETPGRP */
	A(1,1,0),	/* SETPGRP */
	A(1,1,0),	/* GETPRIORITY */
	A(1,1,1),	/* SETPRIORITY */
	A(1,0,0),	/* CHROOT */
	A(1,1,1),	/* FCNTL */
	A(1,1,1),	/* SYSIOCTL */
	A(1,1,0),	/* FLOCK */
	A(0,3,0),	/* UNSHIFT */
	A(1,0,0),	/* REQUIRE */
	A(1,0,0),	/* DOFILE */
	A(1,0,0),	/* EVAL */
	A(1,0,0),	/* FTRREAD */
	A(1,0,0),	/* FTRWRITE */
	A(1,0,0),	/* FTREXEC */
	A(1,0,0),	/* FTEREAD */
	A(1,0,0),	/* FTEWRITE */
	A(1,0,0),	/* FTEEXEC */
	A(1,0,0),	/* FTIS */
	A(1,0,0),	/* FTEOWNED */
	A(1,0,0),	/* FTROWNED */
	A(1,0,0),	/* FTZERO */
	A(1,0,0),	/* FTSIZE */
	A(1,0,0),	/* FTMTIME */
	A(1,0,0),	/* FTATIME */
	A(1,0,0),	/* FTCTIME */
	A(1,0,0),	/* FTSOCK */
	A(1,0,0),	/* FTCHR */
	A(1,0,0),	/* FTBLK */
	A(1,0,0),	/* FTFILE */
	A(1,0,0),	/* FTDIR */
	A(1,0,0),	/* FTPIPE */
	A(1,0,0),	/* FTLINK */
	A(1,1,0),	/* SYMLINK */
	A(1,0,0),	/* READLINK */
	A(1,0,0),	/* FTSUID */
	A(1,0,0),	/* FTSGID */
	A(1,0,0),	/* FTSVTX */
	A(1,0,0),	/* FTTTY */
	A(1,0,0),	/* FTTEXT */
	A(1,0,0),	/* FTBINARY */
	A5(1,1,1,1,0),	/* SOCKET */
	A(1,1,0),	/* BIND */
	A(1,1,0),	/* CONNECT */
	A(1,1,0),	/* LISTEN */
	A(1,1,0),	/* ACCEPT */
	A(1,0,0),	/* GHBYNAME */
	A(1,1,0),	/* GHBYADDR */
	A(0,0,0),	/* GHOSTENT */
	A(1,0,0),	/* GNBYNAME */
	A(1,1,0),	/* GNBYADDR */
	A(0,0,0),	/* GNETENT */
	A(1,0,0),	/* GPBYNAME */
	A(1,0,0),	/* GPBYNUMBER */
	A(0,0,0),	/* GPROTOENT */
	A(1,1,0),	/* GSBYNAME */
	A(1,1,0),	/* GSBYPORT */
	A(0,0,0),	/* GSERVENT */
	A(1,0,0),	/* SHOSTENT */
	A(1,0,0),	/* SNETENT */
	A(1,0,0),	/* SPROTOENT */
	A(1,0,0),	/* SSERVENT */
	A(0,0,0),	/* EHOSTENT */
	A(0,0,0),	/* ENETENT */
	A(0,0,0),	/* EPROTOENT */
	A(0,0,0),	/* ESERVENT */
	A5(1,1,1,1,1),	/* SOCKPAIR */
	A(1,1,0),	/* SHUTDOWN */
	A(1,1,1),	/* GSOCKOPT */
	A5(1,1,1,1,0),	/* SSOCKOPT */
	A(1,0,0),	/* GETSOCKNAME */
	A(1,0,0),	/* GETPEERNAME */
	A5(1,1,1,1,0),	/* SSELECT */
	A(1,0,0),	/* FILENO */
	A(1,0,0),	/* BINMODE */
	A(1,1,1),	/* VEC */
	A(1,0,0),	/* GPWNAM */
	A(1,0,0),	/* GPWUID */
	A(0,0,0),	/* GPWENT */
	A(0,0,0),	/* SPWENT */
	A(0,0,0),	/* EPWENT */
	A(1,0,0),	/* GGRNAM */
	A(1,0,0),	/* GGRGID */
	A(0,0,0),	/* GGRENT */
	A(0,0,0),	/* SGRENT */
	A(0,0,0),	/* EGRENT */
	A(0,0,0),	/* GETLOGIN */
	A(1,1,0),	/* OPENDIR */
	A(1,0,0),	/* READDIR */
	A(1,0,0),	/* TELLDIR */
	A(1,1,0),	/* SEEKDIR */
	A(1,0,0),	/* REWINDDIR */
	A(1,0,0),	/* CLOSEDIR */
	A(1,3,0),	/* SYSCALL */
	A(1,1,0),	/* PIPE */
	A(0,0,0),	/* TRY */
	A(1,0,0),	/* EVALONCE */
	0
};
#undef A
#undef A5
#endif

int do_trans();
int do_split();
bool do_eof();
long do_tell();
bool do_seek();
int do_tms();
int do_time();
int do_stat();
STR *do_push();
FILE *nextargv();
STR *do_fttext();
int do_slice();
