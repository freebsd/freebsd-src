; tanhf.tst
;
; Copyright (c) 2007-2024, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=tanhf op1=7fc00001 result=7fc00001 errno=0
func=tanhf op1=ffc00001 result=7fc00001 errno=0
func=tanhf op1=7f800001 result=7fc00001 errno=0 status=i
func=tanhf op1=ff800001 result=7fc00001 errno=0 status=i
func=tanhf op1=7f800000 result=3f800000 errno=0
func=tanhf op1=ff800000 result=bf800000 errno=0
func=tanhf op1=00000000 result=00000000 errno=0
func=tanhf op1=80000000 result=80000000 errno=0
; No exception is raised with certain versions of glibc. Functions
; approximated by x near zero may not generate/implement flops and
; thus may not raise exceptions.
; func=tanhf op1=00000001 result=00000001 errno=0 maybestatus=ux
; func=tanhf op1=80000001 result=80000001 errno=0 maybestatus=ux
func=tanhf op1=00000001 result=00000001 errno=0 maybestatus=ux
func=tanhf op1=80000001 result=80000001 errno=0 maybestatus=ux
