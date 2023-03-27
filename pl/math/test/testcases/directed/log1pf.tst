; log1pf.tst
;
; Copyright (c) 2009-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=log1pf op1=7fc00001 result=7fc00001 errno=0
func=log1pf op1=ffc00001 result=7fc00001 errno=0
func=log1pf op1=7f800001 result=7fc00001 errno=0 status=i
func=log1pf op1=ff800001 result=7fc00001 errno=0 status=i
func=log1pf op1=ff810000 result=7fc00001 errno=0 status=i
func=log1pf op1=7f800000 result=7f800000 errno=0

; Cases 6, 9 , 10, 11, 12 fail with certain versions of GLIBC and not others.
; The main reason seems to be the handling of errno and exceptions.

func=log1pf op1=00000000 result=00000000 errno=0
func=log1pf op1=80000000 result=80000000 errno=0

; No exception is raised with certain versions of glibc. Functions
; approximated by x near zero may not generate/implement flops and
; thus may not raise exceptions.
func=log1pf op1=00000001 result=00000001 errno=0 maybestatus=ux
func=log1pf op1=80000001 result=80000001 errno=0 maybestatus=ux

func=log1pf op1=3f1e91ee result=3ef6d127.fdb errno=0
func=log1pf op1=3f201046 result=3ef8a881.fba errno=0
func=log1pf op1=3f21b916 result=3efab23b.f9f errno=0
func=log1pf op1=3f21bde6 result=3efab821.fee errno=0
func=log1pf op1=3f22a5ee result=3efbd435.ff2 errno=0
func=log1pf op1=3f231b56 result=3efc63b7.e26 errno=0
func=log1pf op1=3f23ce96 result=3efd3e83.fc8 errno=0
func=log1pf op1=3eee18c6 result=3ec38576.02e errno=0
func=log1pf op1=3eee2f41 result=3ec394ce.057 errno=0
func=log1pf op1=3eee770d result=3ec3c5cc.00c errno=0
func=log1pf op1=3eee7fed result=3ec3cbda.065 errno=0
func=log1pf op1=3eee8fb2 result=3ec3d69c.008 errno=0
func=log1pf op1=3eeeb8eb result=3ec3f2ba.061 errno=0
func=log1pf op1=3eeeccfd result=3ec4006a.01d errno=0
func=log1pf op1=3eeef5f0 result=3ec41c56.020 errno=0
func=log1pf op1=3eeeff12 result=3ec42290.00c errno=0
func=log1pf op1=3eef05cf result=3ec42728.052 errno=0
func=log1pf op1=3eef13d3 result=3ec430b6.00e errno=0
func=log1pf op1=3eef2e70 result=3ec442da.04a errno=0
func=log1pf op1=3eef3fbf result=3ec44ea6.055 errno=0
func=log1pf op1=3eef3feb result=3ec44ec4.021 errno=0
func=log1pf op1=3eef4399 result=3ec45146.011 errno=0
func=log1pf op1=3eef452e result=3ec4525a.049 errno=0
func=log1pf op1=3eef4ea9 result=3ec458d0.020 errno=0
func=log1pf op1=3eef7365 result=3ec471d8.05e errno=0
func=log1pf op1=3eefa38f result=3ec492a8.003 errno=0
func=log1pf op1=3eefb1f1 result=3ec49c74.015 errno=0
func=log1pf op1=3eefb334 result=3ec49d50.023 errno=0
func=log1pf op1=3eefb3c1 result=3ec49db0.0bf errno=0
func=log1pf op1=3eefb591 result=3ec49eec.15d errno=0
func=log1pf op1=3eefd736 result=3ec4b5d6.02d errno=0
func=log1pf op1=3eefd797 result=3ec4b618.114 errno=0
func=log1pf op1=3eefee5d result=3ec4c59a.071 errno=0
func=log1pf op1=3eeffff4 result=3ec4d194.0a7 errno=0
func=log1pf op1=3ef00cd1 result=3ec4da56.025 errno=0
func=log1pf op1=3ef0163a result=3ec4e0be.07a errno=0
func=log1pf op1=3ef01e89 result=3ec4e666.007 errno=0
func=log1pf op1=3ef02004 result=3ec4e768.00a errno=0
func=log1pf op1=3ef02c40 result=3ec4efbc.017 errno=0
func=log1pf op1=3ef05b50 result=3ec50fc4.031 errno=0
func=log1pf op1=3ef05bb1 result=3ec51006.05f errno=0
func=log1pf op1=3ef0651b result=3ec5166e.0d9 errno=0
func=log1pf op1=3ef06609 result=3ec51710.02a errno=0
func=log1pf op1=3ef0666a result=3ec51752.049 errno=0
func=log1pf op1=3ef0791e result=3ec5240c.0a8 errno=0
func=log1pf op1=3ef07d46 result=3ec526e0.00e errno=0
func=log1pf op1=3ef091fd result=3ec534f8.03c errno=0
func=log1pf op1=3ef09602 result=3ec537b4.128 errno=0
func=log1pf op1=3ef09848 result=3ec53940.044 errno=0
func=log1pf op1=3ef0a04f result=3ec53eb6.07d errno=0
func=log1pf op1=3ef0ab6a result=3ec54644.062 errno=0
func=log1pf op1=3ef0ae49 result=3ec54838.002 errno=0
func=log1pf op1=3ef0c1b8 result=3ec55570.000 errno=0
func=log1pf op1=3ef0ca06 result=3ec55b16.00d errno=0
func=log1pf op1=3ef0cc29 result=3ec55c8a.095 errno=0
func=log1pf op1=3ef0d228 result=3ec5609e.04f errno=0
func=log1pf op1=3ef0d8c0 result=3ec5651a.05e errno=0
func=log1pf op1=3ef0dc0c result=3ec56758.029 errno=0
func=log1pf op1=3ef0e0e8 result=3ec56aa6.02e errno=0
func=log1pf op1=3ef0e502 result=3ec56d70.102 errno=0
func=log1pf op1=3ef0e754 result=3ec56f04.017 errno=0
func=log1pf op1=3ef0efe9 result=3ec574da.01c errno=0
func=log1pf op1=3ef0f309 result=3ec576fa.016 errno=0
func=log1pf op1=3ef0f499 result=3ec5780a.005 errno=0
func=log1pf op1=3ef0f6c2 result=3ec57982.083 errno=0
func=log1pf op1=3ef0f852 result=3ec57a92.05d errno=0
func=log1pf op1=3ef0f9e2 result=3ec57ba2.02e errno=0
func=log1pf op1=3ef119ee result=3ec5916c.024 errno=0
func=log1pf op1=3ef11edf result=3ec594c8.03d errno=0
func=log1pf op1=3ef128c4 result=3ec59b82.001 errno=0
func=log1pf op1=3ef12ac1 result=3ec59cdc.04b errno=0
func=log1pf op1=3ef12fea result=3ec5a05e.045 errno=0
func=log1pf op1=3ef131e7 result=3ec5a1b8.05a errno=0
func=log1pf op1=3ef134e1 result=3ec5a3be.00e errno=0
func=log1pf op1=3ef1397a result=3ec5a6de.127 errno=0
func=log1pf op1=3ef13ade result=3ec5a7d0.0f6 errno=0
func=log1pf op1=3ef13c0d result=3ec5a89e.054 errno=0
func=log1pf op1=3ef13d71 result=3ec5a990.016 errno=0
func=log1pf op1=3ef14074 result=3ec5ab9c.12c errno=0
func=log1pf op1=3ef146a0 result=3ec5afce.035 errno=0
func=log1pf op1=3ef14a39 result=3ec5b240.024 errno=0
func=log1pf op1=3ef14d39 result=3ec5b44a.00c errno=0
func=log1pf op1=3ef152a3 result=3ec5b7f8.04d errno=0
func=log1pf op1=3ef170a1 result=3ec5cc5a.021 errno=0
func=log1pf op1=3ef17855 result=3ec5d196.0dc errno=0
func=log1pf op1=3ef17ece result=3ec5d5fc.010 errno=0
func=log1pf op1=3ef1810c result=3ec5d782.08e errno=0
func=log1pf op1=3ef18da9 result=3ec5e014.0ae errno=0
func=log1pf op1=3ef19054 result=3ec5e1e4.1a2 errno=0
func=log1pf op1=3ef190ea result=3ec5e24a.048 errno=0
func=log1pf op1=3ef1a739 result=3ec5f172.0d8 errno=0
func=log1pf op1=3ef1a83c result=3ec5f222.018 errno=0
func=log1pf op1=3ef1bbcc result=3ec5ff6c.09d errno=0
func=log1pf op1=3ef1bd3c result=3ec60066.03a errno=0
func=log1pf op1=3ef1d6ee result=3ec611da.056 errno=0
func=log1pf op1=3ef1de36 result=3ec616cc.01b errno=0
func=log1pf op1=3ef1e623 result=3ec61c2e.008 errno=0
func=log1pf op1=3ef1e9b1 result=3ec61e98.029 errno=0
func=log1pf op1=3ef1ee19 result=3ec62196.0d8 errno=0
func=log1pf op1=3ef1f13a result=3ec623b6.039 errno=0
func=log1pf op1=3ef1f1a7 result=3ec62400.091 errno=0
func=log1pf op1=3ef1f214 result=3ec6244a.0e8 errno=0
func=log1pf op1=3ef206e1 result=3ec6326a.09b errno=0
func=log1pf op1=3ef21245 result=3ec63a26.012 errno=0
func=log1pf op1=3ef217fd result=3ec63e08.048 errno=0
func=log1pf op1=3ef2186a result=3ec63e52.063 errno=0
