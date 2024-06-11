; asinf.tst
;
; Copyright (c) 2009-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=asinf op1=7fc00001 result=7fc00001 errno=0
func=asinf op1=ffc00001 result=7fc00001 errno=0
func=asinf op1=7f800001 result=7fc00001 errno=0 status=i
func=asinf op1=ff800001 result=7fc00001 errno=0 status=i
func=asinf op1=7f800000 result=7fc00001 errno=EDOM status=i
func=asinf op1=ff800000 result=7fc00001 errno=EDOM status=i
func=asinf op1=00000000 result=00000000 errno=0
func=asinf op1=80000000 result=80000000 errno=0
; Inconsistent behavior was detected for the following 2 cases.
; No exception is raised with certain versions of glibc. Functions
; approximated by x near zero may not generate/implement flops and
; thus may not raise exceptions.
func=asinf op1=00000001 result=00000001 errno=0 maybestatus=ux
func=asinf op1=80000001 result=80000001 errno=0 maybestatus=ux

func=asinf op1=3f800000 result=3fc90fda.a22 errno=0
func=asinf op1=bf800000 result=bfc90fda.a22 errno=0
func=asinf op1=3f800001 result=7fc00001 errno=EDOM status=i
func=asinf op1=bf800001 result=7fc00001 errno=EDOM status=i
