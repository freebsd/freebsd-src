; sinhf.tst
;
; Copyright (c) 2009-2024, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=sinhf op1=7fc00001 result=7fc00001 errno=0
func=sinhf op1=ffc00001 result=7fc00001 errno=0
func=sinhf op1=7f800001 result=7fc00001 errno=0 status=i
func=sinhf op1=ff800001 result=7fc00001 errno=0 status=i
func=sinhf op1=7f800000 result=7f800000 errno=0
func=sinhf op1=7f7fffff result=7f800000 errno=ERANGE status=ox
func=sinhf op1=ff800000 result=ff800000 errno=0
func=sinhf op1=ff7fffff result=ff800000 errno=ERANGE status=ox
func=sinhf op1=00000000 result=00000000 errno=0
func=sinhf op1=80000000 result=80000000 errno=0

; No exception is raised with certain versions of glibc. Functions
; approximated by x near zero may not generate/implement flops and
; thus may not raise exceptions.
func=sinhf op1=00000001 result=00000001 errno=0 maybestatus=ux
func=sinhf op1=80000001 result=80000001 errno=0 maybestatus=ux
