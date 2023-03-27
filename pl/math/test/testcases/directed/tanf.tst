; tanf.tst
;
; Copyright (c) 2022-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=tanf op1=7fc00001 result=7fc00001 errno=0
func=tanf op1=ffc00001 result=7fc00001 errno=0
func=tanf op1=7f800001 result=7fc00001 errno=0 status=i
func=tanf op1=ff800001 result=7fc00001 errno=0 status=i
func=tanf op1=7f800000 result=7fc00001 errno=EDOM status=i
func=tanf op1=ff800000 result=7fc00001 errno=EDOM status=i
func=tanf op1=00000000 result=00000000 errno=0
func=tanf op1=80000000 result=80000000 errno=0
; SDCOMP-26094: check tanf in the cases for which the range reducer
; returns values furthest beyond its nominal upper bound of pi/4.
func=tanf op1=46427f1b result=3f80396d.599 error=0
func=tanf op1=4647e568 result=3f8039a6.c9f error=0
func=tanf op1=46428bac result=3f803a03.148 error=0
func=tanf op1=4647f1f9 result=3f803a3c.852 error=0
func=tanf op1=4647fe8a result=3f803ad2.410 error=0
func=tanf op1=45d8d7f1 result=bf800669.901 error=0
func=tanf op1=45d371a4 result=bf800686.3cd error=0
func=tanf op1=45ce0b57 result=bf8006a2.e9a error=0
func=tanf op1=45d35882 result=bf80071b.bc4 error=0
func=tanf op1=45cdf235 result=bf800738.693 error=0
