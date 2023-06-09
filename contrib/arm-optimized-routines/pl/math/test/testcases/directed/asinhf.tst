; asinhf.tst
;
; Copyright (c) 2007-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=asinhf op1=7fc00001 result=7fc00001 errno=0
func=asinhf op1=ffc00001 result=7fc00001 errno=0
func=asinhf op1=7f800001 result=7fc00001 errno=0 status=i
func=asinhf op1=ff800001 result=7fc00001 errno=0 status=i
func=asinhf op1=7f800000 result=7f800000 errno=0
func=asinhf op1=ff800000 result=ff800000 errno=0
func=asinhf op1=00000000 result=00000000 errno=0
func=asinhf op1=80000000 result=80000000 errno=0
; No exception is raised on certain machines (different version of glibc)
; Same issue encountered with other function similar to x close to 0
; Could be due to function so boring no flop is involved in some implementations
func=asinhf op1=00000001 result=00000001 errno=0 maybestatus=ux
func=asinhf op1=80000001 result=80000001 errno=0 maybestatus=ux
