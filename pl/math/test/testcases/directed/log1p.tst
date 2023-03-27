; log1p.tst
;
; Copyright (c) 2009-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=log1p op1=7ff80000.00000001 result=7ff80000.00000001 errno=0
func=log1p op1=fff80000.00000001 result=7ff80000.00000001 errno=0
func=log1p op1=7ff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=log1p op1=fff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=log1p op1=fff02000.00000000 result=7ff80000.00000001 errno=0 status=i
func=log1p op1=7ff00000.00000000 result=7ff00000.00000000 errno=0
; Cases 6, 9 , 10, 11, 12 fail with certain versions of GLIBC and not others.
; The main reason seems to be the handling of errno and exceptions.

func=log1p op1=00000000.00000000 result=00000000.00000000 errno=0
func=log1p op1=80000000.00000000 result=80000000.00000000 errno=0

; No exception is raised with certain versions of glibc. Functions
; approximated by x near zero may not generate/implement flops and
; thus may not raise exceptions.
func=log1p op1=00000000.00000001 result=00000000.00000001 errno=0 maybestatus=ux
func=log1p op1=80000000.00000001 result=80000000.00000001 errno=0 maybestatus=ux
