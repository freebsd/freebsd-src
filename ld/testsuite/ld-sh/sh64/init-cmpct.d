#source: init.s
#as: --abi=32 --isa=SHcompact
#ld: -shared -mshelf32
#readelf: -d
#target: sh64-*-elf

# Make sure that the lsb of DT_INIT and DT_FINI entries is not set
# when _init and _fini are SHcompact code.

Dynamic section at offset .* contains 8 entries:
  Tag        Type                         Name/Value
 0x0000000c \(INIT\) .*[02468ace]
 0x0000000d \(FINI\) .*[02468ace]
#pass
