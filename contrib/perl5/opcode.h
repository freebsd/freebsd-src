#define pp_i_preinc pp_preinc
#define pp_i_predec pp_predec
#define pp_i_postinc pp_postinc
#define pp_i_postdec pp_postdec

typedef enum {
	OP_NULL,	/* 0 */
	OP_STUB,	/* 1 */
	OP_SCALAR,	/* 2 */
	OP_PUSHMARK,	/* 3 */
	OP_WANTARRAY,	/* 4 */
	OP_CONST,	/* 5 */
	OP_GVSV,	/* 6 */
	OP_GV,		/* 7 */
	OP_GELEM,	/* 8 */
	OP_PADSV,	/* 9 */
	OP_PADAV,	/* 10 */
	OP_PADHV,	/* 11 */
	OP_PADANY,	/* 12 */
	OP_PUSHRE,	/* 13 */
	OP_RV2GV,	/* 14 */
	OP_RV2SV,	/* 15 */
	OP_AV2ARYLEN,	/* 16 */
	OP_RV2CV,	/* 17 */
	OP_ANONCODE,	/* 18 */
	OP_PROTOTYPE,	/* 19 */
	OP_REFGEN,	/* 20 */
	OP_SREFGEN,	/* 21 */
	OP_REF,		/* 22 */
	OP_BLESS,	/* 23 */
	OP_BACKTICK,	/* 24 */
	OP_GLOB,	/* 25 */
	OP_READLINE,	/* 26 */
	OP_RCATLINE,	/* 27 */
	OP_REGCMAYBE,	/* 28 */
	OP_REGCRESET,	/* 29 */
	OP_REGCOMP,	/* 30 */
	OP_MATCH,	/* 31 */
	OP_QR,		/* 32 */
	OP_SUBST,	/* 33 */
	OP_SUBSTCONT,	/* 34 */
	OP_TRANS,	/* 35 */
	OP_SASSIGN,	/* 36 */
	OP_AASSIGN,	/* 37 */
	OP_CHOP,	/* 38 */
	OP_SCHOP,	/* 39 */
	OP_CHOMP,	/* 40 */
	OP_SCHOMP,	/* 41 */
	OP_DEFINED,	/* 42 */
	OP_UNDEF,	/* 43 */
	OP_STUDY,	/* 44 */
	OP_POS,		/* 45 */
	OP_PREINC,	/* 46 */
	OP_I_PREINC,	/* 47 */
	OP_PREDEC,	/* 48 */
	OP_I_PREDEC,	/* 49 */
	OP_POSTINC,	/* 50 */
	OP_I_POSTINC,	/* 51 */
	OP_POSTDEC,	/* 52 */
	OP_I_POSTDEC,	/* 53 */
	OP_POW,		/* 54 */
	OP_MULTIPLY,	/* 55 */
	OP_I_MULTIPLY,	/* 56 */
	OP_DIVIDE,	/* 57 */
	OP_I_DIVIDE,	/* 58 */
	OP_MODULO,	/* 59 */
	OP_I_MODULO,	/* 60 */
	OP_REPEAT,	/* 61 */
	OP_ADD,		/* 62 */
	OP_I_ADD,	/* 63 */
	OP_SUBTRACT,	/* 64 */
	OP_I_SUBTRACT,	/* 65 */
	OP_CONCAT,	/* 66 */
	OP_STRINGIFY,	/* 67 */
	OP_LEFT_SHIFT,	/* 68 */
	OP_RIGHT_SHIFT,	/* 69 */
	OP_LT,		/* 70 */
	OP_I_LT,	/* 71 */
	OP_GT,		/* 72 */
	OP_I_GT,	/* 73 */
	OP_LE,		/* 74 */
	OP_I_LE,	/* 75 */
	OP_GE,		/* 76 */
	OP_I_GE,	/* 77 */
	OP_EQ,		/* 78 */
	OP_I_EQ,	/* 79 */
	OP_NE,		/* 80 */
	OP_I_NE,	/* 81 */
	OP_NCMP,	/* 82 */
	OP_I_NCMP,	/* 83 */
	OP_SLT,		/* 84 */
	OP_SGT,		/* 85 */
	OP_SLE,		/* 86 */
	OP_SGE,		/* 87 */
	OP_SEQ,		/* 88 */
	OP_SNE,		/* 89 */
	OP_SCMP,	/* 90 */
	OP_BIT_AND,	/* 91 */
	OP_BIT_XOR,	/* 92 */
	OP_BIT_OR,	/* 93 */
	OP_NEGATE,	/* 94 */
	OP_I_NEGATE,	/* 95 */
	OP_NOT,		/* 96 */
	OP_COMPLEMENT,	/* 97 */
	OP_ATAN2,	/* 98 */
	OP_SIN,		/* 99 */
	OP_COS,		/* 100 */
	OP_RAND,	/* 101 */
	OP_SRAND,	/* 102 */
	OP_EXP,		/* 103 */
	OP_LOG,		/* 104 */
	OP_SQRT,	/* 105 */
	OP_INT,		/* 106 */
	OP_HEX,		/* 107 */
	OP_OCT,		/* 108 */
	OP_ABS,		/* 109 */
	OP_LENGTH,	/* 110 */
	OP_SUBSTR,	/* 111 */
	OP_VEC,		/* 112 */
	OP_INDEX,	/* 113 */
	OP_RINDEX,	/* 114 */
	OP_SPRINTF,	/* 115 */
	OP_FORMLINE,	/* 116 */
	OP_ORD,		/* 117 */
	OP_CHR,		/* 118 */
	OP_CRYPT,	/* 119 */
	OP_UCFIRST,	/* 120 */
	OP_LCFIRST,	/* 121 */
	OP_UC,		/* 122 */
	OP_LC,		/* 123 */
	OP_QUOTEMETA,	/* 124 */
	OP_RV2AV,	/* 125 */
	OP_AELEMFAST,	/* 126 */
	OP_AELEM,	/* 127 */
	OP_ASLICE,	/* 128 */
	OP_EACH,	/* 129 */
	OP_VALUES,	/* 130 */
	OP_KEYS,	/* 131 */
	OP_DELETE,	/* 132 */
	OP_EXISTS,	/* 133 */
	OP_RV2HV,	/* 134 */
	OP_HELEM,	/* 135 */
	OP_HSLICE,	/* 136 */
	OP_UNPACK,	/* 137 */
	OP_PACK,	/* 138 */
	OP_SPLIT,	/* 139 */
	OP_JOIN,	/* 140 */
	OP_LIST,	/* 141 */
	OP_LSLICE,	/* 142 */
	OP_ANONLIST,	/* 143 */
	OP_ANONHASH,	/* 144 */
	OP_SPLICE,	/* 145 */
	OP_PUSH,	/* 146 */
	OP_POP,		/* 147 */
	OP_SHIFT,	/* 148 */
	OP_UNSHIFT,	/* 149 */
	OP_SORT,	/* 150 */
	OP_REVERSE,	/* 151 */
	OP_GREPSTART,	/* 152 */
	OP_GREPWHILE,	/* 153 */
	OP_MAPSTART,	/* 154 */
	OP_MAPWHILE,	/* 155 */
	OP_RANGE,	/* 156 */
	OP_FLIP,	/* 157 */
	OP_FLOP,	/* 158 */
	OP_AND,		/* 159 */
	OP_OR,		/* 160 */
	OP_XOR,		/* 161 */
	OP_COND_EXPR,	/* 162 */
	OP_ANDASSIGN,	/* 163 */
	OP_ORASSIGN,	/* 164 */
	OP_METHOD,	/* 165 */
	OP_ENTERSUB,	/* 166 */
	OP_LEAVESUB,	/* 167 */
	OP_CALLER,	/* 168 */
	OP_WARN,	/* 169 */
	OP_DIE,		/* 170 */
	OP_RESET,	/* 171 */
	OP_LINESEQ,	/* 172 */
	OP_NEXTSTATE,	/* 173 */
	OP_DBSTATE,	/* 174 */
	OP_UNSTACK,	/* 175 */
	OP_ENTER,	/* 176 */
	OP_LEAVE,	/* 177 */
	OP_SCOPE,	/* 178 */
	OP_ENTERITER,	/* 179 */
	OP_ITER,	/* 180 */
	OP_ENTERLOOP,	/* 181 */
	OP_LEAVELOOP,	/* 182 */
	OP_RETURN,	/* 183 */
	OP_LAST,	/* 184 */
	OP_NEXT,	/* 185 */
	OP_REDO,	/* 186 */
	OP_DUMP,	/* 187 */
	OP_GOTO,	/* 188 */
	OP_EXIT,	/* 189 */
	OP_OPEN,	/* 190 */
	OP_CLOSE,	/* 191 */
	OP_PIPE_OP,	/* 192 */
	OP_FILENO,	/* 193 */
	OP_UMASK,	/* 194 */
	OP_BINMODE,	/* 195 */
	OP_TIE,		/* 196 */
	OP_UNTIE,	/* 197 */
	OP_TIED,	/* 198 */
	OP_DBMOPEN,	/* 199 */
	OP_DBMCLOSE,	/* 200 */
	OP_SSELECT,	/* 201 */
	OP_SELECT,	/* 202 */
	OP_GETC,	/* 203 */
	OP_READ,	/* 204 */
	OP_ENTERWRITE,	/* 205 */
	OP_LEAVEWRITE,	/* 206 */
	OP_PRTF,	/* 207 */
	OP_PRINT,	/* 208 */
	OP_SYSOPEN,	/* 209 */
	OP_SYSSEEK,	/* 210 */
	OP_SYSREAD,	/* 211 */
	OP_SYSWRITE,	/* 212 */
	OP_SEND,	/* 213 */
	OP_RECV,	/* 214 */
	OP_EOF,		/* 215 */
	OP_TELL,	/* 216 */
	OP_SEEK,	/* 217 */
	OP_TRUNCATE,	/* 218 */
	OP_FCNTL,	/* 219 */
	OP_IOCTL,	/* 220 */
	OP_FLOCK,	/* 221 */
	OP_SOCKET,	/* 222 */
	OP_SOCKPAIR,	/* 223 */
	OP_BIND,	/* 224 */
	OP_CONNECT,	/* 225 */
	OP_LISTEN,	/* 226 */
	OP_ACCEPT,	/* 227 */
	OP_SHUTDOWN,	/* 228 */
	OP_GSOCKOPT,	/* 229 */
	OP_SSOCKOPT,	/* 230 */
	OP_GETSOCKNAME,	/* 231 */
	OP_GETPEERNAME,	/* 232 */
	OP_LSTAT,	/* 233 */
	OP_STAT,	/* 234 */
	OP_FTRREAD,	/* 235 */
	OP_FTRWRITE,	/* 236 */
	OP_FTREXEC,	/* 237 */
	OP_FTEREAD,	/* 238 */
	OP_FTEWRITE,	/* 239 */
	OP_FTEEXEC,	/* 240 */
	OP_FTIS,	/* 241 */
	OP_FTEOWNED,	/* 242 */
	OP_FTROWNED,	/* 243 */
	OP_FTZERO,	/* 244 */
	OP_FTSIZE,	/* 245 */
	OP_FTMTIME,	/* 246 */
	OP_FTATIME,	/* 247 */
	OP_FTCTIME,	/* 248 */
	OP_FTSOCK,	/* 249 */
	OP_FTCHR,	/* 250 */
	OP_FTBLK,	/* 251 */
	OP_FTFILE,	/* 252 */
	OP_FTDIR,	/* 253 */
	OP_FTPIPE,	/* 254 */
	OP_FTLINK,	/* 255 */
	OP_FTSUID,	/* 256 */
	OP_FTSGID,	/* 257 */
	OP_FTSVTX,	/* 258 */
	OP_FTTTY,	/* 259 */
	OP_FTTEXT,	/* 260 */
	OP_FTBINARY,	/* 261 */
	OP_CHDIR,	/* 262 */
	OP_CHOWN,	/* 263 */
	OP_CHROOT,	/* 264 */
	OP_UNLINK,	/* 265 */
	OP_CHMOD,	/* 266 */
	OP_UTIME,	/* 267 */
	OP_RENAME,	/* 268 */
	OP_LINK,	/* 269 */
	OP_SYMLINK,	/* 270 */
	OP_READLINK,	/* 271 */
	OP_MKDIR,	/* 272 */
	OP_RMDIR,	/* 273 */
	OP_OPEN_DIR,	/* 274 */
	OP_READDIR,	/* 275 */
	OP_TELLDIR,	/* 276 */
	OP_SEEKDIR,	/* 277 */
	OP_REWINDDIR,	/* 278 */
	OP_CLOSEDIR,	/* 279 */
	OP_FORK,	/* 280 */
	OP_WAIT,	/* 281 */
	OP_WAITPID,	/* 282 */
	OP_SYSTEM,	/* 283 */
	OP_EXEC,	/* 284 */
	OP_KILL,	/* 285 */
	OP_GETPPID,	/* 286 */
	OP_GETPGRP,	/* 287 */
	OP_SETPGRP,	/* 288 */
	OP_GETPRIORITY,	/* 289 */
	OP_SETPRIORITY,	/* 290 */
	OP_TIME,	/* 291 */
	OP_TMS,		/* 292 */
	OP_LOCALTIME,	/* 293 */
	OP_GMTIME,	/* 294 */
	OP_ALARM,	/* 295 */
	OP_SLEEP,	/* 296 */
	OP_SHMGET,	/* 297 */
	OP_SHMCTL,	/* 298 */
	OP_SHMREAD,	/* 299 */
	OP_SHMWRITE,	/* 300 */
	OP_MSGGET,	/* 301 */
	OP_MSGCTL,	/* 302 */
	OP_MSGSND,	/* 303 */
	OP_MSGRCV,	/* 304 */
	OP_SEMGET,	/* 305 */
	OP_SEMCTL,	/* 306 */
	OP_SEMOP,	/* 307 */
	OP_REQUIRE,	/* 308 */
	OP_DOFILE,	/* 309 */
	OP_ENTEREVAL,	/* 310 */
	OP_LEAVEEVAL,	/* 311 */
	OP_ENTERTRY,	/* 312 */
	OP_LEAVETRY,	/* 313 */
	OP_GHBYNAME,	/* 314 */
	OP_GHBYADDR,	/* 315 */
	OP_GHOSTENT,	/* 316 */
	OP_GNBYNAME,	/* 317 */
	OP_GNBYADDR,	/* 318 */
	OP_GNETENT,	/* 319 */
	OP_GPBYNAME,	/* 320 */
	OP_GPBYNUMBER,	/* 321 */
	OP_GPROTOENT,	/* 322 */
	OP_GSBYNAME,	/* 323 */
	OP_GSBYPORT,	/* 324 */
	OP_GSERVENT,	/* 325 */
	OP_SHOSTENT,	/* 326 */
	OP_SNETENT,	/* 327 */
	OP_SPROTOENT,	/* 328 */
	OP_SSERVENT,	/* 329 */
	OP_EHOSTENT,	/* 330 */
	OP_ENETENT,	/* 331 */
	OP_EPROTOENT,	/* 332 */
	OP_ESERVENT,	/* 333 */
	OP_GPWNAM,	/* 334 */
	OP_GPWUID,	/* 335 */
	OP_GPWENT,	/* 336 */
	OP_SPWENT,	/* 337 */
	OP_EPWENT,	/* 338 */
	OP_GGRNAM,	/* 339 */
	OP_GGRGID,	/* 340 */
	OP_GGRENT,	/* 341 */
	OP_SGRENT,	/* 342 */
	OP_EGRENT,	/* 343 */
	OP_GETLOGIN,	/* 344 */
	OP_SYSCALL,	/* 345 */
	OP_LOCK,	/* 346 */
	OP_THREADSV,	/* 347 */
	OP_max		
} opcode;

#define MAXO 348

#ifndef DOINIT
EXT char *op_name[];
#else
EXT char *op_name[] = {
	"null",
	"stub",
	"scalar",
	"pushmark",
	"wantarray",
	"const",
	"gvsv",
	"gv",
	"gelem",
	"padsv",
	"padav",
	"padhv",
	"padany",
	"pushre",
	"rv2gv",
	"rv2sv",
	"av2arylen",
	"rv2cv",
	"anoncode",
	"prototype",
	"refgen",
	"srefgen",
	"ref",
	"bless",
	"backtick",
	"glob",
	"readline",
	"rcatline",
	"regcmaybe",
	"regcreset",
	"regcomp",
	"match",
	"qr",
	"subst",
	"substcont",
	"trans",
	"sassign",
	"aassign",
	"chop",
	"schop",
	"chomp",
	"schomp",
	"defined",
	"undef",
	"study",
	"pos",
	"preinc",
	"i_preinc",
	"predec",
	"i_predec",
	"postinc",
	"i_postinc",
	"postdec",
	"i_postdec",
	"pow",
	"multiply",
	"i_multiply",
	"divide",
	"i_divide",
	"modulo",
	"i_modulo",
	"repeat",
	"add",
	"i_add",
	"subtract",
	"i_subtract",
	"concat",
	"stringify",
	"left_shift",
	"right_shift",
	"lt",
	"i_lt",
	"gt",
	"i_gt",
	"le",
	"i_le",
	"ge",
	"i_ge",
	"eq",
	"i_eq",
	"ne",
	"i_ne",
	"ncmp",
	"i_ncmp",
	"slt",
	"sgt",
	"sle",
	"sge",
	"seq",
	"sne",
	"scmp",
	"bit_and",
	"bit_xor",
	"bit_or",
	"negate",
	"i_negate",
	"not",
	"complement",
	"atan2",
	"sin",
	"cos",
	"rand",
	"srand",
	"exp",
	"log",
	"sqrt",
	"int",
	"hex",
	"oct",
	"abs",
	"length",
	"substr",
	"vec",
	"index",
	"rindex",
	"sprintf",
	"formline",
	"ord",
	"chr",
	"crypt",
	"ucfirst",
	"lcfirst",
	"uc",
	"lc",
	"quotemeta",
	"rv2av",
	"aelemfast",
	"aelem",
	"aslice",
	"each",
	"values",
	"keys",
	"delete",
	"exists",
	"rv2hv",
	"helem",
	"hslice",
	"unpack",
	"pack",
	"split",
	"join",
	"list",
	"lslice",
	"anonlist",
	"anonhash",
	"splice",
	"push",
	"pop",
	"shift",
	"unshift",
	"sort",
	"reverse",
	"grepstart",
	"grepwhile",
	"mapstart",
	"mapwhile",
	"range",
	"flip",
	"flop",
	"and",
	"or",
	"xor",
	"cond_expr",
	"andassign",
	"orassign",
	"method",
	"entersub",
	"leavesub",
	"caller",
	"warn",
	"die",
	"reset",
	"lineseq",
	"nextstate",
	"dbstate",
	"unstack",
	"enter",
	"leave",
	"scope",
	"enteriter",
	"iter",
	"enterloop",
	"leaveloop",
	"return",
	"last",
	"next",
	"redo",
	"dump",
	"goto",
	"exit",
	"open",
	"close",
	"pipe_op",
	"fileno",
	"umask",
	"binmode",
	"tie",
	"untie",
	"tied",
	"dbmopen",
	"dbmclose",
	"sselect",
	"select",
	"getc",
	"read",
	"enterwrite",
	"leavewrite",
	"prtf",
	"print",
	"sysopen",
	"sysseek",
	"sysread",
	"syswrite",
	"send",
	"recv",
	"eof",
	"tell",
	"seek",
	"truncate",
	"fcntl",
	"ioctl",
	"flock",
	"socket",
	"sockpair",
	"bind",
	"connect",
	"listen",
	"accept",
	"shutdown",
	"gsockopt",
	"ssockopt",
	"getsockname",
	"getpeername",
	"lstat",
	"stat",
	"ftrread",
	"ftrwrite",
	"ftrexec",
	"fteread",
	"ftewrite",
	"fteexec",
	"ftis",
	"fteowned",
	"ftrowned",
	"ftzero",
	"ftsize",
	"ftmtime",
	"ftatime",
	"ftctime",
	"ftsock",
	"ftchr",
	"ftblk",
	"ftfile",
	"ftdir",
	"ftpipe",
	"ftlink",
	"ftsuid",
	"ftsgid",
	"ftsvtx",
	"fttty",
	"fttext",
	"ftbinary",
	"chdir",
	"chown",
	"chroot",
	"unlink",
	"chmod",
	"utime",
	"rename",
	"link",
	"symlink",
	"readlink",
	"mkdir",
	"rmdir",
	"open_dir",
	"readdir",
	"telldir",
	"seekdir",
	"rewinddir",
	"closedir",
	"fork",
	"wait",
	"waitpid",
	"system",
	"exec",
	"kill",
	"getppid",
	"getpgrp",
	"setpgrp",
	"getpriority",
	"setpriority",
	"time",
	"tms",
	"localtime",
	"gmtime",
	"alarm",
	"sleep",
	"shmget",
	"shmctl",
	"shmread",
	"shmwrite",
	"msgget",
	"msgctl",
	"msgsnd",
	"msgrcv",
	"semget",
	"semctl",
	"semop",
	"require",
	"dofile",
	"entereval",
	"leaveeval",
	"entertry",
	"leavetry",
	"ghbyname",
	"ghbyaddr",
	"ghostent",
	"gnbyname",
	"gnbyaddr",
	"gnetent",
	"gpbyname",
	"gpbynumber",
	"gprotoent",
	"gsbyname",
	"gsbyport",
	"gservent",
	"shostent",
	"snetent",
	"sprotoent",
	"sservent",
	"ehostent",
	"enetent",
	"eprotoent",
	"eservent",
	"gpwnam",
	"gpwuid",
	"gpwent",
	"spwent",
	"epwent",
	"ggrnam",
	"ggrgid",
	"ggrent",
	"sgrent",
	"egrent",
	"getlogin",
	"syscall",
	"lock",
	"threadsv",
};
#endif

#ifndef DOINIT
EXT char *op_desc[];
#else
EXT char *op_desc[] = {
	"null operation",
	"stub",
	"scalar",
	"pushmark",
	"wantarray",
	"constant item",
	"scalar variable",
	"glob value",
	"glob elem",
	"private variable",
	"private array",
	"private hash",
	"private something",
	"push regexp",
	"ref-to-glob cast",
	"scalar deref",
	"array length",
	"subroutine deref",
	"anonymous subroutine",
	"subroutine prototype",
	"reference constructor",
	"scalar ref constructor",
	"reference-type operator",
	"bless",
	"backticks",
	"glob",
	"<HANDLE>",
	"append I/O operator",
	"regexp comp once",
	"regexp reset interpolation flag",
	"regexp compilation",
	"pattern match",
	"pattern quote",
	"substitution",
	"substitution cont",
	"character translation",
	"scalar assignment",
	"list assignment",
	"chop",
	"scalar chop",
	"safe chop",
	"scalar safe chop",
	"defined operator",
	"undef operator",
	"study",
	"match position",
	"preincrement",
	"integer preincrement",
	"predecrement",
	"integer predecrement",
	"postincrement",
	"integer postincrement",
	"postdecrement",
	"integer postdecrement",
	"exponentiation",
	"multiplication",
	"integer multiplication",
	"division",
	"integer division",
	"modulus",
	"integer modulus",
	"repeat",
	"addition",
	"integer addition",
	"subtraction",
	"integer subtraction",
	"concatenation",
	"string",
	"left bitshift",
	"right bitshift",
	"numeric lt",
	"integer lt",
	"numeric gt",
	"integer gt",
	"numeric le",
	"integer le",
	"numeric ge",
	"integer ge",
	"numeric eq",
	"integer eq",
	"numeric ne",
	"integer ne",
	"spaceship operator",
	"integer spaceship",
	"string lt",
	"string gt",
	"string le",
	"string ge",
	"string eq",
	"string ne",
	"string comparison",
	"bitwise and",
	"bitwise xor",
	"bitwise or",
	"negate",
	"integer negate",
	"not",
	"1's complement",
	"atan2",
	"sin",
	"cos",
	"rand",
	"srand",
	"exp",
	"log",
	"sqrt",
	"int",
	"hex",
	"oct",
	"abs",
	"length",
	"substr",
	"vec",
	"index",
	"rindex",
	"sprintf",
	"formline",
	"ord",
	"chr",
	"crypt",
	"upper case first",
	"lower case first",
	"upper case",
	"lower case",
	"quote metachars",
	"array deref",
	"known array element",
	"array element",
	"array slice",
	"each",
	"values",
	"keys",
	"delete",
	"exists operator",
	"hash deref",
	"hash elem",
	"hash slice",
	"unpack",
	"pack",
	"split",
	"join",
	"list",
	"list slice",
	"anonymous list",
	"anonymous hash",
	"splice",
	"push",
	"pop",
	"shift",
	"unshift",
	"sort",
	"reverse",
	"grep",
	"grep iterator",
	"map",
	"map iterator",
	"flipflop",
	"range (or flip)",
	"range (or flop)",
	"logical and",
	"logical or",
	"logical xor",
	"conditional expression",
	"logical and assignment",
	"logical or assignment",
	"method lookup",
	"subroutine entry",
	"subroutine exit",
	"caller",
	"warn",
	"die",
	"reset",
	"line sequence",
	"next statement",
	"debug next statement",
	"iteration finalizer",
	"block entry",
	"block exit",
	"block",
	"foreach loop entry",
	"foreach loop iterator",
	"loop entry",
	"loop exit",
	"return",
	"last",
	"next",
	"redo",
	"dump",
	"goto",
	"exit",
	"open",
	"close",
	"pipe",
	"fileno",
	"umask",
	"binmode",
	"tie",
	"untie",
	"tied",
	"dbmopen",
	"dbmclose",
	"select system call",
	"select",
	"getc",
	"read",
	"write",
	"write exit",
	"printf",
	"print",
	"sysopen",
	"sysseek",
	"sysread",
	"syswrite",
	"send",
	"recv",
	"eof",
	"tell",
	"seek",
	"truncate",
	"fcntl",
	"ioctl",
	"flock",
	"socket",
	"socketpair",
	"bind",
	"connect",
	"listen",
	"accept",
	"shutdown",
	"getsockopt",
	"setsockopt",
	"getsockname",
	"getpeername",
	"lstat",
	"stat",
	"-R",
	"-W",
	"-X",
	"-r",
	"-w",
	"-x",
	"-e",
	"-O",
	"-o",
	"-z",
	"-s",
	"-M",
	"-A",
	"-C",
	"-S",
	"-c",
	"-b",
	"-f",
	"-d",
	"-p",
	"-l",
	"-u",
	"-g",
	"-k",
	"-t",
	"-T",
	"-B",
	"chdir",
	"chown",
	"chroot",
	"unlink",
	"chmod",
	"utime",
	"rename",
	"link",
	"symlink",
	"readlink",
	"mkdir",
	"rmdir",
	"opendir",
	"readdir",
	"telldir",
	"seekdir",
	"rewinddir",
	"closedir",
	"fork",
	"wait",
	"waitpid",
	"system",
	"exec",
	"kill",
	"getppid",
	"getpgrp",
	"setpgrp",
	"getpriority",
	"setpriority",
	"time",
	"times",
	"localtime",
	"gmtime",
	"alarm",
	"sleep",
	"shmget",
	"shmctl",
	"shmread",
	"shmwrite",
	"msgget",
	"msgctl",
	"msgsnd",
	"msgrcv",
	"semget",
	"semctl",
	"semop",
	"require",
	"do 'file'",
	"eval string",
	"eval exit",
	"eval block",
	"eval block exit",
	"gethostbyname",
	"gethostbyaddr",
	"gethostent",
	"getnetbyname",
	"getnetbyaddr",
	"getnetent",
	"getprotobyname",
	"getprotobynumber",
	"getprotoent",
	"getservbyname",
	"getservbyport",
	"getservent",
	"sethostent",
	"setnetent",
	"setprotoent",
	"setservent",
	"endhostent",
	"endnetent",
	"endprotoent",
	"endservent",
	"getpwnam",
	"getpwuid",
	"getpwent",
	"setpwent",
	"endpwent",
	"getgrnam",
	"getgrgid",
	"getgrent",
	"setgrent",
	"endgrent",
	"getlogin",
	"syscall",
	"lock",
	"per-thread variable",
};
#endif

#ifndef PERL_OBJECT
START_EXTERN_C

OP *	ck_anoncode	_((OP* o));
OP *	ck_bitop	_((OP* o));
OP *	ck_concat	_((OP* o));
OP *	ck_delete	_((OP* o));
OP *	ck_eof		_((OP* o));
OP *	ck_eval		_((OP* o));
OP *	ck_exec		_((OP* o));
OP *	ck_exists	_((OP* o));
OP *	ck_ftst		_((OP* o));
OP *	ck_fun		_((OP* o));
OP *	ck_fun_locale	_((OP* o));
OP *	ck_glob		_((OP* o));
OP *	ck_grep		_((OP* o));
OP *	ck_index	_((OP* o));
OP *	ck_lengthconst	_((OP* o));
OP *	ck_lfun		_((OP* o));
OP *	ck_listiob	_((OP* o));
OP *	ck_match	_((OP* o));
OP *	ck_null		_((OP* o));
OP *	ck_repeat	_((OP* o));
OP *	ck_require	_((OP* o));
OP *	ck_rfun		_((OP* o));
OP *	ck_rvconst	_((OP* o));
OP *	ck_scmp		_((OP* o));
OP *	ck_select	_((OP* o));
OP *	ck_shift	_((OP* o));
OP *	ck_sort		_((OP* o));
OP *	ck_spair	_((OP* o));
OP *	ck_split	_((OP* o));
OP *	ck_subr		_((OP* o));
OP *	ck_svconst	_((OP* o));
OP *	ck_trunc	_((OP* o));

OP *	pp_null		_((ARGSproto));
OP *	pp_stub		_((ARGSproto));
OP *	pp_scalar	_((ARGSproto));
OP *	pp_pushmark	_((ARGSproto));
OP *	pp_wantarray	_((ARGSproto));
OP *	pp_const	_((ARGSproto));
OP *	pp_gvsv		_((ARGSproto));
OP *	pp_gv		_((ARGSproto));
OP *	pp_gelem	_((ARGSproto));
OP *	pp_padsv	_((ARGSproto));
OP *	pp_padav	_((ARGSproto));
OP *	pp_padhv	_((ARGSproto));
OP *	pp_padany	_((ARGSproto));
OP *	pp_pushre	_((ARGSproto));
OP *	pp_rv2gv	_((ARGSproto));
OP *	pp_rv2sv	_((ARGSproto));
OP *	pp_av2arylen	_((ARGSproto));
OP *	pp_rv2cv	_((ARGSproto));
OP *	pp_anoncode	_((ARGSproto));
OP *	pp_prototype	_((ARGSproto));
OP *	pp_refgen	_((ARGSproto));
OP *	pp_srefgen	_((ARGSproto));
OP *	pp_ref		_((ARGSproto));
OP *	pp_bless	_((ARGSproto));
OP *	pp_backtick	_((ARGSproto));
OP *	pp_glob		_((ARGSproto));
OP *	pp_readline	_((ARGSproto));
OP *	pp_rcatline	_((ARGSproto));
OP *	pp_regcmaybe	_((ARGSproto));
OP *	pp_regcreset	_((ARGSproto));
OP *	pp_regcomp	_((ARGSproto));
OP *	pp_match	_((ARGSproto));
OP *	pp_qr		_((ARGSproto));
OP *	pp_subst	_((ARGSproto));
OP *	pp_substcont	_((ARGSproto));
OP *	pp_trans	_((ARGSproto));
OP *	pp_sassign	_((ARGSproto));
OP *	pp_aassign	_((ARGSproto));
OP *	pp_chop		_((ARGSproto));
OP *	pp_schop	_((ARGSproto));
OP *	pp_chomp	_((ARGSproto));
OP *	pp_schomp	_((ARGSproto));
OP *	pp_defined	_((ARGSproto));
OP *	pp_undef	_((ARGSproto));
OP *	pp_study	_((ARGSproto));
OP *	pp_pos		_((ARGSproto));
OP *	pp_preinc	_((ARGSproto));
OP *	pp_i_preinc	_((ARGSproto));
OP *	pp_predec	_((ARGSproto));
OP *	pp_i_predec	_((ARGSproto));
OP *	pp_postinc	_((ARGSproto));
OP *	pp_i_postinc	_((ARGSproto));
OP *	pp_postdec	_((ARGSproto));
OP *	pp_i_postdec	_((ARGSproto));
OP *	pp_pow		_((ARGSproto));
OP *	pp_multiply	_((ARGSproto));
OP *	pp_i_multiply	_((ARGSproto));
OP *	pp_divide	_((ARGSproto));
OP *	pp_i_divide	_((ARGSproto));
OP *	pp_modulo	_((ARGSproto));
OP *	pp_i_modulo	_((ARGSproto));
OP *	pp_repeat	_((ARGSproto));
OP *	pp_add		_((ARGSproto));
OP *	pp_i_add	_((ARGSproto));
OP *	pp_subtract	_((ARGSproto));
OP *	pp_i_subtract	_((ARGSproto));
OP *	pp_concat	_((ARGSproto));
OP *	pp_stringify	_((ARGSproto));
OP *	pp_left_shift	_((ARGSproto));
OP *	pp_right_shift	_((ARGSproto));
OP *	pp_lt		_((ARGSproto));
OP *	pp_i_lt		_((ARGSproto));
OP *	pp_gt		_((ARGSproto));
OP *	pp_i_gt		_((ARGSproto));
OP *	pp_le		_((ARGSproto));
OP *	pp_i_le		_((ARGSproto));
OP *	pp_ge		_((ARGSproto));
OP *	pp_i_ge		_((ARGSproto));
OP *	pp_eq		_((ARGSproto));
OP *	pp_i_eq		_((ARGSproto));
OP *	pp_ne		_((ARGSproto));
OP *	pp_i_ne		_((ARGSproto));
OP *	pp_ncmp		_((ARGSproto));
OP *	pp_i_ncmp	_((ARGSproto));
OP *	pp_slt		_((ARGSproto));
OP *	pp_sgt		_((ARGSproto));
OP *	pp_sle		_((ARGSproto));
OP *	pp_sge		_((ARGSproto));
OP *	pp_seq		_((ARGSproto));
OP *	pp_sne		_((ARGSproto));
OP *	pp_scmp		_((ARGSproto));
OP *	pp_bit_and	_((ARGSproto));
OP *	pp_bit_xor	_((ARGSproto));
OP *	pp_bit_or	_((ARGSproto));
OP *	pp_negate	_((ARGSproto));
OP *	pp_i_negate	_((ARGSproto));
OP *	pp_not		_((ARGSproto));
OP *	pp_complement	_((ARGSproto));
OP *	pp_atan2	_((ARGSproto));
OP *	pp_sin		_((ARGSproto));
OP *	pp_cos		_((ARGSproto));
OP *	pp_rand		_((ARGSproto));
OP *	pp_srand	_((ARGSproto));
OP *	pp_exp		_((ARGSproto));
OP *	pp_log		_((ARGSproto));
OP *	pp_sqrt		_((ARGSproto));
OP *	pp_int		_((ARGSproto));
OP *	pp_hex		_((ARGSproto));
OP *	pp_oct		_((ARGSproto));
OP *	pp_abs		_((ARGSproto));
OP *	pp_length	_((ARGSproto));
OP *	pp_substr	_((ARGSproto));
OP *	pp_vec		_((ARGSproto));
OP *	pp_index	_((ARGSproto));
OP *	pp_rindex	_((ARGSproto));
OP *	pp_sprintf	_((ARGSproto));
OP *	pp_formline	_((ARGSproto));
OP *	pp_ord		_((ARGSproto));
OP *	pp_chr		_((ARGSproto));
OP *	pp_crypt	_((ARGSproto));
OP *	pp_ucfirst	_((ARGSproto));
OP *	pp_lcfirst	_((ARGSproto));
OP *	pp_uc		_((ARGSproto));
OP *	pp_lc		_((ARGSproto));
OP *	pp_quotemeta	_((ARGSproto));
OP *	pp_rv2av	_((ARGSproto));
OP *	pp_aelemfast	_((ARGSproto));
OP *	pp_aelem	_((ARGSproto));
OP *	pp_aslice	_((ARGSproto));
OP *	pp_each		_((ARGSproto));
OP *	pp_values	_((ARGSproto));
OP *	pp_keys		_((ARGSproto));
OP *	pp_delete	_((ARGSproto));
OP *	pp_exists	_((ARGSproto));
OP *	pp_rv2hv	_((ARGSproto));
OP *	pp_helem	_((ARGSproto));
OP *	pp_hslice	_((ARGSproto));
OP *	pp_unpack	_((ARGSproto));
OP *	pp_pack		_((ARGSproto));
OP *	pp_split	_((ARGSproto));
OP *	pp_join		_((ARGSproto));
OP *	pp_list		_((ARGSproto));
OP *	pp_lslice	_((ARGSproto));
OP *	pp_anonlist	_((ARGSproto));
OP *	pp_anonhash	_((ARGSproto));
OP *	pp_splice	_((ARGSproto));
OP *	pp_push		_((ARGSproto));
OP *	pp_pop		_((ARGSproto));
OP *	pp_shift	_((ARGSproto));
OP *	pp_unshift	_((ARGSproto));
OP *	pp_sort		_((ARGSproto));
OP *	pp_reverse	_((ARGSproto));
OP *	pp_grepstart	_((ARGSproto));
OP *	pp_grepwhile	_((ARGSproto));
OP *	pp_mapstart	_((ARGSproto));
OP *	pp_mapwhile	_((ARGSproto));
OP *	pp_range	_((ARGSproto));
OP *	pp_flip		_((ARGSproto));
OP *	pp_flop		_((ARGSproto));
OP *	pp_and		_((ARGSproto));
OP *	pp_or		_((ARGSproto));
OP *	pp_xor		_((ARGSproto));
OP *	pp_cond_expr	_((ARGSproto));
OP *	pp_andassign	_((ARGSproto));
OP *	pp_orassign	_((ARGSproto));
OP *	pp_method	_((ARGSproto));
OP *	pp_entersub	_((ARGSproto));
OP *	pp_leavesub	_((ARGSproto));
OP *	pp_caller	_((ARGSproto));
OP *	pp_warn		_((ARGSproto));
OP *	pp_die		_((ARGSproto));
OP *	pp_reset	_((ARGSproto));
OP *	pp_lineseq	_((ARGSproto));
OP *	pp_nextstate	_((ARGSproto));
OP *	pp_dbstate	_((ARGSproto));
OP *	pp_unstack	_((ARGSproto));
OP *	pp_enter	_((ARGSproto));
OP *	pp_leave	_((ARGSproto));
OP *	pp_scope	_((ARGSproto));
OP *	pp_enteriter	_((ARGSproto));
OP *	pp_iter		_((ARGSproto));
OP *	pp_enterloop	_((ARGSproto));
OP *	pp_leaveloop	_((ARGSproto));
OP *	pp_return	_((ARGSproto));
OP *	pp_last		_((ARGSproto));
OP *	pp_next		_((ARGSproto));
OP *	pp_redo		_((ARGSproto));
OP *	pp_dump		_((ARGSproto));
OP *	pp_goto		_((ARGSproto));
OP *	pp_exit		_((ARGSproto));
OP *	pp_open		_((ARGSproto));
OP *	pp_close	_((ARGSproto));
OP *	pp_pipe_op	_((ARGSproto));
OP *	pp_fileno	_((ARGSproto));
OP *	pp_umask	_((ARGSproto));
OP *	pp_binmode	_((ARGSproto));
OP *	pp_tie		_((ARGSproto));
OP *	pp_untie	_((ARGSproto));
OP *	pp_tied		_((ARGSproto));
OP *	pp_dbmopen	_((ARGSproto));
OP *	pp_dbmclose	_((ARGSproto));
OP *	pp_sselect	_((ARGSproto));
OP *	pp_select	_((ARGSproto));
OP *	pp_getc		_((ARGSproto));
OP *	pp_read		_((ARGSproto));
OP *	pp_enterwrite	_((ARGSproto));
OP *	pp_leavewrite	_((ARGSproto));
OP *	pp_prtf		_((ARGSproto));
OP *	pp_print	_((ARGSproto));
OP *	pp_sysopen	_((ARGSproto));
OP *	pp_sysseek	_((ARGSproto));
OP *	pp_sysread	_((ARGSproto));
OP *	pp_syswrite	_((ARGSproto));
OP *	pp_send		_((ARGSproto));
OP *	pp_recv		_((ARGSproto));
OP *	pp_eof		_((ARGSproto));
OP *	pp_tell		_((ARGSproto));
OP *	pp_seek		_((ARGSproto));
OP *	pp_truncate	_((ARGSproto));
OP *	pp_fcntl	_((ARGSproto));
OP *	pp_ioctl	_((ARGSproto));
OP *	pp_flock	_((ARGSproto));
OP *	pp_socket	_((ARGSproto));
OP *	pp_sockpair	_((ARGSproto));
OP *	pp_bind		_((ARGSproto));
OP *	pp_connect	_((ARGSproto));
OP *	pp_listen	_((ARGSproto));
OP *	pp_accept	_((ARGSproto));
OP *	pp_shutdown	_((ARGSproto));
OP *	pp_gsockopt	_((ARGSproto));
OP *	pp_ssockopt	_((ARGSproto));
OP *	pp_getsockname	_((ARGSproto));
OP *	pp_getpeername	_((ARGSproto));
OP *	pp_lstat	_((ARGSproto));
OP *	pp_stat		_((ARGSproto));
OP *	pp_ftrread	_((ARGSproto));
OP *	pp_ftrwrite	_((ARGSproto));
OP *	pp_ftrexec	_((ARGSproto));
OP *	pp_fteread	_((ARGSproto));
OP *	pp_ftewrite	_((ARGSproto));
OP *	pp_fteexec	_((ARGSproto));
OP *	pp_ftis		_((ARGSproto));
OP *	pp_fteowned	_((ARGSproto));
OP *	pp_ftrowned	_((ARGSproto));
OP *	pp_ftzero	_((ARGSproto));
OP *	pp_ftsize	_((ARGSproto));
OP *	pp_ftmtime	_((ARGSproto));
OP *	pp_ftatime	_((ARGSproto));
OP *	pp_ftctime	_((ARGSproto));
OP *	pp_ftsock	_((ARGSproto));
OP *	pp_ftchr	_((ARGSproto));
OP *	pp_ftblk	_((ARGSproto));
OP *	pp_ftfile	_((ARGSproto));
OP *	pp_ftdir	_((ARGSproto));
OP *	pp_ftpipe	_((ARGSproto));
OP *	pp_ftlink	_((ARGSproto));
OP *	pp_ftsuid	_((ARGSproto));
OP *	pp_ftsgid	_((ARGSproto));
OP *	pp_ftsvtx	_((ARGSproto));
OP *	pp_fttty	_((ARGSproto));
OP *	pp_fttext	_((ARGSproto));
OP *	pp_ftbinary	_((ARGSproto));
OP *	pp_chdir	_((ARGSproto));
OP *	pp_chown	_((ARGSproto));
OP *	pp_chroot	_((ARGSproto));
OP *	pp_unlink	_((ARGSproto));
OP *	pp_chmod	_((ARGSproto));
OP *	pp_utime	_((ARGSproto));
OP *	pp_rename	_((ARGSproto));
OP *	pp_link		_((ARGSproto));
OP *	pp_symlink	_((ARGSproto));
OP *	pp_readlink	_((ARGSproto));
OP *	pp_mkdir	_((ARGSproto));
OP *	pp_rmdir	_((ARGSproto));
OP *	pp_open_dir	_((ARGSproto));
OP *	pp_readdir	_((ARGSproto));
OP *	pp_telldir	_((ARGSproto));
OP *	pp_seekdir	_((ARGSproto));
OP *	pp_rewinddir	_((ARGSproto));
OP *	pp_closedir	_((ARGSproto));
OP *	pp_fork		_((ARGSproto));
OP *	pp_wait		_((ARGSproto));
OP *	pp_waitpid	_((ARGSproto));
OP *	pp_system	_((ARGSproto));
OP *	pp_exec		_((ARGSproto));
OP *	pp_kill		_((ARGSproto));
OP *	pp_getppid	_((ARGSproto));
OP *	pp_getpgrp	_((ARGSproto));
OP *	pp_setpgrp	_((ARGSproto));
OP *	pp_getpriority	_((ARGSproto));
OP *	pp_setpriority	_((ARGSproto));
OP *	pp_time		_((ARGSproto));
OP *	pp_tms		_((ARGSproto));
OP *	pp_localtime	_((ARGSproto));
OP *	pp_gmtime	_((ARGSproto));
OP *	pp_alarm	_((ARGSproto));
OP *	pp_sleep	_((ARGSproto));
OP *	pp_shmget	_((ARGSproto));
OP *	pp_shmctl	_((ARGSproto));
OP *	pp_shmread	_((ARGSproto));
OP *	pp_shmwrite	_((ARGSproto));
OP *	pp_msgget	_((ARGSproto));
OP *	pp_msgctl	_((ARGSproto));
OP *	pp_msgsnd	_((ARGSproto));
OP *	pp_msgrcv	_((ARGSproto));
OP *	pp_semget	_((ARGSproto));
OP *	pp_semctl	_((ARGSproto));
OP *	pp_semop	_((ARGSproto));
OP *	pp_require	_((ARGSproto));
OP *	pp_dofile	_((ARGSproto));
OP *	pp_entereval	_((ARGSproto));
OP *	pp_leaveeval	_((ARGSproto));
OP *	pp_entertry	_((ARGSproto));
OP *	pp_leavetry	_((ARGSproto));
OP *	pp_ghbyname	_((ARGSproto));
OP *	pp_ghbyaddr	_((ARGSproto));
OP *	pp_ghostent	_((ARGSproto));
OP *	pp_gnbyname	_((ARGSproto));
OP *	pp_gnbyaddr	_((ARGSproto));
OP *	pp_gnetent	_((ARGSproto));
OP *	pp_gpbyname	_((ARGSproto));
OP *	pp_gpbynumber	_((ARGSproto));
OP *	pp_gprotoent	_((ARGSproto));
OP *	pp_gsbyname	_((ARGSproto));
OP *	pp_gsbyport	_((ARGSproto));
OP *	pp_gservent	_((ARGSproto));
OP *	pp_shostent	_((ARGSproto));
OP *	pp_snetent	_((ARGSproto));
OP *	pp_sprotoent	_((ARGSproto));
OP *	pp_sservent	_((ARGSproto));
OP *	pp_ehostent	_((ARGSproto));
OP *	pp_enetent	_((ARGSproto));
OP *	pp_eprotoent	_((ARGSproto));
OP *	pp_eservent	_((ARGSproto));
OP *	pp_gpwnam	_((ARGSproto));
OP *	pp_gpwuid	_((ARGSproto));
OP *	pp_gpwent	_((ARGSproto));
OP *	pp_spwent	_((ARGSproto));
OP *	pp_epwent	_((ARGSproto));
OP *	pp_ggrnam	_((ARGSproto));
OP *	pp_ggrgid	_((ARGSproto));
OP *	pp_ggrent	_((ARGSproto));
OP *	pp_sgrent	_((ARGSproto));
OP *	pp_egrent	_((ARGSproto));
OP *	pp_getlogin	_((ARGSproto));
OP *	pp_syscall	_((ARGSproto));
OP *	pp_lock		_((ARGSproto));
OP *	pp_threadsv	_((ARGSproto));

END_EXTERN_C
#endif	/* PERL_OBJECT */

#ifndef DOINIT
EXT OP * (CPERLscope(*ppaddr)[])(ARGSproto);
#else
#ifndef PERL_OBJECT
EXT OP * (CPERLscope(*ppaddr)[])(ARGSproto) = {
	pp_null,
	pp_stub,
	pp_scalar,
	pp_pushmark,
	pp_wantarray,
	pp_const,
	pp_gvsv,
	pp_gv,
	pp_gelem,
	pp_padsv,
	pp_padav,
	pp_padhv,
	pp_padany,
	pp_pushre,
	pp_rv2gv,
	pp_rv2sv,
	pp_av2arylen,
	pp_rv2cv,
	pp_anoncode,
	pp_prototype,
	pp_refgen,
	pp_srefgen,
	pp_ref,
	pp_bless,
	pp_backtick,
	pp_glob,
	pp_readline,
	pp_rcatline,
	pp_regcmaybe,
	pp_regcreset,
	pp_regcomp,
	pp_match,
	pp_qr,
	pp_subst,
	pp_substcont,
	pp_trans,
	pp_sassign,
	pp_aassign,
	pp_chop,
	pp_schop,
	pp_chomp,
	pp_schomp,
	pp_defined,
	pp_undef,
	pp_study,
	pp_pos,
	pp_preinc,
	pp_i_preinc,
	pp_predec,
	pp_i_predec,
	pp_postinc,
	pp_i_postinc,
	pp_postdec,
	pp_i_postdec,
	pp_pow,
	pp_multiply,
	pp_i_multiply,
	pp_divide,
	pp_i_divide,
	pp_modulo,
	pp_i_modulo,
	pp_repeat,
	pp_add,
	pp_i_add,
	pp_subtract,
	pp_i_subtract,
	pp_concat,
	pp_stringify,
	pp_left_shift,
	pp_right_shift,
	pp_lt,
	pp_i_lt,
	pp_gt,
	pp_i_gt,
	pp_le,
	pp_i_le,
	pp_ge,
	pp_i_ge,
	pp_eq,
	pp_i_eq,
	pp_ne,
	pp_i_ne,
	pp_ncmp,
	pp_i_ncmp,
	pp_slt,
	pp_sgt,
	pp_sle,
	pp_sge,
	pp_seq,
	pp_sne,
	pp_scmp,
	pp_bit_and,
	pp_bit_xor,
	pp_bit_or,
	pp_negate,
	pp_i_negate,
	pp_not,
	pp_complement,
	pp_atan2,
	pp_sin,
	pp_cos,
	pp_rand,
	pp_srand,
	pp_exp,
	pp_log,
	pp_sqrt,
	pp_int,
	pp_hex,
	pp_oct,
	pp_abs,
	pp_length,
	pp_substr,
	pp_vec,
	pp_index,
	pp_rindex,
	pp_sprintf,
	pp_formline,
	pp_ord,
	pp_chr,
	pp_crypt,
	pp_ucfirst,
	pp_lcfirst,
	pp_uc,
	pp_lc,
	pp_quotemeta,
	pp_rv2av,
	pp_aelemfast,
	pp_aelem,
	pp_aslice,
	pp_each,
	pp_values,
	pp_keys,
	pp_delete,
	pp_exists,
	pp_rv2hv,
	pp_helem,
	pp_hslice,
	pp_unpack,
	pp_pack,
	pp_split,
	pp_join,
	pp_list,
	pp_lslice,
	pp_anonlist,
	pp_anonhash,
	pp_splice,
	pp_push,
	pp_pop,
	pp_shift,
	pp_unshift,
	pp_sort,
	pp_reverse,
	pp_grepstart,
	pp_grepwhile,
	pp_mapstart,
	pp_mapwhile,
	pp_range,
	pp_flip,
	pp_flop,
	pp_and,
	pp_or,
	pp_xor,
	pp_cond_expr,
	pp_andassign,
	pp_orassign,
	pp_method,
	pp_entersub,
	pp_leavesub,
	pp_caller,
	pp_warn,
	pp_die,
	pp_reset,
	pp_lineseq,
	pp_nextstate,
	pp_dbstate,
	pp_unstack,
	pp_enter,
	pp_leave,
	pp_scope,
	pp_enteriter,
	pp_iter,
	pp_enterloop,
	pp_leaveloop,
	pp_return,
	pp_last,
	pp_next,
	pp_redo,
	pp_dump,
	pp_goto,
	pp_exit,
	pp_open,
	pp_close,
	pp_pipe_op,
	pp_fileno,
	pp_umask,
	pp_binmode,
	pp_tie,
	pp_untie,
	pp_tied,
	pp_dbmopen,
	pp_dbmclose,
	pp_sselect,
	pp_select,
	pp_getc,
	pp_read,
	pp_enterwrite,
	pp_leavewrite,
	pp_prtf,
	pp_print,
	pp_sysopen,
	pp_sysseek,
	pp_sysread,
	pp_syswrite,
	pp_send,
	pp_recv,
	pp_eof,
	pp_tell,
	pp_seek,
	pp_truncate,
	pp_fcntl,
	pp_ioctl,
	pp_flock,
	pp_socket,
	pp_sockpair,
	pp_bind,
	pp_connect,
	pp_listen,
	pp_accept,
	pp_shutdown,
	pp_gsockopt,
	pp_ssockopt,
	pp_getsockname,
	pp_getpeername,
	pp_lstat,
	pp_stat,
	pp_ftrread,
	pp_ftrwrite,
	pp_ftrexec,
	pp_fteread,
	pp_ftewrite,
	pp_fteexec,
	pp_ftis,
	pp_fteowned,
	pp_ftrowned,
	pp_ftzero,
	pp_ftsize,
	pp_ftmtime,
	pp_ftatime,
	pp_ftctime,
	pp_ftsock,
	pp_ftchr,
	pp_ftblk,
	pp_ftfile,
	pp_ftdir,
	pp_ftpipe,
	pp_ftlink,
	pp_ftsuid,
	pp_ftsgid,
	pp_ftsvtx,
	pp_fttty,
	pp_fttext,
	pp_ftbinary,
	pp_chdir,
	pp_chown,
	pp_chroot,
	pp_unlink,
	pp_chmod,
	pp_utime,
	pp_rename,
	pp_link,
	pp_symlink,
	pp_readlink,
	pp_mkdir,
	pp_rmdir,
	pp_open_dir,
	pp_readdir,
	pp_telldir,
	pp_seekdir,
	pp_rewinddir,
	pp_closedir,
	pp_fork,
	pp_wait,
	pp_waitpid,
	pp_system,
	pp_exec,
	pp_kill,
	pp_getppid,
	pp_getpgrp,
	pp_setpgrp,
	pp_getpriority,
	pp_setpriority,
	pp_time,
	pp_tms,
	pp_localtime,
	pp_gmtime,
	pp_alarm,
	pp_sleep,
	pp_shmget,
	pp_shmctl,
	pp_shmread,
	pp_shmwrite,
	pp_msgget,
	pp_msgctl,
	pp_msgsnd,
	pp_msgrcv,
	pp_semget,
	pp_semctl,
	pp_semop,
	pp_require,
	pp_dofile,
	pp_entereval,
	pp_leaveeval,
	pp_entertry,
	pp_leavetry,
	pp_ghbyname,
	pp_ghbyaddr,
	pp_ghostent,
	pp_gnbyname,
	pp_gnbyaddr,
	pp_gnetent,
	pp_gpbyname,
	pp_gpbynumber,
	pp_gprotoent,
	pp_gsbyname,
	pp_gsbyport,
	pp_gservent,
	pp_shostent,
	pp_snetent,
	pp_sprotoent,
	pp_sservent,
	pp_ehostent,
	pp_enetent,
	pp_eprotoent,
	pp_eservent,
	pp_gpwnam,
	pp_gpwuid,
	pp_gpwent,
	pp_spwent,
	pp_epwent,
	pp_ggrnam,
	pp_ggrgid,
	pp_ggrent,
	pp_sgrent,
	pp_egrent,
	pp_getlogin,
	pp_syscall,
	pp_lock,
	pp_threadsv,
};
#endif	/* PERL_OBJECT */
#endif

#ifndef DOINIT
EXT OP * (CPERLscope(*check)[]) _((OP *op));
#else
#ifndef PERL_OBJECT
EXT OP * (CPERLscope(*check)[]) _((OP *op)) = {
	ck_null,	/* null */
	ck_null,	/* stub */
	ck_fun,		/* scalar */
	ck_null,	/* pushmark */
	ck_null,	/* wantarray */
	ck_svconst,	/* const */
	ck_null,	/* gvsv */
	ck_null,	/* gv */
	ck_null,	/* gelem */
	ck_null,	/* padsv */
	ck_null,	/* padav */
	ck_null,	/* padhv */
	ck_null,	/* padany */
	ck_null,	/* pushre */
	ck_rvconst,	/* rv2gv */
	ck_rvconst,	/* rv2sv */
	ck_null,	/* av2arylen */
	ck_rvconst,	/* rv2cv */
	ck_anoncode,	/* anoncode */
	ck_null,	/* prototype */
	ck_spair,	/* refgen */
	ck_null,	/* srefgen */
	ck_fun,		/* ref */
	ck_fun,		/* bless */
	ck_null,	/* backtick */
	ck_glob,	/* glob */
	ck_null,	/* readline */
	ck_null,	/* rcatline */
	ck_fun,		/* regcmaybe */
	ck_fun,		/* regcreset */
	ck_null,	/* regcomp */
	ck_match,	/* match */
	ck_match,	/* qr */
	ck_null,	/* subst */
	ck_null,	/* substcont */
	ck_null,	/* trans */
	ck_null,	/* sassign */
	ck_null,	/* aassign */
	ck_spair,	/* chop */
	ck_null,	/* schop */
	ck_spair,	/* chomp */
	ck_null,	/* schomp */
	ck_rfun,	/* defined */
	ck_lfun,	/* undef */
	ck_fun,		/* study */
	ck_lfun,	/* pos */
	ck_lfun,	/* preinc */
	ck_lfun,	/* i_preinc */
	ck_lfun,	/* predec */
	ck_lfun,	/* i_predec */
	ck_lfun,	/* postinc */
	ck_lfun,	/* i_postinc */
	ck_lfun,	/* postdec */
	ck_lfun,	/* i_postdec */
	ck_null,	/* pow */
	ck_null,	/* multiply */
	ck_null,	/* i_multiply */
	ck_null,	/* divide */
	ck_null,	/* i_divide */
	ck_null,	/* modulo */
	ck_null,	/* i_modulo */
	ck_repeat,	/* repeat */
	ck_null,	/* add */
	ck_null,	/* i_add */
	ck_null,	/* subtract */
	ck_null,	/* i_subtract */
	ck_concat,	/* concat */
	ck_fun,		/* stringify */
	ck_bitop,	/* left_shift */
	ck_bitop,	/* right_shift */
	ck_null,	/* lt */
	ck_null,	/* i_lt */
	ck_null,	/* gt */
	ck_null,	/* i_gt */
	ck_null,	/* le */
	ck_null,	/* i_le */
	ck_null,	/* ge */
	ck_null,	/* i_ge */
	ck_null,	/* eq */
	ck_null,	/* i_eq */
	ck_null,	/* ne */
	ck_null,	/* i_ne */
	ck_null,	/* ncmp */
	ck_null,	/* i_ncmp */
	ck_scmp,	/* slt */
	ck_scmp,	/* sgt */
	ck_scmp,	/* sle */
	ck_scmp,	/* sge */
	ck_null,	/* seq */
	ck_null,	/* sne */
	ck_scmp,	/* scmp */
	ck_bitop,	/* bit_and */
	ck_bitop,	/* bit_xor */
	ck_bitop,	/* bit_or */
	ck_null,	/* negate */
	ck_null,	/* i_negate */
	ck_null,	/* not */
	ck_bitop,	/* complement */
	ck_fun,		/* atan2 */
	ck_fun,		/* sin */
	ck_fun,		/* cos */
	ck_fun,		/* rand */
	ck_fun,		/* srand */
	ck_fun,		/* exp */
	ck_fun,		/* log */
	ck_fun,		/* sqrt */
	ck_fun,		/* int */
	ck_fun,		/* hex */
	ck_fun,		/* oct */
	ck_fun,		/* abs */
	ck_lengthconst,	/* length */
	ck_fun,		/* substr */
	ck_fun,		/* vec */
	ck_index,	/* index */
	ck_index,	/* rindex */
	ck_fun_locale,	/* sprintf */
	ck_fun,		/* formline */
	ck_fun,		/* ord */
	ck_fun,		/* chr */
	ck_fun,		/* crypt */
	ck_fun_locale,	/* ucfirst */
	ck_fun_locale,	/* lcfirst */
	ck_fun_locale,	/* uc */
	ck_fun_locale,	/* lc */
	ck_fun,		/* quotemeta */
	ck_rvconst,	/* rv2av */
	ck_null,	/* aelemfast */
	ck_null,	/* aelem */
	ck_null,	/* aslice */
	ck_fun,		/* each */
	ck_fun,		/* values */
	ck_fun,		/* keys */
	ck_delete,	/* delete */
	ck_exists,	/* exists */
	ck_rvconst,	/* rv2hv */
	ck_null,	/* helem */
	ck_null,	/* hslice */
	ck_fun,		/* unpack */
	ck_fun,		/* pack */
	ck_split,	/* split */
	ck_fun,		/* join */
	ck_null,	/* list */
	ck_null,	/* lslice */
	ck_fun,		/* anonlist */
	ck_fun,		/* anonhash */
	ck_fun,		/* splice */
	ck_fun,		/* push */
	ck_shift,	/* pop */
	ck_shift,	/* shift */
	ck_fun,		/* unshift */
	ck_sort,	/* sort */
	ck_fun,		/* reverse */
	ck_grep,	/* grepstart */
	ck_null,	/* grepwhile */
	ck_grep,	/* mapstart */
	ck_null,	/* mapwhile */
	ck_null,	/* range */
	ck_null,	/* flip */
	ck_null,	/* flop */
	ck_null,	/* and */
	ck_null,	/* or */
	ck_null,	/* xor */
	ck_null,	/* cond_expr */
	ck_null,	/* andassign */
	ck_null,	/* orassign */
	ck_null,	/* method */
	ck_subr,	/* entersub */
	ck_null,	/* leavesub */
	ck_fun,		/* caller */
	ck_fun,		/* warn */
	ck_fun,		/* die */
	ck_fun,		/* reset */
	ck_null,	/* lineseq */
	ck_null,	/* nextstate */
	ck_null,	/* dbstate */
	ck_null,	/* unstack */
	ck_null,	/* enter */
	ck_null,	/* leave */
	ck_null,	/* scope */
	ck_null,	/* enteriter */
	ck_null,	/* iter */
	ck_null,	/* enterloop */
	ck_null,	/* leaveloop */
	ck_null,	/* return */
	ck_null,	/* last */
	ck_null,	/* next */
	ck_null,	/* redo */
	ck_null,	/* dump */
	ck_null,	/* goto */
	ck_fun,		/* exit */
	ck_fun,		/* open */
	ck_fun,		/* close */
	ck_fun,		/* pipe_op */
	ck_fun,		/* fileno */
	ck_fun,		/* umask */
	ck_fun,		/* binmode */
	ck_fun,		/* tie */
	ck_fun,		/* untie */
	ck_fun,		/* tied */
	ck_fun,		/* dbmopen */
	ck_fun,		/* dbmclose */
	ck_select,	/* sselect */
	ck_select,	/* select */
	ck_eof,		/* getc */
	ck_fun,		/* read */
	ck_fun,		/* enterwrite */
	ck_null,	/* leavewrite */
	ck_listiob,	/* prtf */
	ck_listiob,	/* print */
	ck_fun,		/* sysopen */
	ck_fun,		/* sysseek */
	ck_fun,		/* sysread */
	ck_fun,		/* syswrite */
	ck_fun,		/* send */
	ck_fun,		/* recv */
	ck_eof,		/* eof */
	ck_fun,		/* tell */
	ck_fun,		/* seek */
	ck_trunc,	/* truncate */
	ck_fun,		/* fcntl */
	ck_fun,		/* ioctl */
	ck_fun,		/* flock */
	ck_fun,		/* socket */
	ck_fun,		/* sockpair */
	ck_fun,		/* bind */
	ck_fun,		/* connect */
	ck_fun,		/* listen */
	ck_fun,		/* accept */
	ck_fun,		/* shutdown */
	ck_fun,		/* gsockopt */
	ck_fun,		/* ssockopt */
	ck_fun,		/* getsockname */
	ck_fun,		/* getpeername */
	ck_ftst,	/* lstat */
	ck_ftst,	/* stat */
	ck_ftst,	/* ftrread */
	ck_ftst,	/* ftrwrite */
	ck_ftst,	/* ftrexec */
	ck_ftst,	/* fteread */
	ck_ftst,	/* ftewrite */
	ck_ftst,	/* fteexec */
	ck_ftst,	/* ftis */
	ck_ftst,	/* fteowned */
	ck_ftst,	/* ftrowned */
	ck_ftst,	/* ftzero */
	ck_ftst,	/* ftsize */
	ck_ftst,	/* ftmtime */
	ck_ftst,	/* ftatime */
	ck_ftst,	/* ftctime */
	ck_ftst,	/* ftsock */
	ck_ftst,	/* ftchr */
	ck_ftst,	/* ftblk */
	ck_ftst,	/* ftfile */
	ck_ftst,	/* ftdir */
	ck_ftst,	/* ftpipe */
	ck_ftst,	/* ftlink */
	ck_ftst,	/* ftsuid */
	ck_ftst,	/* ftsgid */
	ck_ftst,	/* ftsvtx */
	ck_ftst,	/* fttty */
	ck_ftst,	/* fttext */
	ck_ftst,	/* ftbinary */
	ck_fun,		/* chdir */
	ck_fun,		/* chown */
	ck_fun,		/* chroot */
	ck_fun,		/* unlink */
	ck_fun,		/* chmod */
	ck_fun,		/* utime */
	ck_fun,		/* rename */
	ck_fun,		/* link */
	ck_fun,		/* symlink */
	ck_fun,		/* readlink */
	ck_fun,		/* mkdir */
	ck_fun,		/* rmdir */
	ck_fun,		/* open_dir */
	ck_fun,		/* readdir */
	ck_fun,		/* telldir */
	ck_fun,		/* seekdir */
	ck_fun,		/* rewinddir */
	ck_fun,		/* closedir */
	ck_null,	/* fork */
	ck_null,	/* wait */
	ck_fun,		/* waitpid */
	ck_exec,	/* system */
	ck_exec,	/* exec */
	ck_fun,		/* kill */
	ck_null,	/* getppid */
	ck_fun,		/* getpgrp */
	ck_fun,		/* setpgrp */
	ck_fun,		/* getpriority */
	ck_fun,		/* setpriority */
	ck_null,	/* time */
	ck_null,	/* tms */
	ck_fun,		/* localtime */
	ck_fun,		/* gmtime */
	ck_fun,		/* alarm */
	ck_fun,		/* sleep */
	ck_fun,		/* shmget */
	ck_fun,		/* shmctl */
	ck_fun,		/* shmread */
	ck_fun,		/* shmwrite */
	ck_fun,		/* msgget */
	ck_fun,		/* msgctl */
	ck_fun,		/* msgsnd */
	ck_fun,		/* msgrcv */
	ck_fun,		/* semget */
	ck_fun,		/* semctl */
	ck_fun,		/* semop */
	ck_require,	/* require */
	ck_fun,		/* dofile */
	ck_eval,	/* entereval */
	ck_null,	/* leaveeval */
	ck_null,	/* entertry */
	ck_null,	/* leavetry */
	ck_fun,		/* ghbyname */
	ck_fun,		/* ghbyaddr */
	ck_null,	/* ghostent */
	ck_fun,		/* gnbyname */
	ck_fun,		/* gnbyaddr */
	ck_null,	/* gnetent */
	ck_fun,		/* gpbyname */
	ck_fun,		/* gpbynumber */
	ck_null,	/* gprotoent */
	ck_fun,		/* gsbyname */
	ck_fun,		/* gsbyport */
	ck_null,	/* gservent */
	ck_fun,		/* shostent */
	ck_fun,		/* snetent */
	ck_fun,		/* sprotoent */
	ck_fun,		/* sservent */
	ck_null,	/* ehostent */
	ck_null,	/* enetent */
	ck_null,	/* eprotoent */
	ck_null,	/* eservent */
	ck_fun,		/* gpwnam */
	ck_fun,		/* gpwuid */
	ck_null,	/* gpwent */
	ck_null,	/* spwent */
	ck_null,	/* epwent */
	ck_fun,		/* ggrnam */
	ck_fun,		/* ggrgid */
	ck_null,	/* ggrent */
	ck_null,	/* sgrent */
	ck_null,	/* egrent */
	ck_null,	/* getlogin */
	ck_fun,		/* syscall */
	ck_rfun,	/* lock */
	ck_null,	/* threadsv */
};
#endif	/* PERL_OBJECT */
#endif

#ifndef DOINIT
EXT U32 opargs[];
#else
EXT U32 opargs[] = {
	0x00000000,	/* null */
	0x00000000,	/* stub */
	0x00001c04,	/* scalar */
	0x00000004,	/* pushmark */
	0x00000014,	/* wantarray */
	0x00000704,	/* const */
	0x00000844,	/* gvsv */
	0x00000844,	/* gv */
	0x00011240,	/* gelem */
	0x00000044,	/* padsv */
	0x00000040,	/* padav */
	0x00000040,	/* padhv */
	0x00000040,	/* padany */
	0x00000640,	/* pushre */
	0x00000144,	/* rv2gv */
	0x00000144,	/* rv2sv */
	0x00000114,	/* av2arylen */
	0x00000140,	/* rv2cv */
	0x00000700,	/* anoncode */
	0x00001c04,	/* prototype */
	0x00002101,	/* refgen */
	0x00001106,	/* srefgen */
	0x00009c8c,	/* ref */
	0x00091504,	/* bless */
	0x00000c08,	/* backtick */
	0x00099508,	/* glob */
	0x00000c08,	/* readline */
	0x00000c08,	/* rcatline */
	0x00001104,	/* regcmaybe */
	0x00001104,	/* regcreset */
	0x00001304,	/* regcomp */
	0x00000640,	/* match */
	0x00000604,	/* qr */
	0x00001654,	/* subst */
	0x00000354,	/* substcont */
	0x00001914,	/* trans */
	0x00000004,	/* sassign */
	0x00022208,	/* aassign */
	0x00002c0d,	/* chop */
	0x00009c8c,	/* schop */
	0x00002c0d,	/* chomp */
	0x00009c8c,	/* schomp */
	0x00009c94,	/* defined */
	0x00009c04,	/* undef */
	0x00009c84,	/* study */
	0x00009c8c,	/* pos */
	0x00001164,	/* preinc */
	0x00001154,	/* i_preinc */
	0x00001164,	/* predec */
	0x00001154,	/* i_predec */
	0x0000116c,	/* postinc */
	0x0000115c,	/* i_postinc */
	0x0000116c,	/* postdec */
	0x0000115c,	/* i_postdec */
	0x0001120e,	/* pow */
	0x0001122e,	/* multiply */
	0x0001121e,	/* i_multiply */
	0x0001122e,	/* divide */
	0x0001121e,	/* i_divide */
	0x0001123e,	/* modulo */
	0x0001121e,	/* i_modulo */
	0x00012209,	/* repeat */
	0x0001122e,	/* add */
	0x0001121e,	/* i_add */
	0x0001122e,	/* subtract */
	0x0001121e,	/* i_subtract */
	0x0001120e,	/* concat */
	0x0000150e,	/* stringify */
	0x0001120e,	/* left_shift */
	0x0001120e,	/* right_shift */
	0x00011236,	/* lt */
	0x00011216,	/* i_lt */
	0x00011236,	/* gt */
	0x00011216,	/* i_gt */
	0x00011236,	/* le */
	0x00011216,	/* i_le */
	0x00011236,	/* ge */
	0x00011216,	/* i_ge */
	0x00011236,	/* eq */
	0x00011216,	/* i_eq */
	0x00011236,	/* ne */
	0x00011216,	/* i_ne */
	0x0001123e,	/* ncmp */
	0x0001121e,	/* i_ncmp */
	0x00011216,	/* slt */
	0x00011216,	/* sgt */
	0x00011216,	/* sle */
	0x00011216,	/* sge */
	0x00011216,	/* seq */
	0x00011216,	/* sne */
	0x0001121e,	/* scmp */
	0x0001120e,	/* bit_and */
	0x0001120e,	/* bit_xor */
	0x0001120e,	/* bit_or */
	0x0000112e,	/* negate */
	0x0000111e,	/* i_negate */
	0x00001116,	/* not */
	0x0000110e,	/* complement */
	0x0001150e,	/* atan2 */
	0x00009c8e,	/* sin */
	0x00009c8e,	/* cos */
	0x00009c0c,	/* rand */
	0x00009c04,	/* srand */
	0x00009c8e,	/* exp */
	0x00009c8e,	/* log */
	0x00009c8e,	/* sqrt */
	0x00009c8e,	/* int */
	0x00009c8e,	/* hex */
	0x00009c8e,	/* oct */
	0x00009c8e,	/* abs */
	0x00009c9c,	/* length */
	0x0991150c,	/* substr */
	0x0011151c,	/* vec */
	0x0091151c,	/* index */
	0x0091151c,	/* rindex */
	0x0002150f,	/* sprintf */
	0x00021505,	/* formline */
	0x00009c9e,	/* ord */
	0x00009c8e,	/* chr */
	0x0001150e,	/* crypt */
	0x00009c8e,	/* ucfirst */
	0x00009c8e,	/* lcfirst */
	0x00009c8e,	/* uc */
	0x00009c8e,	/* lc */
	0x00009c8e,	/* quotemeta */
	0x00000148,	/* rv2av */
	0x00013804,	/* aelemfast */
	0x00013204,	/* aelem */
	0x00023501,	/* aslice */
	0x00004c08,	/* each */
	0x00004c08,	/* values */
	0x00004c08,	/* keys */
	0x00001c00,	/* delete */
	0x00001c14,	/* exists */
	0x00000148,	/* rv2hv */
	0x00014204,	/* helem */
	0x00024501,	/* hslice */
	0x00011500,	/* unpack */
	0x0002150d,	/* pack */
	0x00111508,	/* split */
	0x0002150d,	/* join */
	0x00002501,	/* list */
	0x00224200,	/* lslice */
	0x00002505,	/* anonlist */
	0x00002505,	/* anonhash */
	0x02993501,	/* splice */
	0x0002351d,	/* push */
	0x00003c04,	/* pop */
	0x00003c04,	/* shift */
	0x0002351d,	/* unshift */
	0x0002d501,	/* sort */
	0x00002509,	/* reverse */
	0x00025541,	/* grepstart */
	0x00000348,	/* grepwhile */
	0x00025541,	/* mapstart */
	0x00000348,	/* mapwhile */
	0x00011400,	/* range */
	0x00011100,	/* flip */
	0x00000100,	/* flop */
	0x00000300,	/* and */
	0x00000300,	/* or */
	0x00011306,	/* xor */
	0x00000440,	/* cond_expr */
	0x00000304,	/* andassign */
	0x00000304,	/* orassign */
	0x00000140,	/* method */
	0x00002149,	/* entersub */
	0x00000100,	/* leavesub */
	0x00009c08,	/* caller */
	0x0000251d,	/* warn */
	0x0000255d,	/* die */
	0x00009c14,	/* reset */
	0x00000500,	/* lineseq */
	0x00000b04,	/* nextstate */
	0x00000b04,	/* dbstate */
	0x00000004,	/* unstack */
	0x00000000,	/* enter */
	0x00000500,	/* leave */
	0x00000500,	/* scope */
	0x00000a40,	/* enteriter */
	0x00000000,	/* iter */
	0x00000a40,	/* enterloop */
	0x00000200,	/* leaveloop */
	0x00002541,	/* return */
	0x00000e44,	/* last */
	0x00000e44,	/* next */
	0x00000e44,	/* redo */
	0x00000e44,	/* dump */
	0x00000e44,	/* goto */
	0x00009c44,	/* exit */
	0x0009651c,	/* open */
	0x0000ec14,	/* close */
	0x00066514,	/* pipe_op */
	0x00006c1c,	/* fileno */
	0x00009c1c,	/* umask */
	0x00006c04,	/* binmode */
	0x00217555,	/* tie */
	0x00007c14,	/* untie */
	0x00007c04,	/* tied */
	0x00114514,	/* dbmopen */
	0x00004c14,	/* dbmclose */
	0x01111508,	/* sselect */
	0x0000e50c,	/* select */
	0x0000ec0c,	/* getc */
	0x0917651d,	/* read */
	0x0000ec54,	/* enterwrite */
	0x00000100,	/* leavewrite */
	0x0002e515,	/* prtf */
	0x0002e515,	/* print */
	0x09116504,	/* sysopen */
	0x00116504,	/* sysseek */
	0x0917651d,	/* sysread */
	0x0991651d,	/* syswrite */
	0x0911651d,	/* send */
	0x0117651d,	/* recv */
	0x0000ec14,	/* eof */
	0x0000ec0c,	/* tell */
	0x00116504,	/* seek */
	0x00011514,	/* truncate */
	0x0011650c,	/* fcntl */
	0x0011650c,	/* ioctl */
	0x0001651c,	/* flock */
	0x01116514,	/* socket */
	0x11166514,	/* sockpair */
	0x00016514,	/* bind */
	0x00016514,	/* connect */
	0x00016514,	/* listen */
	0x0006651c,	/* accept */
	0x0001651c,	/* shutdown */
	0x00116514,	/* gsockopt */
	0x01116514,	/* ssockopt */
	0x00006c14,	/* getsockname */
	0x00006c14,	/* getpeername */
	0x00006d80,	/* lstat */
	0x00006d80,	/* stat */
	0x00006d94,	/* ftrread */
	0x00006d94,	/* ftrwrite */
	0x00006d94,	/* ftrexec */
	0x00006d94,	/* fteread */
	0x00006d94,	/* ftewrite */
	0x00006d94,	/* fteexec */
	0x00006d94,	/* ftis */
	0x00006d94,	/* fteowned */
	0x00006d94,	/* ftrowned */
	0x00006d94,	/* ftzero */
	0x00006d9c,	/* ftsize */
	0x00006d8c,	/* ftmtime */
	0x00006d8c,	/* ftatime */
	0x00006d8c,	/* ftctime */
	0x00006d94,	/* ftsock */
	0x00006d94,	/* ftchr */
	0x00006d94,	/* ftblk */
	0x00006d94,	/* ftfile */
	0x00006d94,	/* ftdir */
	0x00006d94,	/* ftpipe */
	0x00006d94,	/* ftlink */
	0x00006d94,	/* ftsuid */
	0x00006d94,	/* ftsgid */
	0x00006d94,	/* ftsvtx */
	0x00006d14,	/* fttty */
	0x00006d94,	/* fttext */
	0x00006d94,	/* ftbinary */
	0x00009c1c,	/* chdir */
	0x0000251d,	/* chown */
	0x00009c9c,	/* chroot */
	0x0000259d,	/* unlink */
	0x0000251d,	/* chmod */
	0x0000251d,	/* utime */
	0x0001151c,	/* rename */
	0x0001151c,	/* link */
	0x0001151c,	/* symlink */
	0x00009c8c,	/* readlink */
	0x0001151c,	/* mkdir */
	0x00009c9c,	/* rmdir */
	0x00016514,	/* open_dir */
	0x00006c00,	/* readdir */
	0x00006c0c,	/* telldir */
	0x00016504,	/* seekdir */
	0x00006c04,	/* rewinddir */
	0x00006c14,	/* closedir */
	0x0000001c,	/* fork */
	0x0000001c,	/* wait */
	0x0001151c,	/* waitpid */
	0x0002951d,	/* system */
	0x0002955d,	/* exec */
	0x0000255d,	/* kill */
	0x0000001c,	/* getppid */
	0x00009c1c,	/* getpgrp */
	0x0009951c,	/* setpgrp */
	0x0001151c,	/* getpriority */
	0x0011151c,	/* setpriority */
	0x0000001c,	/* time */
	0x00000000,	/* tms */
	0x00009c08,	/* localtime */
	0x00009c08,	/* gmtime */
	0x00009c9c,	/* alarm */
	0x00009c1c,	/* sleep */
	0x0011151d,	/* shmget */
	0x0011151d,	/* shmctl */
	0x0111151d,	/* shmread */
	0x0111151d,	/* shmwrite */
	0x0001151d,	/* msgget */
	0x0011151d,	/* msgctl */
	0x0011151d,	/* msgsnd */
	0x1111151d,	/* msgrcv */
	0x0011151d,	/* semget */
	0x0111151d,	/* semctl */
	0x0001151d,	/* semop */
	0x00009cc0,	/* require */
	0x00001140,	/* dofile */
	0x00001c40,	/* entereval */
	0x00001100,	/* leaveeval */
	0x00000300,	/* entertry */
	0x00000500,	/* leavetry */
	0x00001c00,	/* ghbyname */
	0x00011500,	/* ghbyaddr */
	0x00000000,	/* ghostent */
	0x00001c00,	/* gnbyname */
	0x00011500,	/* gnbyaddr */
	0x00000000,	/* gnetent */
	0x00001c00,	/* gpbyname */
	0x00001500,	/* gpbynumber */
	0x00000000,	/* gprotoent */
	0x00011500,	/* gsbyname */
	0x00011500,	/* gsbyport */
	0x00000000,	/* gservent */
	0x00001c14,	/* shostent */
	0x00001c14,	/* snetent */
	0x00001c14,	/* sprotoent */
	0x00001c14,	/* sservent */
	0x00000014,	/* ehostent */
	0x00000014,	/* enetent */
	0x00000014,	/* eprotoent */
	0x00000014,	/* eservent */
	0x00001c00,	/* gpwnam */
	0x00001c00,	/* gpwuid */
	0x00000000,	/* gpwent */
	0x00000014,	/* spwent */
	0x00000014,	/* epwent */
	0x00001c00,	/* ggrnam */
	0x00001c00,	/* ggrgid */
	0x00000000,	/* ggrent */
	0x00000014,	/* sgrent */
	0x00000014,	/* egrent */
	0x0000000c,	/* getlogin */
	0x0002151d,	/* syscall */
	0x00001c04,	/* lock */
	0x00000044,	/* threadsv */
};
#endif
