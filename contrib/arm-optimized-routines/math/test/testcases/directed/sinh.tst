; sinh.tst
;
; Copyright (c) 1999-2024, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=sinh op1=7ff80000.00000001 result=7ff80000.00000001 errno=0
func=sinh op1=fff80000.00000001 result=7ff80000.00000001 errno=0
func=sinh op1=7ff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=sinh op1=fff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=sinh op1=7ff00000.00000000 result=7ff00000.00000000 errno=0
func=sinh op1=7fefffff.ffffffff result=7ff00000.00000000 errno=ERANGE status=ox
func=sinh op1=fff00000.00000000 result=fff00000.00000000 errno=0
func=sinh op1=ffefffff.ffffffff result=fff00000.00000000 errno=ERANGE status=ox
func=sinh op1=00000000.00000000 result=00000000.00000000 errno=0
func=sinh op1=80000000.00000000 result=80000000.00000000 errno=0

; No exception is raised with certain versions of glibc. Functions
; approximated by x near zero may not generate/implement flops and
; thus may not raise exceptions.
func=sinh op1=00000000.00000001 result=00000000.00000001 errno=0 maybestatus=ux
func=sinh op1=80000000.00000001 result=80000000.00000001 errno=0 maybestatus=ux
