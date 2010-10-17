
Symbol table '.symtab' contains .* entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 00000000     0 SECTION LOCAL  DEFAULT    1 
     2: 00000000     0 SECTION LOCAL  DEFAULT    3 
     3: 00000000     0 SECTION LOCAL  DEFAULT    4 
     4: 00000000     0 NOTYPE  LOCAL  DEFAULT    1 static_text_symbol
# arm-elf targets add the $d mapping symbol here...
#...
     .: 00000000     0 NOTYPE  LOCAL  DEFAULT    3 static_data_symbol
# v850 targets include extra SECTION symbols here for the .call_table_data
# and .call_table_text sections.
#...
.*   .: 00000000     0 NOTYPE  GLOBAL DEFAULT    1 text_symbol
     .: 00000000     0 NOTYPE  GLOBAL DEFAULT  UND external_symbol
    ..: 00000000     0 NOTYPE  GLOBAL DEFAULT    3 data_symbol
    ..: 00000004     4 OBJECT  GLOBAL DEFAULT ( COM|ANSI_COM) common_symbol
