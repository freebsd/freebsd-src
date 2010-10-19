 .equiv two, 2*one
 .equiv minus_one, -one
 .equ one, 1
 .equiv three, 3*one
 .eqv four, 4*one

 .data

 .if two > one
  .byte one
  .byte two
 .endif
	
 .if four > one
  .byte three
  .byte four
 .endif

 .equ one, -1
 .byte one
 .byte two
	
 .if four < one
  .byte three
  .byte four
 .endif

 .equ one, -minus_one
 .byte one
 .byte two
	
 .if four > one
  .byte three
  .byte four
 .endif

 .equ one, minus_one
 .byte one
 .byte two

 .if four < one
  .byte three
  .byte four
 .endif
