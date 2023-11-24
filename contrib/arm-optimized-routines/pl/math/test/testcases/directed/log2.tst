; Directed test cases for log2
;
; Copyright (c) 2018-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=log2 op1=7ff80000.00000001 result=7ff80000.00000001 errno=0
func=log2 op1=fff80000.00000001 result=7ff80000.00000001 errno=0
func=log2 op1=7ff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=log2 op1=fff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=log2 op1=7ff00000.00000000 result=7ff00000.00000000 errno=0
func=log2 op1=fff00000.00000000 result=7ff80000.00000001 errno=EDOM status=i
func=log2 op1=7fefffff.ffffffff result=408fffff.ffffffff.ffa errno=0
func=log2 op1=ffefffff.ffffffff result=7ff80000.00000001 errno=EDOM status=i
func=log2 op1=3ff00000.00000000 result=00000000.00000000 errno=0
func=log2 op1=bff00000.00000000 result=7ff80000.00000001 errno=EDOM status=i
func=log2 op1=00000000.00000000 result=fff00000.00000000 errno=ERANGE status=z
func=log2 op1=80000000.00000000 result=fff00000.00000000 errno=ERANGE status=z
func=log2 op1=00000000.00000001 result=c090c800.00000000 errno=0
func=log2 op1=80000000.00000001 result=7ff80000.00000001 errno=EDOM status=i
func=log2 op1=40000000.00000000 result=3ff00000.00000000 errno=0
func=log2 op1=3fe00000.00000000 result=bff00000.00000000 errno=0
