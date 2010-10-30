#objdump: -t
#as: -mips32r2
#name: MIPS16 intermix

.*: +file format .*mips.*

SYMBOL TABLE:
#...
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static1_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static1_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static32_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static32_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static16_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static16_l
0+[0-9a-f]+ l    d  .mips16.fn.m16_d	0+[0-9a-f]+ .mips16.fn.m16_d
0+[0-9a-f]+ l     F .mips16.fn.m16_d	0+[0-9a-f]+ __fn_stub_m16_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static_d
0+[0-9a-f]+ l    d  .mips16.fn.m16_static_d	0+[0-9a-f]+ .mips16.fn.m16_static_d
0+[0-9a-f]+ l     F .mips16.fn.m16_static_d	0+[0-9a-f]+ __fn_stub_m16_static_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static1_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static1_d
0+[0-9a-f]+ l    d  .mips16.fn.m16_static1_d	0+[0-9a-f]+ .mips16.fn.m16_static1_d
0+[0-9a-f]+ l     F .mips16.fn.m16_static1_d	0+[0-9a-f]+ __fn_stub_m16_static1_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static32_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static32_d
0+[0-9a-f]+ l    d  .mips16.fn.m16_static32_d	0+[0-9a-f]+ .mips16.fn.m16_static32_d
0+[0-9a-f]+ l     F .mips16.fn.m16_static32_d	0+[0-9a-f]+ __fn_stub_m16_static32_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static16_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static16_d
0+[0-9a-f]+ l    d  .mips16.fn.m16_static16_d	0+[0-9a-f]+ .mips16.fn.m16_static16_d
0+[0-9a-f]+ l     F .mips16.fn.m16_static16_d	0+[0-9a-f]+ __fn_stub_m16_static16_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static_ld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static_ld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static1_ld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static1_ld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static32_ld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static32_ld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static16_ld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static16_ld
0+[0-9a-f]+ l    d  .mips16.fn.m16_dl	0+[0-9a-f]+ .mips16.fn.m16_dl
0+[0-9a-f]+ l     F .mips16.fn.m16_dl	0+[0-9a-f]+ __fn_stub_m16_dl
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static_dl
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static_dl
0+[0-9a-f]+ l    d  .mips16.fn.m16_static_dl	0+[0-9a-f]+ .mips16.fn.m16_static_dl
0+[0-9a-f]+ l     F .mips16.fn.m16_static_dl	0+[0-9a-f]+ __fn_stub_m16_static_dl
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static1_dl
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static1_dl
0+[0-9a-f]+ l    d  .mips16.fn.m16_static1_dl	0+[0-9a-f]+ .mips16.fn.m16_static1_dl
0+[0-9a-f]+ l     F .mips16.fn.m16_static1_dl	0+[0-9a-f]+ __fn_stub_m16_static1_dl
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static32_dl
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static32_dl
0+[0-9a-f]+ l    d  .mips16.fn.m16_static32_dl	0+[0-9a-f]+ .mips16.fn.m16_static32_dl
0+[0-9a-f]+ l     F .mips16.fn.m16_static32_dl	0+[0-9a-f]+ __fn_stub_m16_static32_dl
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static16_dl
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static16_dl
0+[0-9a-f]+ l    d  .mips16.fn.m16_static16_dl	0+[0-9a-f]+ .mips16.fn.m16_static16_dl
0+[0-9a-f]+ l     F .mips16.fn.m16_static16_dl	0+[0-9a-f]+ __fn_stub_m16_static16_dl
0+[0-9a-f]+ l    d  .mips16.fn.m16_dlld	0+[0-9a-f]+ .mips16.fn.m16_dlld
0+[0-9a-f]+ l     F .mips16.fn.m16_dlld	0+[0-9a-f]+ __fn_stub_m16_dlld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static_dlld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static_dlld
0+[0-9a-f]+ l    d  .mips16.fn.m16_static_dlld	0+[0-9a-f]+ .mips16.fn.m16_static_dlld
0+[0-9a-f]+ l     F .mips16.fn.m16_static_dlld	0+[0-9a-f]+ __fn_stub_m16_static_dlld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static1_dlld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static1_dlld
0+[0-9a-f]+ l    d  .mips16.fn.m16_static1_dlld	0+[0-9a-f]+ .mips16.fn.m16_static1_dlld
0+[0-9a-f]+ l     F .mips16.fn.m16_static1_dlld	0+[0-9a-f]+ __fn_stub_m16_static1_dlld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static32_dlld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static32_dlld
0+[0-9a-f]+ l    d  .mips16.fn.m16_static32_dlld	0+[0-9a-f]+ .mips16.fn.m16_static32_dlld
0+[0-9a-f]+ l     F .mips16.fn.m16_static32_dlld	0+[0-9a-f]+ __fn_stub_m16_static32_dlld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static16_dlld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static16_dlld
0+[0-9a-f]+ l    d  .mips16.fn.m16_static16_dlld	0+[0-9a-f]+ .mips16.fn.m16_static16_dlld
0+[0-9a-f]+ l     F .mips16.fn.m16_static16_dlld	0+[0-9a-f]+ __fn_stub_m16_static16_dlld
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static_d_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static_d_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static1_d_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static1_d_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static32_d_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static32_d_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static16_d_l
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static16_d_l
0+[0-9a-f]+ l    d  .mips16.fn.m16_d_d	0+[0-9a-f]+ .mips16.fn.m16_d_d
0+[0-9a-f]+ l     F .mips16.fn.m16_d_d	0+[0-9a-f]+ __fn_stub_m16_d_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static_d_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static_d_d
0+[0-9a-f]+ l    d  .mips16.fn.m16_static_d_d	0+[0-9a-f]+ .mips16.fn.m16_static_d_d
0+[0-9a-f]+ l     F .mips16.fn.m16_static_d_d	0+[0-9a-f]+ __fn_stub_m16_static_d_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static1_d_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static1_d_d
0+[0-9a-f]+ l    d  .mips16.fn.m16_static1_d_d	0+[0-9a-f]+ .mips16.fn.m16_static1_d_d
0+[0-9a-f]+ l     F .mips16.fn.m16_static1_d_d	0+[0-9a-f]+ __fn_stub_m16_static1_d_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static32_d_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static32_d_d
0+[0-9a-f]+ l    d  .mips16.fn.m16_static32_d_d	0+[0-9a-f]+ .mips16.fn.m16_static32_d_d
0+[0-9a-f]+ l     F .mips16.fn.m16_static32_d_d	0+[0-9a-f]+ __fn_stub_m16_static32_d_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ m32_static16_d_d
0+[0-9a-f]+ l     F .text	0+[0-9a-f]+ 0xf0 m16_static16_d_d
0+[0-9a-f]+ l    d  .mips16.fn.m16_static16_d_d	0+[0-9a-f]+ .mips16.fn.m16_static16_d_d
0+[0-9a-f]+ l     F .mips16.fn.m16_static16_d_d	0+[0-9a-f]+ __fn_stub_m16_static16_d_d
0+[0-9a-f]+ l    d  .mips16.call.m32_static1_d	0+[0-9a-f]+ .mips16.call.m32_static1_d
0+[0-9a-f]+ l     F .mips16.call.m32_static1_d	0+[0-9a-f]+ __call_stub_m32_static1_d
0+[0-9a-f]+ l    d  .mips16.call.m16_static1_d	0+[0-9a-f]+ .mips16.call.m16_static1_d
0+[0-9a-f]+ l     F .mips16.call.m16_static1_d	0+[0-9a-f]+ __call_stub_m16_static1_d
0+[0-9a-f]+ l    d  .mips16.call.m32_static1_dl	0+[0-9a-f]+ .mips16.call.m32_static1_dl
0+[0-9a-f]+ l     F .mips16.call.m32_static1_dl	0+[0-9a-f]+ __call_stub_m32_static1_dl
0+[0-9a-f]+ l    d  .mips16.call.m16_static1_dl	0+[0-9a-f]+ .mips16.call.m16_static1_dl
0+[0-9a-f]+ l     F .mips16.call.m16_static1_dl	0+[0-9a-f]+ __call_stub_m16_static1_dl
0+[0-9a-f]+ l    d  .mips16.call.m32_static1_dlld	0+[0-9a-f]+ .mips16.call.m32_static1_dlld
0+[0-9a-f]+ l     F .mips16.call.m32_static1_dlld	0+[0-9a-f]+ __call_stub_m32_static1_dlld
0+[0-9a-f]+ l    d  .mips16.call.m16_static1_dlld	0+[0-9a-f]+ .mips16.call.m16_static1_dlld
0+[0-9a-f]+ l     F .mips16.call.m16_static1_dlld	0+[0-9a-f]+ __call_stub_m16_static1_dlld
0+[0-9a-f]+ l    d  .mips16.call.fp.m32_static1_d_l	0+[0-9a-f]+ .mips16.call.fp.m32_static1_d_l
0+[0-9a-f]+ l     F .mips16.call.fp.m32_static1_d_l	0+[0-9a-f]+ __call_stub_fp_m32_static1_d_l
0+[0-9a-f]+ l    d  .mips16.call.fp.m16_static1_d_l	0+[0-9a-f]+ .mips16.call.fp.m16_static1_d_l
0+[0-9a-f]+ l     F .mips16.call.fp.m16_static1_d_l	0+[0-9a-f]+ __call_stub_fp_m16_static1_d_l
0+[0-9a-f]+ l    d  .mips16.call.fp.m32_static1_d_d	0+[0-9a-f]+ .mips16.call.fp.m32_static1_d_d
0+[0-9a-f]+ l     F .mips16.call.fp.m32_static1_d_d	0+[0-9a-f]+ __call_stub_fp_m32_static1_d_d
0+[0-9a-f]+ l    d  .mips16.call.fp.m16_static1_d_d	0+[0-9a-f]+ .mips16.call.fp.m16_static1_d_d
0+[0-9a-f]+ l     F .mips16.call.fp.m16_static1_d_d	0+[0-9a-f]+ __call_stub_fp_m16_static1_d_d
0+[0-9a-f]+ l    d  .mips16.call.m32_static16_d	0+[0-9a-f]+ .mips16.call.m32_static16_d
0+[0-9a-f]+ l     F .mips16.call.m32_static16_d	0+[0-9a-f]+ __call_stub_m32_static16_d
0+[0-9a-f]+ l    d  .mips16.call.m16_static16_d	0+[0-9a-f]+ .mips16.call.m16_static16_d
0+[0-9a-f]+ l     F .mips16.call.m16_static16_d	0+[0-9a-f]+ __call_stub_m16_static16_d
0+[0-9a-f]+ l    d  .mips16.call.m32_static16_dl	0+[0-9a-f]+ .mips16.call.m32_static16_dl
0+[0-9a-f]+ l     F .mips16.call.m32_static16_dl	0+[0-9a-f]+ __call_stub_m32_static16_dl
0+[0-9a-f]+ l    d  .mips16.call.m16_static16_dl	0+[0-9a-f]+ .mips16.call.m16_static16_dl
0+[0-9a-f]+ l     F .mips16.call.m16_static16_dl	0+[0-9a-f]+ __call_stub_m16_static16_dl
0+[0-9a-f]+ l    d  .mips16.call.m32_static16_dlld	0+[0-9a-f]+ .mips16.call.m32_static16_dlld
0+[0-9a-f]+ l     F .mips16.call.m32_static16_dlld	0+[0-9a-f]+ __call_stub_m32_static16_dlld
0+[0-9a-f]+ l    d  .mips16.call.m16_static16_dlld	0+[0-9a-f]+ .mips16.call.m16_static16_dlld
0+[0-9a-f]+ l     F .mips16.call.m16_static16_dlld	0+[0-9a-f]+ __call_stub_m16_static16_dlld
0+[0-9a-f]+ l    d  .mips16.call.fp.m32_static16_d_l	0+[0-9a-f]+ .mips16.call.fp.m32_static16_d_l
0+[0-9a-f]+ l     F .mips16.call.fp.m32_static16_d_l	0+[0-9a-f]+ __call_stub_fp_m32_static16_d_l
0+[0-9a-f]+ l    d  .mips16.call.fp.m16_static16_d_l	0+[0-9a-f]+ .mips16.call.fp.m16_static16_d_l
0+[0-9a-f]+ l     F .mips16.call.fp.m16_static16_d_l	0+[0-9a-f]+ __call_stub_fp_m16_static16_d_l
0+[0-9a-f]+ l    d  .mips16.call.fp.m32_static16_d_d	0+[0-9a-f]+ .mips16.call.fp.m32_static16_d_d
0+[0-9a-f]+ l     F .mips16.call.fp.m32_static16_d_d	0+[0-9a-f]+ __call_stub_fp_m32_static16_d_d
0+[0-9a-f]+ l    d  .mips16.call.fp.m16_static16_d_d	0+[0-9a-f]+ .mips16.call.fp.m16_static16_d_d
0+[0-9a-f]+ l     F .mips16.call.fp.m16_static16_d_d	0+[0-9a-f]+ __call_stub_fp_m16_static16_d_d
#...
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ m32_l
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ 0xf0 m16_l
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ m32_d
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ 0xf0 m16_d
#...
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ m32_ld
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ 0xf0 m16_ld
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ m32_dl
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ 0xf0 m16_dl
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ m32_dlld
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ 0xf0 m16_dlld
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ m32_d_l
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ 0xf0 m16_d_l
#...
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ m32_d_d
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ 0xf0 m16_d_d
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ f32
0+[0-9a-f]+ g     F .text	0+[0-9a-f]+ 0xf0 f16
#pass
