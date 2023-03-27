; cbrtf.tst
;
; Copyright (c) 2009-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=cbrtf op1=7f800000 result=7f800000 errno=0
func=cbrtf op1=ff800000 result=ff800000 errno=0
func=cbrtf op1=7f800001 result=7fc00001 errno=0 status=i
func=cbrtf op1=7fc00001 result=7fc00001 errno=0
func=cbrtf op1=00000000 result=00000000 errno=0
func=cbrtf op1=00000001 result=26a14517.cc7 errno=0
func=cbrtf op1=00000002 result=26cb2ff5.29f errno=0
func=cbrtf op1=00000003 result=26e89768.579 errno=0
func=cbrtf op1=00000004 result=27000000.000 errno=0
func=cbrtf op1=00400000 result=2a4b2ff5.29f errno=0
func=cbrtf op1=00800000 result=2a800000.000 errno=0
func=cbrtf op1=3f800000 result=3f800000.000 errno=0
func=cbrtf op1=40000000 result=3fa14517.cc7 errno=0
func=cbrtf op1=7f7fffff result=54cb2ff4.e63 errno=0
func=cbrtf op1=80000000 result=80000000 errno=0
func=cbrtf op1=80000001 result=a6a14517.cc7 errno=0
func=cbrtf op1=80000002 result=a6cb2ff5.29f errno=0
func=cbrtf op1=80000003 result=a6e89768.579 errno=0
func=cbrtf op1=80000004 result=a7000000.000 errno=0
func=cbrtf op1=80400000 result=aa4b2ff5.29f errno=0
func=cbrtf op1=80800000 result=aa800000.000 errno=0
func=cbrtf op1=bf800000 result=bf800000.000 errno=0
func=cbrtf op1=c0000000 result=bfa14517.cc7 errno=0
func=cbrtf op1=ff7fffff result=d4cb2ff4.e63 errno=0
