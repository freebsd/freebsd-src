.equ zero, 0
.equ one, 1
.equ two, 2


 .data

 .if two > one
   .byte one
 .else
   .byte two
 .endif

 .if one == one
   .byte one
  .else
    .byte two
  .endif

  .if one < two
    .byte one
  .else
    .byte two
  .endif

  .if one <> two
    .byte one
  .else
    .byte two
  .endif

  .if one != two
    .byte one
  .else
    .byte two
  .endif

  .if one <= two
    .byte one
  .else
    .byte two
  .endif

  .if two >= one
    .byte one
  .else
    .byte two
  .endif
