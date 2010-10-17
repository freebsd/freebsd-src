#as: --abi=32
#objdump: -dr
#name: Predefined control register names.

.*:     file format .*-sh64.*

Disassembly of section \.text:

[0]+ <start>:
[ 	]+0:[ 	]+240ffd50[ 	]+getcon	sr,r21
[ 	]+4:[ 	]+241ffdf0[ 	]+getcon	ssr,r31
[ 	]+8:[ 	]+242ffd60[ 	]+getcon	pssr,r22
[ 	]+c:[ 	]+244ffd50[ 	]+getcon	intevt,r21
[ 	]+10:[ 	]+245ffd50[ 	]+getcon	expevt,r21
[ 	]+14:[ 	]+246ffd50[ 	]+getcon	pexpevt,r21
[ 	]+18:[ 	]+247ffcc0[ 	]+getcon	tra,r12
[ 	]+1c:[ 	]+248ffd50[ 	]+getcon	spc,r21
[ 	]+20:[ 	]+249ffe90[ 	]+getcon	pspc,r41
[ 	]+24:[ 	]+24affd50[ 	]+getcon	resvec,r21
[ 	]+28:[ 	]+24bffd30[ 	]+getcon	vbr,r19
[ 	]+2c:[ 	]+24dffd50[ 	]+getcon	tea,r21
[ 	]+30:[ 	]+250ffe30[ 	]+getcon	dcr,r35
[ 	]+34:[ 	]+251ffd50[ 	]+getcon	kcr0,r21
[ 	]+38:[ 	]+252ffd50[ 	]+getcon	kcr1,r21
[ 	]+3c:[ 	]+27effd60[ 	]+getcon	ctc,r22
[ 	]+40:[ 	]+27fffd50[ 	]+getcon	usr,r21
[ 	]+44:[ 	]+240ffc20[ 	]+getcon	sr,r2
[ 	]+48:[ 	]+241ffd50[ 	]+getcon	ssr,r21
[ 	]+4c:[ 	]+242ffd50[ 	]+getcon	pssr,r21
[ 	]+50:[ 	]+244ffd50[ 	]+getcon	intevt,r21
[ 	]+54:[ 	]+245ffe60[ 	]+getcon	expevt,r38
[ 	]+58:[ 	]+246ffd50[ 	]+getcon	pexpevt,r21
[ 	]+5c:[ 	]+247ffd50[ 	]+getcon	tra,r21
[ 	]+60:[ 	]+248ffc10[ 	]+getcon	spc,r1
[ 	]+64:[ 	]+249ffd50[ 	]+getcon	pspc,r21
[ 	]+68:[ 	]+24affd50[ 	]+getcon	resvec,r21
[ 	]+6c:[ 	]+24bffef0[ 	]+getcon	vbr,r47
[ 	]+70:[ 	]+24dffd50[ 	]+getcon	tea,r21
[ 	]+74:[ 	]+250ffd50[ 	]+getcon	dcr,r21
[ 	]+78:[ 	]+251ffe30[ 	]+getcon	kcr0,r35
[ 	]+7c:[ 	]+252ffd50[ 	]+getcon	kcr1,r21
[ 	]+80:[ 	]+27effd50[ 	]+getcon	ctc,r21
[ 	]+84:[ 	]+27fffd50[ 	]+getcon	usr,r21
[ 	]+88:[ 	]+6d5ffc00[ 	]+putcon	r21,sr
[ 	]+8c:[ 	]+6dfffc10[ 	]+putcon	r31,ssr
[ 	]+90:[ 	]+6d6ffc20[ 	]+putcon	r22,pssr
[ 	]+94:[ 	]+6d5ffc40[ 	]+putcon	r21,intevt
[ 	]+98:[ 	]+6d5ffc50[ 	]+putcon	r21,expevt
[ 	]+9c:[ 	]+6d5ffc60[ 	]+putcon	r21,pexpevt
[ 	]+a0:[ 	]+6ccffc70[ 	]+putcon	r12,tra
[ 	]+a4:[ 	]+6d5ffc80[ 	]+putcon	r21,spc
[ 	]+a8:[ 	]+6e9ffc90[ 	]+putcon	r41,pspc
[ 	]+ac:[ 	]+6d5ffca0[ 	]+putcon	r21,resvec
[ 	]+b0:[ 	]+6d3ffcb0[ 	]+putcon	r19,vbr
[ 	]+b4:[ 	]+6d5ffcd0[ 	]+putcon	r21,tea
[ 	]+b8:[ 	]+6e3ffd00[ 	]+putcon	r35,dcr
[ 	]+bc:[ 	]+6d5ffd10[ 	]+putcon	r21,kcr0
[ 	]+c0:[ 	]+6d5ffd20[ 	]+putcon	r21,kcr1
[ 	]+c4:[ 	]+6d6fffe0[ 	]+putcon	r22,ctc
[ 	]+c8:[ 	]+6d5ffff0[ 	]+putcon	r21,usr
[ 	]+cc:[ 	]+6c2ffc00[ 	]+putcon	r2,sr
[ 	]+d0:[ 	]+6d5ffc10[ 	]+putcon	r21,ssr
[ 	]+d4:[ 	]+6d5ffc20[ 	]+putcon	r21,pssr
[ 	]+d8:[ 	]+6d5ffc40[ 	]+putcon	r21,intevt
[ 	]+dc:[ 	]+6e6ffc50[ 	]+putcon	r38,expevt
[ 	]+e0:[ 	]+6d5ffc60[ 	]+putcon	r21,pexpevt
[ 	]+e4:[ 	]+6d5ffc70[ 	]+putcon	r21,tra
[ 	]+e8:[ 	]+6c1ffc80[ 	]+putcon	r1,spc
[ 	]+ec:[ 	]+6d5ffc90[ 	]+putcon	r21,pspc
[ 	]+f0:[ 	]+6d5ffca0[ 	]+putcon	r21,resvec
[ 	]+f4:[ 	]+6efffcb0[ 	]+putcon	r47,vbr
[ 	]+f8:[ 	]+6d5ffcd0[ 	]+putcon	r21,tea
[ 	]+fc:[ 	]+6d5ffd00[ 	]+putcon	r21,dcr
[ 	]+100:[ 	]+6e3ffd10[ 	]+putcon	r35,kcr0
[ 	]+104:[ 	]+6d5ffd20[ 	]+putcon	r21,kcr1
[ 	]+108:[ 	]+6d5fffe0[ 	]+putcon	r21,ctc
[ 	]+10c:[ 	]+6d5ffff0[ 	]+putcon	r21,usr
