#source: size2a.s
#source: size2b.s
#ld:
#readelf: -s
Symbol table '.symtab' contains .* entries:
#...
    .*: [0-9a-f]* +1 +FUNC +GLOBAL +DEFAULT +[0-9] +__?start
#...
    .*: [0-9a-f]* +1 +FUNC +WEAK +DEFAULT +[0-9] +foo
#...
