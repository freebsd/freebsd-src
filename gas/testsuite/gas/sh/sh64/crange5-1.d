#as: --abi=32 --isa=SHmedia
#objdump: -sr
#source: crange5.s
#name: Avoid zero length .cranges range descriptor at .align in code.

.*:     file format .*-sh64.*

Contents of section \.text:
 0000 e8003a00 d4ff80f0 4455fc00 acf000e0  .*
 0010 acf00c00 acf009c0 acf00520 00f8fce0  .*
 0020 0029fc10 e4110200 ebffda50 d81201c0  .*
 0030 e8000a00 cc000420 6ff0fff0           .*
