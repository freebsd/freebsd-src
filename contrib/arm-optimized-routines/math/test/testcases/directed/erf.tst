; erf.tst - Directed test cases for erf
;
; Copyright (c) 2007-2020, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=erf op1=7ff80000.00000001 result=7ff80000.00000001 errno=0
func=erf op1=fff80000.00000001 result=7ff80000.00000001 errno=0
func=erf op1=7ff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=erf op1=fff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=erf op1=7ff00000.00000000 result=3ff00000.00000000 errno=0
func=erf op1=fff00000.00000000 result=bff00000.00000000 errno=0
func=erf op1=00000000.00000000 result=00000000.00000000 errno=ERANGE
func=erf op1=80000000.00000000 result=80000000.00000000 errno=ERANGE
func=erf op1=00000000.00000001 result=00000000.00000001 errno=0 status=ux
func=erf op1=80000000.00000001 result=80000000.00000001 errno=0 status=ux
func=erf op1=3ff00000.00000000 result=3feaf767.a741088a.c6d errno=0
func=erf op1=bff00000.00000000 result=bfeaf767.a741088a.c6d errno=0
