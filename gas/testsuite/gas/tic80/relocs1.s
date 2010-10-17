;; This is the hand hacked output of the TI C compiler for a simple
;; test program that contains local/global functions, local/global
;; function calls, and an "if" and "for" statement.

	.file	   "relocs1.s"

	.global	_xfunc

_sfunc:
         addu      -16,r1,r1
         st        12(r1),r31
         st        0(r1),r2
         jsr       _xfunc(r0),r31
         ld        0(r1),r2
         ld        12(r1),r31
         jsr       r31(r0),r0
         addu      16,r1,r1

	.global	_gfunc

_gfunc:
         addu      -16,r1,r1
         st        12(r1),r31
         st        0(r1),r2
         jsr       _sfunc(r0),r31
         ld        0(r1),r2
         ld        12(r1),r31
         jsr       r31(r0),r0
         addu      16,r1,r1


	.global	_branches

_branches:
         addu      -16,r1,r1
         st        12(r1),r31
         st        0(r1),r2
         ld        0(r1),r2
         st        4(r1),r2
         ld        0(r1),r2
         ld        4(r1),r3
         addu      10,r2,r2
         cmp       r3,r2,r2
         bbo.a     L12,r2,ge.w
L8:
         ld        4(r1),r2
         bbz.a     L10,r2,0
         jsr       _gfunc(r0),r31
         ld        4(r1),r2
         br.a      L11
L10:
         jsr       _xfunc(r0),r31
         ld        4(r1),r2
L11:
         ld        4(r1),r2
         addu      1,r2,r2
         st        4(r1),r2
         ld        0(r1),r3
         ld        4(r1),r2
         addu      10,r3,r3
         cmp       r2,r3,r2
         bbo.a     L8,r2,lt.w
L12:
         ld        12(r1),r31
         jsr       r31(r0),r0
         addu      16,r1,r1
