#source: merge-error-1a.s -march=isaaplus
#source: merge-error-1b.s -march=isab
#ld: -r
#error: ^[^\n]* m68k:isa-b [^\n]* incompatible with m68k:isa-aplus [^\n]*$
