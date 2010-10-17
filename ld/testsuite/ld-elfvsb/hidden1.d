#source: undef.s
#ld: -r
#readelf: -s

Symbol table '.symtab' contains .* entries:
   Num:    Value[ 	]+Size Type    Bind   Vis      Ndx Name
#...
[ 	]*[0-9]+: [0-9a-fA-F]*     0 NOTYPE  GLOBAL HIDDEN  UND hidden
#pass
