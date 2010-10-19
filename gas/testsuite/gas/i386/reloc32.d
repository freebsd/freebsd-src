#objdump: -Drw
#name: i386 relocs

.*: +file format .*i386.*

Disassembly of section \.text:
#...
.*[ 	]+R_386_32[ 	]+xtrn
.*[ 	]+R_386_16[ 	]+xtrn
.*[ 	]+R_386_8[ 	]+xtrn
.*[ 	]+R_386_32[ 	]+xtrn
.*[ 	]+R_386_16[ 	]+xtrn
.*[ 	]+R_386_PC32[ 	]+xtrn
.*[ 	]+R_386_PC16[ 	]+xtrn
.*[ 	]+R_386_PC8[ 	]+xtrn
.*[ 	]+R_386_PC32[ 	]+xtrn
.*[ 	]+R_386_PC16[ 	]+xtrn
.*[ 	]+R_386_PC32[ 	]+xtrn
.*[ 	]+R_386_PC8[ 	]+xtrn
.*[ 	]+R_386_GOT32[ 	]+xtrn
.*[ 	]+R_386_GOT32[ 	]+xtrn
.*[ 	]+R_386_GOTOFF[ 	]+xtrn
.*[ 	]+R_386_GOTOFF[ 	]+xtrn
.*[ 	]+R_386_GOTPC[ 	]+_GLOBAL_OFFSET_TABLE_
.*[ 	]+R_386_GOTPC[ 	]+_GLOBAL_OFFSET_TABLE_
.*[ 	]+R_386_PLT32[ 	]+xtrn
.*[ 	]+R_386_PLT32[ 	]+xtrn
.*[ 	]+R_386_PLT32[ 	]+xtrn
.*[ 	]+R_386_TLS_GD[ 	]+xtrn
.*[ 	]+R_386_TLS_GD[ 	]+xtrn
.*[ 	]+R_386_TLS_GOTIE[ 	]+xtrn
.*[ 	]+R_386_TLS_GOTIE[ 	]+xtrn
.*[ 	]+R_386_TLS_IE[ 	]+xtrn
.*[ 	]+R_386_TLS_IE[ 	]+xtrn
.*[ 	]+R_386_TLS_IE_32[ 	]+xtrn
.*[ 	]+R_386_TLS_IE_32[ 	]+xtrn
.*[ 	]+R_386_TLS_LDM[ 	]+xtrn
.*[ 	]+R_386_TLS_LDM[ 	]+xtrn
.*[ 	]+R_386_TLS_LDO_32[ 	]+xtrn
.*[ 	]+R_386_TLS_LDO_32[ 	]+xtrn
.*[ 	]+R_386_TLS_LE[ 	]+xtrn
.*[ 	]+R_386_TLS_LE[ 	]+xtrn
.*[ 	]+R_386_TLS_LE_32[ 	]+xtrn
.*[ 	]+R_386_TLS_LE_32[ 	]+xtrn
Disassembly of section \.data:
#...
.*[ 	]+R_386_32[ 	]+xtrn
.*[ 	]+R_386_PC32[ 	]+xtrn
.*[ 	]+R_386_GOT32[ 	]+xtrn
.*[ 	]+R_386_GOTOFF[ 	]+xtrn
.*[ 	]+R_386_GOTPC[ 	]+_GLOBAL_OFFSET_TABLE_
.*[ 	]+R_386_GOTPC[ 	]+_GLOBAL_OFFSET_TABLE_
.*[ 	]+R_386_PLT32[ 	]+xtrn
#...
.*[ 	]+R_386_TLS_GD[ 	]+xtrn
#...
.*[ 	]+R_386_TLS_GOTIE[ 	]+xtrn
.*[ 	]+R_386_TLS_IE[ 	]+xtrn
.*[ 	]+R_386_TLS_IE_32[ 	]+xtrn
.*[ 	]+R_386_TLS_LDM[ 	]+xtrn
.*[ 	]+R_386_TLS_LDO_32[ 	]+xtrn
.*[ 	]+R_386_TLS_LE[ 	]+xtrn
.*[ 	]+R_386_TLS_LE_32[ 	]+xtrn
.*[ 	]+R_386_16[ 	]+xtrn
.*[ 	]+R_386_PC16[ 	]+xtrn
.*[ 	]+R_386_8[ 	]+xtrn
.*[ 	]+R_386_PC8[ 	]+xtrn
