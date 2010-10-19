#as: --underscore --em=criself --march=v32
#objdump: -dr

.*:     file format elf32-us-cris

Disassembly of section .text:

0+ <here>:
[ 	]+0:[ 	]+3306[ 	]+move r3,bz
[ 	]+2:[ 	]+3516[ 	]+move r5,vr
[ 	]+4:[ 	]+3626[ 	]+move r6,pid
[ 	]+6:[ 	]+3736[ 	]+move r7,srs
[ 	]+8:[ 	]+3846[ 	]+move r8,wz
[ 	]+a:[ 	]+3956[ 	]+move r9,exs
[ 	]+c:[ 	]+3566[ 	]+move r5,eda
[ 	]+e:[ 	]+3676[ 	]+move r6,mof
[ 	]+10:[ 	]+3786[ 	]+move r7,dz
[ 	]+12:[ 	]+3296[ 	]+move r2,ebp
[ 	]+14:[ 	]+34a6[ 	]+move r4,erp
[ 	]+16:[ 	]+30b6[ 	]+move r0,srp
[ 	]+18:[ 	]+36c6[ 	]+move r6,nrp
[ 	]+1a:[ 	]+3ad6[ 	]+move r10,ccs
[ 	]+1c:[ 	]+3ce6[ 	]+move r12,usp
[ 	]+1e:[ 	]+3df6[ 	]+move r13,spc
[ 	]+20:[ 	]+7306[ 	]+clear\.b r3
[ 	]+22:[ 	]+7516[ 	]+move vr,r5
[ 	]+24:[ 	]+7626[ 	]+move pid,r6
[ 	]+26:[ 	]+7736[ 	]+move srs,r7
[ 	]+28:[ 	]+7846[ 	]+clear\.w r8
[ 	]+2a:[ 	]+7956[ 	]+move exs,r9
[ 	]+2c:[ 	]+7566[ 	]+move eda,r5
[ 	]+2e:[ 	]+7676[ 	]+move mof,r6
[ 	]+30:[ 	]+7786[ 	]+clear\.d r7
[ 	]+32:[ 	]+7296[ 	]+move ebp,r2
[ 	]+34:[ 	]+74a6[ 	]+move erp,r4
[ 	]+36:[ 	]+70b6[ 	]+move srp,r0
[ 	]+38:[ 	]+76c6[ 	]+move nrp,r6
[ 	]+3a:[ 	]+7ad6[ 	]+move ccs,r10
[ 	]+3c:[ 	]+7ce6[ 	]+move usp,r12
[ 	]+3e:[ 	]+7df6[ 	]+move spc,r13
[ 	]+40:[ 	]+3f0e 0300 0000[ 	]+move 3 <here\+0x3>,bz
[ 	]+46:[ 	]+3f1e 0500 0000[ 	]+move 5 <here\+0x5>,vr
[ 	]+4c:[ 	]+3f2e 0600 0000[ 	]+move 6 <here\+0x6>,pid
[ 	]+52:[ 	]+3f3e 0700 0000[ 	]+move 7 <here\+0x7>,srs
[ 	]+58:[ 	]+3f4e 0800 0000[ 	]+move 8 <here\+0x8>,wz
[ 	]+5e:[ 	]+3f5e 0900 0000[ 	]+move 9 <here\+0x9>,exs
[ 	]+64:[ 	]+3f6e 0a00 0000[ 	]+move a <here\+0xa>,eda
[ 	]+6a:[ 	]+3f7e 6500 0000[ 	]+move 65 <here\+0x65>,mof
[ 	]+70:[ 	]+3f8e 7800 0000[ 	]+move 78 <here\+0x78>,dz
[ 	]+76:[ 	]+3f9e 0d00 0000[ 	]+move d <here\+0xd>,ebp
[ 	]+7c:[ 	]+3fae 0400 0000[ 	]+move 4 <here\+0x4>,erp
[ 	]+82:[ 	]+3fbe 0000 0000[ 	]+move 0 <here>,srp
[ 	]+88:[ 	]+3fce 0600 0000[ 	]+move 6 <here\+0x6>,nrp
[ 	]+8e:[ 	]+3fde 0a00 0000[ 	]+move a <here\+0xa>,ccs
[ 	]+94:[ 	]+3fee 0c00 0000[ 	]+move c <here\+0xc>,usp
[ 	]+9a:[ 	]+3ffe 0d00 0000[ 	]+move d <here\+0xd>,spc
[ 	]+a0:[ 	]+730a[ 	]+clear\.b \[r3\]
[ 	]+a2:[ 	]+751a[ 	]+move vr,\[r5\]
[ 	]+a4:[ 	]+762a[ 	]+move pid,\[r6\]
[ 	]+a6:[ 	]+773a[ 	]+move srs,\[r7\]
[ 	]+a8:[ 	]+784a[ 	]+clear\.w \[r8\]
[ 	]+aa:[ 	]+795a[ 	]+move exs,\[r9\]
[ 	]+ac:[ 	]+756a[ 	]+move eda,\[r5\]
[ 	]+ae:[ 	]+767a[ 	]+move mof,\[r6\]
[ 	]+b0:[ 	]+778a[ 	]+clear\.d \[r7\]
[ 	]+b2:[ 	]+729a[ 	]+move ebp,\[r2\]
[ 	]+b4:[ 	]+74aa[ 	]+move erp,\[r4\]
[ 	]+b6:[ 	]+70ba[ 	]+move srp,\[r0\]
[ 	]+b8:[ 	]+76ca[ 	]+move nrp,\[r6\]
[ 	]+ba:[ 	]+7ada[ 	]+move ccs,\[r10\]
[ 	]+bc:[ 	]+7cea[ 	]+move usp,\[r12\]
[ 	]+be:[ 	]+7dfa[ 	]+move spc,\[r13\]
[ 	]+c0:[ 	]+330a[ 	]+move \[r3\],bz
[ 	]+c2:[ 	]+351a[ 	]+move \[r5\],vr
[ 	]+c4:[ 	]+362a[ 	]+move \[r6\],pid
[ 	]+c6:[ 	]+373a[ 	]+move \[r7\],srs
[ 	]+c8:[ 	]+384a[ 	]+move \[r8\],wz
[ 	]+ca:[ 	]+395a[ 	]+move \[r9\],exs
[ 	]+cc:[ 	]+356a[ 	]+move \[r5\],eda
[ 	]+ce:[ 	]+367a[ 	]+move \[r6\],mof
[ 	]+d0:[ 	]+378a[ 	]+move \[r7\],dz
[ 	]+d2:[ 	]+329a[ 	]+move \[r2\],ebp
[ 	]+d4:[ 	]+34aa[ 	]+move \[r4\],erp
[ 	]+d6:[ 	]+30ba[ 	]+move \[r0\],srp
[ 	]+d8:[ 	]+36ca[ 	]+move \[r6\],nrp
[ 	]+da:[ 	]+3ada[ 	]+move \[r10\],ccs
[ 	]+dc:[ 	]+3cea[ 	]+move \[r12\],usp
[ 	]+de:[ 	]+3dfa[ 	]+move \[r13\],spc
