; erfc.tst - Directed test cases for erfc
;
; Copyright (c) 2022-2024, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=erfc op1=7ff80000.00000001 result=7ff80000.00000001 errno=0
func=erfc op1=fff80000.00000001 result=7ff80000.00000001 errno=0
func=erfc op1=7ff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=erfc op1=fff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=erfc op1=7ff00000.00000000 result=00000000.00000000 errno=0
func=erfc op1=7fefffff.ffffffff result=00000000.00000000 errno=ERANGE status=ux
; We deliberately turned off errno setting in erf, as standard simply
; state that errno `may` be set to ERANGE in case of underflow.
; As a result the following condition on errno cannot be satisfied.
;
; func=erfc op1=403b44af.48b01531 result=00000000.00000000 errno=ERANGE status=ux
;
func=erfc op1=c03b44af.48b01531 result=40000000.00000000 errno=0
func=erfc op1=403bffff.ffffffff result=00000000.00000000 errno=ERANGE status=ux
func=erfc op1=c03bffff.ffffffff result=40000000.00000000 errno=0
func=erfc op1=fff00000.00000000 result=40000000.00000000 errno=0
func=erfc op1=00000000.00000000 result=3ff00000.00000000 errno=0
func=erfc op1=80000000.00000000 result=3ff00000.00000000 errno=0
