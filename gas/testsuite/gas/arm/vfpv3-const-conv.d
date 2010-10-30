# name: VFPv3 additional constant and conversion ops
# as: -mfpu=vfp3
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section \.text:
0[0-9a-f]+ <[^>]+> eef08a04 	fconsts	s17, #4
0[0-9a-f]+ <[^>]+> eeba9a05 	fconsts	s18, #165
0[0-9a-f]+ <[^>]+> eef49a00 	fconsts	s19, #64
0[0-9a-f]+ <[^>]+> eef01b04 	fconstd	d17, #4
0[0-9a-f]+ <[^>]+> eefa2b05 	fconstd	d18, #165
0[0-9a-f]+ <[^>]+> eef43b00 	fconstd	d19, #64
0[0-9a-f]+ <[^>]+> eefa8a63 	fshtos	s17, #9
0[0-9a-f]+ <[^>]+> eefa1b63 	fshtod	d17, #9
0[0-9a-f]+ <[^>]+> eefa8aeb 	fsltos	s17, #9
0[0-9a-f]+ <[^>]+> eefa1beb 	fsltod	d17, #9
0[0-9a-f]+ <[^>]+> eefb8a63 	fuhtos	s17, #9
0[0-9a-f]+ <[^>]+> eefb1b63 	fuhtod	d17, #9
0[0-9a-f]+ <[^>]+> eefb8aeb 	fultos	s17, #9
0[0-9a-f]+ <[^>]+> eefb1beb 	fultod	d17, #9
0[0-9a-f]+ <[^>]+> eefe9a64 	ftoshs	s19, #7
0[0-9a-f]+ <[^>]+> eefe3b64 	ftoshd	d19, #7
0[0-9a-f]+ <[^>]+> eefe9aec 	ftosls	s19, #7
0[0-9a-f]+ <[^>]+> eefe3bec 	ftosld	d19, #7
0[0-9a-f]+ <[^>]+> eeff9a64 	ftouhs	s19, #7
0[0-9a-f]+ <[^>]+> eeff3b64 	ftouhd	d19, #7
0[0-9a-f]+ <[^>]+> eeff9aec 	ftouls	s19, #7
0[0-9a-f]+ <[^>]+> eeff3bec 	ftould	d19, #7
