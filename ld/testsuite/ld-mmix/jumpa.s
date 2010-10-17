* Just jump to an external symbol, with some padding.
 .text
 .global jumpa
jumpa:
 SET $253,2
 JMP a
 SET $253,3
