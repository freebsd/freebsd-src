; acos.tst
;
; Copyright (c) 2009-2024, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=acos op1=7ff80000.00000001 result=7ff80000.00000001 errno=0
func=acos op1=fff80000.00000001 result=7ff80000.00000001 errno=0
func=acos op1=7ff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=acos op1=fff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=acos op1=7ff00000.00000000 result=7ff80000.00000001 errno=EDOM status=i
func=acos op1=fff00000.00000000 result=7ff80000.00000001 errno=EDOM status=i
func=acos op1=00000000.00000000 result=3ff921fb.54442d18.469 errno=0
func=acos op1=80000000.00000000 result=3ff921fb.54442d18.469 errno=0
func=acos op1=3ff00000.00000000 result=00000000.00000000 errno=0
func=acos op1=bff00000.00000000 result=400921fb.54442d18.469 errno=0
func=acos op1=3ff00000.00000001 result=7ff80000.00000001 errno=EDOM status=i
func=acos op1=bff00000.00000001 result=7ff80000.00000001 errno=EDOM status=i
