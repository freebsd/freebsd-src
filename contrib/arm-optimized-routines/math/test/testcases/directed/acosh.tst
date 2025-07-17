; acosh.tst
;
; Copyright (c) 2009-2024, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=acosh op1=7ff80000.00000001 result=7ff80000.00000001 errno=0
func=acosh op1=fff80000.00000001 result=7ff80000.00000001 errno=0
func=acosh op1=7ff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=acosh op1=fff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=acosh op1=7ff00000.00000000 result=7ff00000.00000000 errno=0
func=acosh op1=3ff00000.00000000 result=00000000.00000000 errno=0
func=acosh op1=3fefffff.ffffffff result=7ff80000.00000001 errno=EDOM status=i
func=acosh op1=00000000.00000000 result=7ff80000.00000001 errno=EDOM status=i
func=acosh op1=80000000.00000000 result=7ff80000.00000001 errno=EDOM status=i
func=acosh op1=bfefffff.ffffffff result=7ff80000.00000001 errno=EDOM status=i
func=acosh op1=bff00000.00000000 result=7ff80000.00000001 errno=EDOM status=i
func=acosh op1=bff00000.00000001 result=7ff80000.00000001 errno=EDOM status=i
func=acosh op1=fff00000.00000000 result=7ff80000.00000001 errno=EDOM status=i
func=acosh op1=7fe01ac0.7f03a83e result=40862e50.541778f1.8cc error=0
