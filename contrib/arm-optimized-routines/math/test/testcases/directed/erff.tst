; erff.tst
;
; Copyright (c) 2007-2020, Arm Limited.
; SPDX-License-Identifier: MIT

func=erff op1=7fc00001 result=7fc00001 errno=0
func=erff op1=ffc00001 result=7fc00001 errno=0
func=erff op1=7f800001 result=7fc00001 errno=0 status=i
func=erff op1=ff800001 result=7fc00001 errno=0 status=i
func=erff op1=7f800000 result=3f800000 errno=0
func=erff op1=ff800000 result=bf800000 errno=0
func=erff op1=00000000 result=00000000 errno=ERANGE
func=erff op1=80000000 result=80000000 errno=ERANGE
func=erff op1=00000001 result=00000001 errno=0 status=ux
func=erff op1=80000001 result=80000001 errno=0 status=ux
func=erff op1=3f800000 result=3f57bb3d.3a0 errno=0
func=erff op1=bf800000 result=bf57bb3d.3a0 errno=0
