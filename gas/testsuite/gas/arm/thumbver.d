# as: -meabi=4
# readelf: -s
# This test is only valid on ELF based ports.
#not-target: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*

Symbol table '\.symtab' contains .* entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 00000000     0 SECTION LOCAL  DEFAULT    1 
#...
     .*: 00000001     0 FUNC    LOCAL  DEFAULT    1 a_alias
     .*: 00000001     0 FUNC    LOCAL  DEFAULT    1 a_body
     .*: 00000000     0 NOTYPE  LOCAL  DEFAULT    1 \$t
     .*: 00000001     0 FUNC    LOCAL  DEFAULT    1 a_export@VERSION
#...
