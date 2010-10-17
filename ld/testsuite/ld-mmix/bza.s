* Just BEQs to an external symbol, with some padding.
 .text
 .global bza
bza:
 SET $253,2
 BZ $234,a
 SET $253,3
