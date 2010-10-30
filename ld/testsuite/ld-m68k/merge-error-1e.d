#source: merge-error-1a.s -march=isaa -mno-div -mmac
#source: merge-error-1b.s -march=isaa -mno-div -memac
#ld: -r
#error: ^[^\n]* m68k:isa-a:emac [^\n]* is incompatible with m68k:isa-a:mac [^\n]*$
