# name: H8300 Relaxation Test 5
# source: relax-5.s
# ld: --relax -m h8300self
# objdump: -d --no-show-raw-insn

.*:     file format .*-h8300

Disassembly of section .text:

00000100 <_start>:
 100:	01 40 6b 00 00 00 ldc	@0x0:16,ccr
 106:	01 40 6b 00 7f ff ldc	@0x7fff:16,ccr
 10c:	01 40 6b 20 00 00 80 00 ldc	@0x8000:32,ccr
 114:	01 40 6b 20 00 00 ff 00 ldc	@0xff00:32,ccr
 11c:	01 40 6b 20 00 ff ff 00 ldc	@0xffff00:32,ccr
 124:	01 40 6b 20 ff ff 7f ff ldc	@0xffff7fff:32,ccr
 12c:	01 40 6b 00 80 00 ldc	@0x8000:16,ccr
 132:	01 40 6b 00 fe ff ldc	@0xfeff:16,ccr
 138:	01 40 6b 00 ff 00 ldc	@0xff00:16,ccr
 13e:	01 40 6b 00 ff ff ldc	@0xffff:16,ccr
 144:	01 40 6b 80 00 00 stc	ccr,@0x0:16
 14a:	01 40 6b 80 7f ff stc	ccr,@0x7fff:16
 150:	01 40 6b a0 00 00 80 00 stc	ccr,@0x8000:32
 158:	01 40 6b a0 00 00 ff 00 stc	ccr,@0xff00:32
 160:	01 40 6b a0 00 ff ff 00 stc	ccr,@0xffff00:32
 168:	01 40 6b a0 ff ff 7f ff stc	ccr,@0xffff7fff:32
 170:	01 40 6b 80 80 00 stc	ccr,@0x8000:16
 176:	01 40 6b 80 fe ff stc	ccr,@0xfeff:16
 17c:	01 40 6b 80 ff 00 stc	ccr,@0xff00:16
 182:	01 40 6b 80 ff ff stc	ccr,@0xffff:16
 188:	01 41 6b 00 00 00 ldc	@0x0:16,exr
 18e:	01 41 6b 00 7f ff ldc	@0x7fff:16,exr
 194:	01 41 6b 20 00 00 80 00 ldc	@0x8000:32,exr
 19c:	01 41 6b 20 00 00 ff 00 ldc	@0xff00:32,exr
 1a4:	01 41 6b 20 00 ff ff 00 ldc	@0xffff00:32,exr
 1ac:	01 41 6b 20 ff ff 7f ff ldc	@0xffff7fff:32,exr
 1b4:	01 41 6b 00 80 00 ldc	@0x8000:16,exr
 1ba:	01 41 6b 00 fe ff ldc	@0xfeff:16,exr
 1c0:	01 41 6b 00 ff 00 ldc	@0xff00:16,exr
 1c6:	01 41 6b 00 ff ff ldc	@0xffff:16,exr
 1cc:	01 41 6b 80 00 00 stc	exr,@0x0:16
 1d2:	01 41 6b 80 7f ff stc	exr,@0x7fff:16
 1d8:	01 41 6b a0 00 00 80 00 stc	exr,@0x8000:32
 1e0:	01 41 6b a0 00 00 ff 00 stc	exr,@0xff00:32
 1e8:	01 41 6b a0 00 ff ff 00 stc	exr,@0xffff00:32
 1f0:	01 41 6b a0 ff ff 7f ff stc	exr,@0xffff7fff:32
 1f8:	01 41 6b 80 80 00 stc	exr,@0x8000:16
 1fe:	01 41 6b 80 fe ff stc	exr,@0xfeff:16
 204:	01 41 6b 80 ff 00 stc	exr,@0xff00:16
 20a:	01 41 6b 80 ff ff stc	exr,@0xffff:16
