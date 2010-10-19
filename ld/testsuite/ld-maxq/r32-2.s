;
; test the intersegment relocation
; inderpreetb@noida.hcltech.com
.extern _start
.extern _abort
.extern _exit
.global _main
_main:
	call _exit
	call _abort
	ljump _abort
	ljump _exit

