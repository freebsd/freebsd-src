#name: LDR group relocations failure test
#source: group-relocs-ldr-bad.s
#ld: -Ttext 0x8000 --section-start foo=0x8001000
#error: .*Overflow whilst splitting 0x8001000 for group relocation.*
