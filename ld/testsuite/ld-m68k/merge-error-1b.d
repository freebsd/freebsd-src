#source: merge-error-1a.s -mcpu=cpu32
#source: merge-error-1b.s -mcpu=5207
#ld: -r
#error: ^[^\n]* m68k:isa-aplus:emac [^\n]* incompatible with m68k:cpu32 [^\n]*$
