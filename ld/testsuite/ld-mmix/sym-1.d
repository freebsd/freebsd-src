#source: start.s
#source: a.s
#ld: -m mmo
#nm: -n

# Test that nm can grok a simple mmo symbol table (or that mmo lets nm
# grok it).

0+ T Main
0+ T _start
0+4 T a
