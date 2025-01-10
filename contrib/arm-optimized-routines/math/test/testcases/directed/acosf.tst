; acosf.tst
;
; Copyright (c) 2009-2024, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=acosf op1=7fc00001 result=7fc00001 errno=0
func=acosf op1=ffc00001 result=7fc00001 errno=0
func=acosf op1=7f800001 result=7fc00001 errno=0 status=i
func=acosf op1=ff800001 result=7fc00001 errno=0 status=i
func=acosf op1=7f800000 result=7fc00001 errno=EDOM status=i
func=acosf op1=ff800000 result=7fc00001 errno=EDOM status=i
func=acosf op1=00000000 result=3fc90fda.a22 errno=0
func=acosf op1=80000000 result=3fc90fda.a22 errno=0
func=acosf op1=3f800000 result=00000000 errno=0
func=acosf op1=bf800000 result=40490fda.a22 errno=0
func=acosf op1=3f800001 result=7fc00001 errno=EDOM status=i
func=acosf op1=bf800001 result=7fc00001 errno=EDOM status=i
func=acosf op1=33000000 result=3fc90fda.622 error=0
func=acosf op1=30000000 result=3fc90fda.a12 error=0
func=acosf op1=2d000000 result=3fc90fda.a21 error=0
func=acosf op1=2a000000 result=3fc90fda.a22 error=0
