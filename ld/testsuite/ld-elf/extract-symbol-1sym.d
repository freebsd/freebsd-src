#name: --extract-symbol test 1 (symbols)
#source: extract-symbol-1.s
#ld: -Textract-symbol-1.ld
#objcopy_linked_file: --extract-symbol
#nm: -n
#xfail: "hppa*-*-*"
0*00010010 T B
0*00020123 T C
0*00030000 A _entry
0*00040000 A linker_symbol
0*12345678 A D
