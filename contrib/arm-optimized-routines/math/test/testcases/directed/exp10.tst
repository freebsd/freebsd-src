; Directed test cases for exp10
;
; Copyright (c) 2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=exp10 op1=7ff80000.00000001 result=7ff80000.00000001 errno=0
func=exp10 op1=fff80000.00000001 result=7ff80000.00000001 errno=0
func=exp10 op1=7ff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=exp10 op1=fff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=exp10 op1=7ff00000.00000000 result=7ff00000.00000000 errno=0
func=exp10 op1=7fefffff.ffffffff result=7ff00000.00000000 errno=ERANGE status=ox
func=exp10 op1=fff00000.00000000 result=00000000.00000000 errno=0
func=exp10 op1=ffefffff.ffffffff result=00000000.00000000 errno=ERANGE status=ux
func=exp10 op1=00000000.00000000 result=3ff00000.00000000 errno=0
func=exp10 op1=80000000.00000000 result=3ff00000.00000000 errno=0
