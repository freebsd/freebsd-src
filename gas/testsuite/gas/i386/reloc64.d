#objdump: -Drw
#name: x86-64 relocs

.*: +file format .*x86-64.*

Disassembly of section \.text:
#...
.*[ 	]+R_X86_64_64[ 	]+xtrn
.*[ 	]+R_X86_64_32S[ 	]+xtrn
.*[ 	]+R_X86_64_32[ 	]+xtrn
.*[ 	]+R_X86_64_16[ 	]+xtrn
.*[ 	]+R_X86_64_8[ 	]+xtrn
.*[ 	]+R_X86_64_32S[ 	]+xtrn
.*[ 	]+R_X86_64_32[ 	]+xtrn
.*[ 	]+R_X86_64_PC64[ 	]+xtrn\+0x0*2
.*[ 	]+R_X86_64_PC32[ 	]+xtrn\+0x0*2
.*[ 	]+R_X86_64_PC16[ 	]+xtrn\+0x0*2
.*[ 	]+R_X86_64_PC8[ 	]+xtrn\+0x0*1
.*[ 	]+R_X86_64_PC32[ 	]+xtrn\+0xf+c
.*[ 	]+R_X86_64_PC32[ 	]+xtrn\+0xf+c
.*[ 	]+R_X86_64_PC8[ 	]+xtrn\+0xf+f
.*[ 	]+R_X86_64_GOT64[ 	]+xtrn
.*[ 	]+R_X86_64_GOT32[ 	]+xtrn
.*[ 	]+R_X86_64_GOT32[ 	]+xtrn
.*[ 	]+R_X86_64_GOTOFF64[ 	]+xtrn
.*[ 	]+R_X86_64_GOTPCREL[ 	]+xtrn
.*[ 	]+R_X86_64_GOTPCREL[ 	]+xtrn
.*[ 	]+R_X86_64_GOTPCREL[ 	]+xtrn\+0xf+c
.*[ 	]+R_X86_64_GOTPC32[ 	]+_GLOBAL_OFFSET_TABLE_\+0x0*2
.*[ 	]+R_X86_64_GOTPC32[ 	]+_GLOBAL_OFFSET_TABLE_\+0xf+c
.*[ 	]+R_X86_64_GOTPC32[ 	]+_GLOBAL_OFFSET_TABLE_\+0x0*2
.*[ 	]+R_X86_64_PLT32[ 	]+xtrn
.*[ 	]+R_X86_64_PLT32[ 	]+xtrn
.*[ 	]+R_X86_64_PLT32[ 	]+xtrn\+0xf+c
.*[ 	]+R_X86_64_TLSGD[ 	]+xtrn
.*[ 	]+R_X86_64_TLSGD[ 	]+xtrn
.*[ 	]+R_X86_64_TLSGD[ 	]+xtrn\+0xf+c
.*[ 	]+R_X86_64_GOTTPOFF[ 	]+xtrn
.*[ 	]+R_X86_64_GOTTPOFF[ 	]+xtrn
.*[ 	]+R_X86_64_GOTTPOFF[ 	]+xtrn\+0xf+c
.*[ 	]+R_X86_64_TLSLD[ 	]+xtrn
.*[ 	]+R_X86_64_TLSLD[ 	]+xtrn
.*[ 	]+R_X86_64_TLSLD[ 	]+xtrn\+0xf+c
.*[ 	]+R_X86_64_DTPOFF64[ 	]+xtrn
.*[ 	]+R_X86_64_DTPOFF32[ 	]+xtrn
.*[ 	]+R_X86_64_DTPOFF32[ 	]+xtrn
.*[ 	]+R_X86_64_TPOFF64[ 	]+xtrn
.*[ 	]+R_X86_64_TPOFF32[ 	]+xtrn
.*[ 	]+R_X86_64_TPOFF32[ 	]+xtrn
Disassembly of section \.data:
#...
.*[ 	]+R_X86_64_64[ 	]+xtrn
.*[ 	]+R_X86_64_PC64[ 	]+xtrn
.*[ 	]+R_X86_64_GOT64[ 	]+xtrn
.*[ 	]+R_X86_64_GOTOFF64[ 	]+xtrn
.*[ 	]+R_X86_64_GOTPCREL64[ 	]+xtrn
.*[ 	]+R_X86_64_DTPOFF64[ 	]+xtrn
.*[ 	]+R_X86_64_TPOFF64[ 	]+xtrn
.*[ 	]+R_X86_64_32[ 	]+xtrn
.*[ 	]+R_X86_64_PC32[ 	]+xtrn
.*[ 	]+R_X86_64_GOT32[ 	]+xtrn
.*[ 	]+R_X86_64_GOTPCREL[ 	]+xtrn
.*[ 	]+R_X86_64_GOTPC32[ 	]+_GLOBAL_OFFSET_TABLE_
.*[ 	]+R_X86_64_GOTPC32[ 	]+_GLOBAL_OFFSET_TABLE_
.*[ 	]+R_X86_64_PLT32[ 	]+xtrn
.*[ 	]+R_X86_64_TLSGD[ 	]+xtrn
.*[ 	]+R_X86_64_GOTTPOFF[ 	]+xtrn
.*[ 	]+R_X86_64_TLSLD[ 	]+xtrn
.*[ 	]+R_X86_64_DTPOFF32[ 	]+xtrn
.*[ 	]+R_X86_64_TPOFF32[ 	]+xtrn
.*[ 	]+R_X86_64_32S[ 	]+xtrn
.*[ 	]+R_X86_64_PC32[ 	]+xtrn
.*[ 	]+R_X86_64_GOT32[ 	]+xtrn
.*[ 	]+R_X86_64_GOTPCREL[ 	]+xtrn
.*[ 	]+R_X86_64_GOTPC32[ 	]+_GLOBAL_OFFSET_TABLE_
.*[ 	]+R_X86_64_GOTPC32[ 	]+_GLOBAL_OFFSET_TABLE_
.*[ 	]+R_X86_64_PLT32[ 	]+xtrn
.*[ 	]+R_X86_64_TLSGD[ 	]+xtrn
.*[ 	]+R_X86_64_GOTTPOFF[ 	]+xtrn
.*[ 	]+R_X86_64_TLSLD[ 	]+xtrn
.*[ 	]+R_X86_64_DTPOFF32[ 	]+xtrn
.*[ 	]+R_X86_64_TPOFF32[ 	]+xtrn
.*[ 	]+R_X86_64_16[ 	]+xtrn
.*[ 	]+R_X86_64_PC16[ 	]+xtrn
.*[ 	]+R_X86_64_8[ 	]+xtrn
.*[ 	]+R_X86_64_PC8[ 	]+xtrn
