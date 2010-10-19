	.stabs	"/x/y/z/",100,0,2,Ltext0
	.stabs	"/blah/foo.c",100,0,2,Ltext0
	.text
Ltext0:
	.global _xyzzy
	.type	_xyzzy, @function
_xyzzy:
	.stabd	46,0,0
	.stabn	68,0,95,LM16-_xyzzy
LM16:
	.long 0
	.stabn	68,0,96,LM17-_xyzzy
LM17:
	.long globsym1
	.stabn	68,0,88,LM25-_xyzzy
LM25:
	.long 0
	.size	_xyzzy, .-_xyzzy
	.stabn	192,0,0,_xyzzy-_xyzzy
	.stabn	224,0,0,Lscope0-_xyzzy
Lscope0:
;# This is the stabs construct that was barfed upon; BFD for
;# a.out expects it to be of two parts, like the construct at
;# the top of this file.
	.stabs	"",100,0,0,Letext0
Letext0:
