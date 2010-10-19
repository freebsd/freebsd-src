#objdump: -d
#name: instructions with offsets

.*: .*

Disassembly of section .text:

0+ <.text>:

[ 	]+0:[ 	]+18 7e[ 	]+jr 0x0080
[ 	]+2:[ 	]+dd 34 05[ 	]+inc \(ix\+5\)
[ 	]+5:[ 	]+fd 35 ff[ 	]+dec \(iy\+?-1\)
[ 	]+8:[ 	]+dd 7e 80[ 	]+ld a,\(ix\+?-128\)
[ 	]+b:[ 	]+fd 77 7f[ 	]+ld \(iy\+127\),a
[ 	]+e:[ 	]+10 f0[ 	]+djnz 0x0000
[ 	]+10:[ 	]+28 02[ 	]+jr z,0x0014
[ 	]+12:[ 	]+38 04[ 	]+jr c,0x0018
[ 	]+14:[ 	]+20 02[ 	]+jr nz,0x0018
[ 	]+16:[ 	]+30 fc[ 	]+jr nc,0x0014
[ 	]+18:[ 	]+dd 36 22 09[ 	]+ld \(ix\+34\),0x09
[ 	]+1c:[ 	]+fd 36 de f7[ 	]+ld \(iy\+?-34\),0xf7
[ 	]+20:[ 	]+dd cb 37 1e[ 	]+rr \(ix\+55\)
[ 	]+24:[ 	]+fd cb c9 16[ 	]+rl \(iy\+?-55\)
#pass
