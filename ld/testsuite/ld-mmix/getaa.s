* Just geta an external symbol, with some padding.
 .text
 .global getaa
getaa:
 SET $253,2
 GETA $123,a
 SET $253,3
