; acoshf.tst
;
; Copyright (c) 2009-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=acoshf op1=7fc00001 result=7fc00001 errno=0
func=acoshf op1=ffc00001 result=7fc00001 errno=0
func=acoshf op1=7f800001 result=7fc00001 errno=0 status=i
func=acoshf op1=ff800001 result=7fc00001 errno=0 status=i
func=acoshf op1=7f800000 result=7f800000 errno=0
func=acoshf op1=3f800000 result=00000000 errno=0
func=acoshf op1=3f7fffff result=7fc00001 errno=EDOM status=i
func=acoshf op1=00000000 result=7fc00001 errno=EDOM status=i
func=acoshf op1=80000000 result=7fc00001 errno=EDOM status=i
func=acoshf op1=bf7fffff result=7fc00001 errno=EDOM status=i
func=acoshf op1=bf800000 result=7fc00001 errno=EDOM status=i
func=acoshf op1=bf800001 result=7fc00001 errno=EDOM status=i
func=acoshf op1=ff800000 result=7fc00001 errno=EDOM status=i
func=acoshf op1=7f767efe result=42b2c19d.83e error=0
