#source: merge-error-1a.s -march=isaa -mmac
#source: merge-error-1b.s -march=isaa -memac
#ld: -r
#error: ^[^\n]* m68k:isa-a:emac [^\n]* incompatible with m68k:isa-a:mac [^\n]*$
