; log10f.tst
;
; Copyright (c) 2007-2023, Arm Limited.
; SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

func=log10f op1=7fc00001 result=7fc00001 errno=0
func=log10f op1=ffc00001 result=7fc00001 errno=0
func=log10f op1=7f800001 result=7fc00001 errno=0 status=i
func=log10f op1=ff800001 result=7fc00001 errno=0 status=i
func=log10f op1=ff810000 result=7fc00001 errno=0 status=i
func=log10f op1=7f800000 result=7f800000 errno=0
func=log10f op1=3f800000 result=00000000 errno=0
func=log10f op1=ff800000 result=7fc00001 errno=EDOM status=i
func=log10f op1=00000000 result=ff800000 errno=ERANGE status=z
func=log10f op1=80000000 result=ff800000 errno=ERANGE status=z
func=log10f op1=80000001 result=7fc00001 errno=EDOM status=i

; Directed tests for the special-case handling of log10 of things
; very near 1
func=log10f op1=3f81a618 result=3bb62472.b92 error=0
func=log10f op1=3f876783 result=3cc811f4.26c error=0
func=log10f op1=3f816af8 result=3b9cc4c7.057 error=0
func=log10f op1=3f7bed7d result=bbe432cb.e23 error=0
func=log10f op1=3f803ece result=3a59ff3a.a84 error=0
func=log10f op1=3f80089f result=38ef9728.aa6 error=0
func=log10f op1=3f86ab72 result=3cb4b711.457 error=0
func=log10f op1=3f780854 result=bc60f953.904 error=0
func=log10f op1=3f7c6d76 result=bbc7fd01.01c error=0
func=log10f op1=3f85dff6 result=3c9fa76f.81f error=0
func=log10f op1=3f7b87f4 result=bbfa9edc.be4 error=0
func=log10f op1=3f81c710 result=3bc4457b.745 error=0
func=log10f op1=3f80946d result=3b00a140.c06 error=0
func=log10f op1=3f7e87ea result=bb23cd70.828 error=0
func=log10f op1=3f811437 result=3b6ee960.b40 error=0
func=log10f op1=3f858dcf result=3c971d9b.2ea error=0
func=log10f op1=3f7f61a3 result=ba89b814.4e0 error=0
func=log10f op1=3f82d642 result=3c1bfb8d.517 error=0
func=log10f op1=3f80f3bc result=3b52ebe8.c75 error=0
func=log10f op1=3f85eff9 result=3ca150d9.7e8 error=0
func=log10f op1=3f843eb8 result=3c68263f.771 error=0
func=log10f op1=3f78e691 result=bc481cf4.50a error=0
func=log10f op1=3f87c56f result=3cd1b268.5e6 error=0
func=log10f op1=3f83b711 result=3c4b94c5.918 error=0
func=log10f op1=3f823b2b result=3bf5eb02.e2a error=0
func=log10f op1=3f7f2c4e result=bab82c80.519 error=0
func=log10f op1=3f83fc92 result=3c5a3ba1.543 error=0
func=log10f op1=3f793956 result=bc3ee04e.03c error=0
func=log10f op1=3f839ba5 result=3c45caca.92a error=0
func=log10f op1=3f862f30 result=3ca7de76.16f error=0
func=log10f op1=3f832a20 result=3c2dc6e9.afd error=0
func=log10f op1=3f810296 result=3b5fb92a.429 error=0
func=log10f op1=3f7e58c9 result=bb38655a.0a4 error=0
func=log10f op1=3f8362e7 result=3c39cc65.d15 error=0
func=log10f op1=3f7fdb85 result=b97d9016.40b error=0
func=log10f op1=3f84484e result=3c6a29f2.f74 error=0
func=log10f op1=3f861862 result=3ca5819e.f2d error=0
func=log10f op1=3f7c027b result=bbdf912d.440 error=0
func=log10f op1=3f867803 result=3caf6744.34d error=0
func=log10f op1=3f789a89 result=bc509bce.458 error=0
func=log10f op1=3f8361d9 result=3c399347.379 error=0
func=log10f op1=3f7d3ac3 result=bb9ad93a.93d error=0
func=log10f op1=3f7ee241 result=baf8bd12.a62 error=0
func=log10f op1=3f83a1fd result=3c4721bd.0a4 error=0
func=log10f op1=3f840da3 result=3c5dd375.675 error=0
func=log10f op1=3f79c2fe result=bc2f8a60.8c5 error=0
func=log10f op1=3f854a93 result=3c901cc9.add error=0
func=log10f op1=3f87a50a result=3cce6125.cd6 error=0
func=log10f op1=3f818bf5 result=3baaee68.a55 error=0
func=log10f op1=3f830a44 result=3c2705c4.d87 error=0
