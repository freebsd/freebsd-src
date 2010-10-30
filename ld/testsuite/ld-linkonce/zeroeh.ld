SECTIONS {
 .text 0xa00 : { *(.text); *(.gnu.linkonce.t.*) }
 .gcc_except_table 0x2000 : { *(.gcc_except_table) }
 .eh_frame 0x4000 : { *(.eh_frame) }
}
