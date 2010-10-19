# name: H8300 Relaxation Test 5 (for COFF)
# source: relax-5.s
# ld: --relax -m h8300s
# objdump: -d --no-show-raw-insn

.*:     file format .*-h8300

Disassembly of section .text:

00000100 <_start>:
 100:.*ldc	@0x0:16,ccr
 106:.*ldc	@0x7fff:16,ccr
 10c:.*ldc	@0x8000:32,ccr
 114:.*ldc	@0xff00:32,ccr
 11c:.*ldc	@0xffff00:32,ccr
 124:.*ldc	@0xffff7fff:32,ccr
 12c:.*ldc	@0x8000:16,ccr
 132:.*ldc	@0xfeff:16,ccr
 138:.*ldc	@0xff00:16,ccr
 13e:.*ldc	@0xffff:16,ccr
 144:.*stc	ccr,@0x0:16
 14a:.*stc	ccr,@0x7fff:16
 150:.*stc	ccr,@0x8000:32
 158:.*stc	ccr,@0xff00:32
 160:.*stc	ccr,@0xffff00:32
 168:.*stc	ccr,@0xffff7fff:32
 170:.*stc	ccr,@0x8000:16
 176:.*stc	ccr,@0xfeff:16
 17c:.*stc	ccr,@0xff00:16
 182:.*stc	ccr,@0xffff:16
 188:.*ldc	@0x0:16,exr
 18e:.*ldc	@0x7fff:16,exr
 194:.*ldc	@0x8000:32,exr
 19c:.*ldc	@0xff00:32,exr
 1a4:.*ldc	@0xffff00:32,exr
 1ac:.*ldc	@0xffff7fff:32,exr
 1b4:.*ldc	@0x8000:16,exr
 1ba:.*ldc	@0xfeff:16,exr
 1c0:.*ldc	@0xff00:16,exr
 1c6:.*ldc	@0xffff:16,exr
 1cc:.*stc	exr,@0x0:16
 1d2:.*stc	exr,@0x7fff:16
 1d8:.*stc	exr,@0x8000:32
 1e0:.*stc	exr,@0xff00:32
 1e8:.*stc	exr,@0xffff00:32
 1f0:.*stc	exr,@0xffff7fff:32
 1f8:.*stc	exr,@0x8000:16
 1fe:.*stc	exr,@0xfeff:16
 204:.*stc	exr,@0xff00:16
 20a:.*stc	exr,@0xffff:16
