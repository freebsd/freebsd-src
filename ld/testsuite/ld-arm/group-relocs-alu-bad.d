#name: ALU group relocations failure test
#source: group-relocs-alu-bad.s
#ld: -Ttext 0x8000 --section-start foo=0x9010
#error: Overflow whilst splitting 0x1010 for group relocation
