; atan2f.tst
;
; Copyright (c) 1999-2024, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=atan2f op1=7f800001 op2=7f800001 result=7fc00001 errno=0 status=i
func=atan2f op1=7f800001 op2=ff800001 result=7fc00001 errno=0 status=i
func=atan2f op1=7f800001 op2=7fc00001 result=7fc00001 errno=0 status=i
func=atan2f op1=7f800001 op2=ffc00001 result=7fc00001 errno=0 status=i
func=atan2f op1=7f800001 op2=7f800000 result=7fc00001 errno=0 status=i
func=atan2f op1=7f800001 op2=ff800000 result=7fc00001 errno=0 status=i
func=atan2f op1=7f800001 op2=00000000 result=7fc00001 errno=0 status=i
func=atan2f op1=7f800001 op2=80000000 result=7fc00001 errno=0 status=i
func=atan2f op1=7f800001 op2=3f800000 result=7fc00001 errno=0 status=i
func=atan2f op1=7f800001 op2=bf800000 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800001 op2=7f800001 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800001 op2=ff800001 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800001 op2=7fc00001 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800001 op2=ffc00001 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800001 op2=7f800000 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800001 op2=ff800000 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800001 op2=00000000 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800001 op2=80000000 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800001 op2=3f800000 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800001 op2=bf800000 result=7fc00001 errno=0 status=i
func=atan2f op1=7fc00001 op2=7f800001 result=7fc00001 errno=0 status=i
func=atan2f op1=7fc00001 op2=ff800001 result=7fc00001 errno=0 status=i
func=atan2f op1=7fc00001 op2=7fc00001 result=7fc00001 errno=0
func=atan2f op1=7fc00001 op2=ffc00001 result=7fc00001 errno=0
func=atan2f op1=7fc00001 op2=7f800000 result=7fc00001 errno=0
func=atan2f op1=7fc00001 op2=ff800000 result=7fc00001 errno=0
func=atan2f op1=7fc00001 op2=00000000 result=7fc00001 errno=0
func=atan2f op1=7fc00001 op2=80000000 result=7fc00001 errno=0
func=atan2f op1=7fc00001 op2=3f800000 result=7fc00001 errno=0
func=atan2f op1=7fc00001 op2=bf800000 result=7fc00001 errno=0
func=atan2f op1=ffc00001 op2=7f800001 result=7fc00001 errno=0 status=i
func=atan2f op1=ffc00001 op2=ff800001 result=7fc00001 errno=0 status=i
func=atan2f op1=ffc00001 op2=7fc00001 result=ffc00001 errno=0
func=atan2f op1=ffc00001 op2=ffc00001 result=ffc00001 errno=0
func=atan2f op1=ffc00001 op2=7f800000 result=ffc00001 errno=0
func=atan2f op1=ffc00001 op2=ff800000 result=ffc00001 errno=0
func=atan2f op1=ffc00001 op2=00000000 result=ffc00001 errno=0
func=atan2f op1=ffc00001 op2=80000000 result=ffc00001 errno=0
func=atan2f op1=ffc00001 op2=3f800000 result=ffc00001 errno=0
func=atan2f op1=ffc00001 op2=bf800000 result=ffc00001 errno=0
func=atan2f op1=7f800000 op2=7f800001 result=7fc00001 errno=0 status=i
func=atan2f op1=7f800000 op2=ff800001 result=7fc00001 errno=0 status=i
func=atan2f op1=7f800000 op2=7fc00001 result=7fc00001 errno=0
func=atan2f op1=7f800000 op2=ffc00001 result=7fc00001 errno=0
func=atan2f op1=7f800000 op2=7f800000 result=3f490fda.a22 errno=0
func=atan2f op1=7f800000 op2=ff800000 result=4016cbe3.f99 errno=0
func=atan2f op1=7f800000 op2=00000000 result=3fc90fda.a22 errno=0
func=atan2f op1=7f800000 op2=80000000 result=3fc90fda.a22 errno=0
func=atan2f op1=7f800000 op2=3f800000 result=3fc90fda.a22 errno=0
func=atan2f op1=7f800000 op2=bf800000 result=3fc90fda.a22 errno=0
func=atan2f op1=ff800000 op2=7f800001 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800000 op2=ff800001 result=7fc00001 errno=0 status=i
func=atan2f op1=ff800000 op2=7fc00001 result=7fc00001 errno=0
func=atan2f op1=ff800000 op2=ffc00001 result=ffc00001 errno=0
func=atan2f op1=ff800000 op2=7f800000 result=bf490fda.a22 errno=0
func=atan2f op1=ff800000 op2=ff800000 result=c016cbe3.f99 errno=0
func=atan2f op1=ff800000 op2=00000000 result=bfc90fda.a22 errno=0
func=atan2f op1=ff800000 op2=80000000 result=bfc90fda.a22 errno=0
func=atan2f op1=ff800000 op2=3f800000 result=bfc90fda.a22 errno=0
func=atan2f op1=ff800000 op2=bf800000 result=bfc90fda.a22 errno=0
func=atan2f op1=00000000 op2=7f800001 result=7fc00001 errno=0 status=i
func=atan2f op1=00000000 op2=ff800001 result=7fc00001 errno=0 status=i
func=atan2f op1=00000000 op2=7fc00001 result=7fc00001 errno=0
func=atan2f op1=00000000 op2=ffc00001 result=ffc00001 errno=0
func=atan2f op1=00000000 op2=7f800000 result=00000000 errno=0
func=atan2f op1=00000000 op2=ff800000 result=40490fda.a22 errno=0
func=atan2f op1=00000000 op2=00000000 result=00000000 errno=0
func=atan2f op1=00000000 op2=80000000 result=40490fda.a22 errno=0
func=atan2f op1=00000000 op2=3f800000 result=00000000 errno=0
func=atan2f op1=00000000 op2=bf800000 result=40490fda.a22 errno=0
; No exception is raised on certain machines (different version of glibc)
; Same issue encountered with other function similar to x close to 0
; Could be due to function so boring no flop is involved in some implementations
func=atan2f op1=00000001 op2=3f800000 result=00000001 errno=0 maybestatus=ux

func=atan2f op1=80000000 op2=7f800001 result=7fc00001 errno=0 status=i
func=atan2f op1=80000000 op2=ff800001 result=7fc00001 errno=0 status=i
func=atan2f op1=80000000 op2=7fc00001 result=7fc00001 errno=0
func=atan2f op1=80000000 op2=ffc00001 result=ffc00001 errno=0
func=atan2f op1=80000000 op2=7f800000 result=80000000 errno=0
func=atan2f op1=80000000 op2=ff800000 result=c0490fda.a22 errno=0
func=atan2f op1=80000000 op2=00000000 result=80000000 errno=0
func=atan2f op1=80000000 op2=80000000 result=c0490fda.a22 errno=0
func=atan2f op1=80000000 op2=3f800000 result=80000000 errno=0
func=atan2f op1=80000000 op2=bf800000 result=c0490fda.a22 errno=0
; No exception is raised on certain machines (different version of glibc)
; Same issue encountered with other function similar to x close to 0
; Could be due to function so boring no flop is involved in some implementations
func=atan2f op1=80000001 op2=3f800000 result=80000001 errno=0 maybestatus=ux

func=atan2f op1=3f800000 op2=7f800001 result=7fc00001 errno=0 status=i
func=atan2f op1=3f800000 op2=ff800001 result=7fc00001 errno=0 status=i
func=atan2f op1=3f800000 op2=7fc00001 result=7fc00001 errno=0
func=atan2f op1=3f800000 op2=ffc00001 result=ffc00001 errno=0
func=atan2f op1=3f800000 op2=7f800000 result=00000000 errno=0
func=atan2f op1=3f800000 op2=ff800000 result=40490fda.a22 errno=0
func=atan2f op1=3f800000 op2=00000000 result=3fc90fda.a22 errno=0
func=atan2f op1=3f800000 op2=80000000 result=3fc90fda.a22 errno=0
func=atan2f op1=3f800000 op2=3f800000 result=3f490fda.a22 errno=0
func=atan2f op1=3f800000 op2=bf800000 result=4016cbe3.f99 errno=0
func=atan2f op1=bf800000 op2=7f800001 result=7fc00001 errno=0 status=i
func=atan2f op1=bf800000 op2=ff800001 result=7fc00001 errno=0 status=i
func=atan2f op1=bf800000 op2=7fc00001 result=7fc00001 errno=0
func=atan2f op1=bf800000 op2=ffc00001 result=ffc00001 errno=0
func=atan2f op1=bf800000 op2=7f800000 result=80000000 errno=0
func=atan2f op1=bf800000 op2=ff800000 result=c0490fda.a22 errno=0
func=atan2f op1=bf800000 op2=00000000 result=bfc90fda.a22 errno=0
func=atan2f op1=bf800000 op2=80000000 result=bfc90fda.a22 errno=0
func=atan2f op1=bf800000 op2=3f800000 result=bf490fda.a22 errno=0
func=atan2f op1=bf800000 op2=bf800000 result=c016cbe3.f99 errno=0
func=atan2f op1=8005f16d op2=002bb601 result=be0a60a5.d88 error=0
func=atan2f op1=80818ec8 op2=80ba5db9 result=c0222eda.f42 error=0

func=atan2f op1=ff7fffff op2=ff7fffff result=c016cbe3.f99 errno=0
func=atan2f op1=bfc00001 op2=7f7fffff result=80300000.700 errno=0 status=u
func=atan2f op1=80800001 op2=40000000 result=80400000.800 errno=0 status=u
