dnl	This probably will not run on any m4 that cannot
dnl	handle char constants in eval.
dnl
changequote(<,>) define(HASHVAL,99) dnl
define(hash,<eval(str(substr($1,1),0)%HASHVAL)>) dnl
define(str,
	<ifelse($1,",$2,
		<str(substr(<$1>,1),<eval($2+'substr($1,0,1)')>)>)
	>) dnl
define(KEYWORD,<$1,hash($1),>) dnl
define(TSTART,
<struct prehash {
	char *keyword;
	int   hashval;
} keytab[] = {>) dnl
define(TEND,<	"",0
};>) dnl
