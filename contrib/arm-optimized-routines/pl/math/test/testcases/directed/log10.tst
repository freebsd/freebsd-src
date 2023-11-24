; log10.tst
;
; Copyright (c) 2007-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=log10 op1=7ff80000.00000001 result=7ff80000.00000001 errno=0
func=log10 op1=fff80000.00000001 result=7ff80000.00000001 errno=0
func=log10 op1=7ff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=log10 op1=fff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=log10 op1=fff02000.00000000 result=7ff80000.00000001 errno=0 status=i
func=log10 op1=7ff00000.00000000 result=7ff00000.00000000 errno=0
func=log10 op1=3ff00000.00000000 result=00000000.00000000 errno=0
func=log10 op1=fff00000.00000000 result=7ff80000.00000001 errno=EDOM status=i
func=log10 op1=00000000.00000000 result=fff00000.00000000 errno=ERANGE status=z
func=log10 op1=80000000.00000000 result=fff00000.00000000 errno=ERANGE status=z
func=log10 op1=80000000.00000001 result=7ff80000.00000001 errno=EDOM status=i
