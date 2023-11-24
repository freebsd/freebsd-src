; expm1f.tst
;
; Copyright (c) 2009-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=expm1f op1=7fc00001 result=7fc00001 errno=0
func=expm1f op1=ffc00001 result=7fc00001 errno=0
func=expm1f op1=7f800001 result=7fc00001 errno=0 status=i
func=expm1f op1=ff800001 result=7fc00001 errno=0 status=i
func=expm1f op1=7f800000 result=7f800000 errno=0
func=expm1f op1=7f7fffff result=7f800000 errno=ERANGE status=ox
func=expm1f op1=ff800000 result=bf800000 errno=0
func=expm1f op1=ff7fffff result=bf800000 errno=0
func=expm1f op1=00000000 result=00000000 errno=0
func=expm1f op1=80000000 result=80000000 errno=0

; No exception is raised with certain versions of glibc. Functions
; approximated by x near zero may not generate/implement flops and
; thus may not raise exceptions.

func=expm1f op1=00000001 result=00000001 errno=0 maybestatus=ux
func=expm1f op1=80000001 result=80000001 errno=0 maybestatus=ux

func=expm1f op1=42b145c0 result=7f6ac2dd.9b8 errno=0

; Check both sides of the over/underflow thresholds in the code.
func=expm1f op1=c2000000 result=bf7fffff.fff error=0
func=expm1f op1=c2000001 result=bf7fffff.fff error=0
func=expm1f op1=43000000 result=7f800000 error=overflow
func=expm1f op1=43000001 result=7f800000 error=overflow
func=expm1f op1=c2a80000 result=bf800000.000 error=0
func=expm1f op1=c2a80001 result=bf800000.000 error=0

; Check values for which exp goes denormal. expm1f should not report
; spurious overflow.
func=expm1f op1=c2b00f34 result=bf800000.000 error=0
func=expm1f op1=c2ce8ed0 result=bf800000.000 error=0
func=expm1f op1=c2dc6bba result=bf800000.000 error=0

; Regression tests for significance loss when the two components of
; the result have opposite sign but similar magnitude
func=expm1f op1=be8516c1 result=be6a652b.0dc error=0
func=expm1f op1=be851714 result=be6a65ab.0e5 error=0
func=expm1f op1=be851cc7 result=be6a6e75.111 error=0
func=expm1f op1=be851d1a result=be6a6ef5.102 error=0
func=expm1f op1=be851d6d result=be6a6f75.0f2 error=0
func=expm1f op1=be852065 result=be6a7409.0e4 error=0
func=expm1f op1=be8520b8 result=be6a7489.0c7 error=0
func=expm1f op1=be85210b result=be6a7509.0a8 error=0
func=expm1f op1=be855401 result=be6ac39b.0d5 error=0
func=expm1f op1=be933307 result=be7fdbf0.d8d error=0
func=expm1f op1=be92ed6b result=be7f737a.d81 error=0
func=expm1f op1=be933b90 result=be7fe8be.d76 error=0
func=expm1f op1=3eb11364 result=3ed38deb.0c0 error=0
func=expm1f op1=3f28e830 result=3f6f344b.0da error=0
func=expm1f op1=3eb1578f result=3ed3ee47.13b error=0
func=expm1f op1=3f50176a result=3fa08e36.fea error=0
