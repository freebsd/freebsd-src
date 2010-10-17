SECTIONS {
 .text 0xa00 : { *(.text); *(.gnu.linkonce.t.*) }
 .eh_frame 0x2000 : { *(.eh_frame) }
}
