#source: init.s
#as: --abi=64
#ld: -shared -mshelf64
#readelf: -d
#target: sh64-*-elf

# Make sure that the lsb of DT_INIT and DT_FINI entries is set
# when _init and _fini are SHmedia code.

.*
  Tag        Type                         Name/Value
 0x000000000000000c \(INIT\) .*[13579bdf]
 0x000000000000000d \(FINI\) .*[13579bdf]
#pass
