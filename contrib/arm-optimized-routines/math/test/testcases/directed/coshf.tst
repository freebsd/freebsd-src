; coshf.tst
;
; Copyright (c) 2007-2024, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=coshf op1=7fc00001 result=7fc00001 errno=0
func=coshf op1=ffc00001 result=7fc00001 errno=0
func=coshf op1=7f800001 result=7fc00001 errno=0 status=i
func=coshf op1=ff800001 result=7fc00001 errno=0 status=i
func=coshf op1=7f800000 result=7f800000 errno=0
func=coshf op1=7f7fffff result=7f800000 errno=ERANGE status=ox
func=coshf op1=ff800000 result=7f800000 errno=0
func=coshf op1=ff7fffff result=7f800000 errno=ERANGE status=ox
func=coshf op1=00000000 result=3f800000 errno=0
func=coshf op1=80000000 result=3f800000 errno=0
