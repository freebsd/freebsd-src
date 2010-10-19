#source: textrel1.s
#source: textrel2.s
#as: -little
#ld: -shared -EL
#readelf: -d
#target: sh*-*-elf sh*-*-linux* sh*-*-netbsd*

# Make sure that there is no unnecessary DT_TEXTREL entry.

Dynamic section at offset 0x[0-9a-f]+ contains 9 entries:
  Tag        Type                         Name/Value
 0x00000004 \(HASH\)                       0x[0-9a-f]+
 0x00000005 \(STRTAB\)                     0x[0-9a-f]+
 0x00000006 \(SYMTAB\)                     0x[0-9a-f]+
 0x0000000a \(STRSZ\)                      [0-9]+ \(bytes\)
 0x0000000b \(SYMENT\)                     16 \(bytes\)
 0x00000007 \(RELA\)                       0x[0-9a-f]+
 0x00000008 \(RELASZ\)                     12 \(bytes\)
 0x00000009 \(RELAENT\)                    12 \(bytes\)
 0x00000000 \(NULL\)                       0x0
