#source: merge-error-1a.s -mcpu=cpu32
#source: merge-error-1b.s -mcpu=68000
#ld: -r
#error: ^[^\n]* m68k:68000 [^\n]* incompatible with m68k:cpu32 [^\n]*$
