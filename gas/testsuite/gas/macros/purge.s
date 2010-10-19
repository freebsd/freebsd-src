 .data
 .macro MACRO1
 .endm
 .macro macro2
 .endm
	MACRO1
	MACRO2
	macro1
	macro2
 .purgem MACRO1
 .purgem macro2
	MACRO1
	MACRO2
	macro1
	macro2
 .purgem macro1
 .purgem MACRO2
 .macro macro1
 .endm
 .macro MACRO2
 .endm
	MACRO1
	MACRO2
	macro1
	macro2
 .purgem MACRO1
 .purgem macro2

 .irpc a,ABCDEFGHIJKLMNOPQRSTUVWXYZ
  .irpc b,ABCDEFGHIJKLMNOPQRSTUVWXYZ
   .irpc c,ABCDEFGHIJKLMNOPQRSTUVWXYZ
    .irpc d,ABCDEFGHIJKLMNOPQRSTUVWXYZ
     .macro _\a\b\c\d arg1=0, arg2=0
      .if \arg1 + \arg2
       .purgem _\a\b\c\d
      .endif
     .endm
	_\a\b\c\d 1, 2
    .endr
   .endr
  .endr
 .endr
