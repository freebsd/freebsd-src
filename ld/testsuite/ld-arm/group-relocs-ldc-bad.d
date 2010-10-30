#name: LDC group relocations failure test
#source: group-relocs-ldc-bad.s
#ld: -Ttext 0x8000 --section-start foo=0x118400
#error: Overflow whilst splitting 0x110400 for group relocation
