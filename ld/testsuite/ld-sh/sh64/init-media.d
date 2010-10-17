#source: init.s
#as: --abi=32 --isa=SHmedia
#ld: -shared -mshelf32
#readelf: -d
#target: sh64-*-elf

# Make sure that the lsb of DT_INIT and DT_FINI entries is set
# when _init and _fini are SHmedia code.

Dynamic segment at offset 0x338 contains 8 entries:
  Tag        Type                         Name/Value
 0x0000000c \(INIT\)                       0x319
 0x0000000d \(FINI\)                       0x329
 0x00000004 \(HASH\)                       0x94
 0x00000005 \(STRTAB\)                     0x2ac
 0x00000006 \(SYMTAB\)                     0x13c
 0x0000000a \(STRSZ\)                      107 \(bytes\)
 0x0000000b \(SYMENT\)                     16 \(bytes\)
 0x00000000 \(NULL\)                       0x0
