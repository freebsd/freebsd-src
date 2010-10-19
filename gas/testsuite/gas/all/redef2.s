 .data
here:
 .set sym, here
 .long sym
 .set sym, 0x11111111
 .long sym
 .set sym, xtrn
 .long sym
 .set sym, 0x22222222
 .long sym
sym:
 .long sym
