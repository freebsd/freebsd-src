* Just PUSHJs to an external symbol, with some padding.
 .text
 .global pushja
pushja:
 SET $253,2
 PUSHJ $12,a
 SET $253,3
