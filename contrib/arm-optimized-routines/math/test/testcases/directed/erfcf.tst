; erfcf.tst - Directed test cases for erfcf
;
; Copyright (c) 2007-2024, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=erfcf op1=7fc00001 result=7fc00001 errno=0
func=erfcf op1=ffc00001 result=7fc00001 errno=0
func=erfcf op1=7f800001 result=7fc00001 errno=0 status=i
func=erfcf op1=ff800001 result=7fc00001 errno=0 status=i
func=erfcf op1=7f800000 result=00000000 errno=0
func=erfcf op1=7f7fffff result=00000000 errno=ERANGE status=ux
func=erfcf op1=ff800000 result=40000000 errno=0
func=erfcf op1=00000000 result=3f800000 errno=0
func=erfcf op1=80000000 result=3f800000 errno=0
