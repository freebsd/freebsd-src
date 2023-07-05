; asinh.tst
;
; Copyright (c) 2022-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=asinh op1=7ff80000.00000001 result=7ff80000.00000001 errno=0
func=asinh op1=fff80000.00000001 result=7ff80000.00000001 errno=0
func=asinh op1=7ff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=asinh op1=fff00000.00000001 result=7ff80000.00000001 errno=0 status=i
func=asinh op1=7ff00000.00000000 result=7ff00000.00000000 errno=0
func=asinh op1=fff00000.00000000 result=fff00000.00000000 errno=0
func=asinh op1=00000000.00000000 result=00000000.00000000 errno=0
func=asinh op1=80000000.00000000 result=80000000.00000000 errno=0
; No exception is raised with certain versions of glibc. Functions
; approximated by x near zero may not generate/implement flops and
; thus may not raise exceptions.
func=asinh op1=00000000.00000001 result=00000000.00000001 errno=0 maybestatus=ux
func=asinh op1=80000000.00000001 result=80000000.00000001 errno=0 maybestatus=ux
