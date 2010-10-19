#objdump: -d --architecture=m68k:cfv4e
#as: -mcfv4e

.*:     file format .*

Disassembly of section .text:

0+ <.text>:
[ 0-9a-f]+:	f200 0004      	fsqrtd %fp0,%fp0
[ 0-9a-f]+:	f22e 4004 0008 	fsqrtl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4404 0008 	fsqrts %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5004 0008 	fsqrtw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5404 0008 	fsqrtd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5804 0008 	fsqrtb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0041      	fssqrtd %fp0,%fp0
[ 0-9a-f]+:	f22e 4041 0008 	fssqrtl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4441 0008 	fssqrts %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5041 0008 	fssqrtw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5441 0008 	fssqrtd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5841 0008 	fssqrtb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0045      	fdsqrtd %fp0,%fp0
[ 0-9a-f]+:	f22e 4045 0008 	fdsqrtl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4445 0008 	fdsqrts %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5045 0008 	fdsqrtw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5445 0008 	fdsqrtd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5845 0008 	fdsqrtb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0018      	fabsd %fp0,%fp0
[ 0-9a-f]+:	f22e 4018 0008 	fabsl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4418 0008 	fabss %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5018 0008 	fabsw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5418 0008 	fabsd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5818 0008 	fabsb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0058      	fsabsd %fp0,%fp0
[ 0-9a-f]+:	f22e 4058 0008 	fsabsl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4458 0008 	fsabss %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5058 0008 	fsabsw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5458 0008 	fsabsd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5858 0008 	fsabsb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 005c      	fdabsd %fp0,%fp0
[ 0-9a-f]+:	f22e 405c 0008 	fdabsl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 445c 0008 	fdabss %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 505c 0008 	fdabsw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 545c 0008 	fdabsd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 585c 0008 	fdabsb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 001a      	fnegd %fp0,%fp0
[ 0-9a-f]+:	f22e 401a 0008 	fnegl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 441a 0008 	fnegs %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 501a 0008 	fnegw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 541a 0008 	fnegd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 581a 0008 	fnegb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 005a      	fsnegd %fp0,%fp0
[ 0-9a-f]+:	f22e 405a 0008 	fsnegl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 445a 0008 	fsnegs %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 505a 0008 	fsnegw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 545a 0008 	fsnegd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 585a 0008 	fsnegb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 005e      	fdnegd %fp0,%fp0
[ 0-9a-f]+:	f22e 405e 0008 	fdnegl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 445e 0008 	fdnegs %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 505e 0008 	fdnegw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 545e 0008 	fdnegd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 585e 0008 	fdnegb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0020      	fdivd %fp0,%fp0
[ 0-9a-f]+:	f22e 4020 0008 	fdivl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4420 0008 	fdivs %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5020 0008 	fdivw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5420 0008 	fdivd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5820 0008 	fdivb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0060      	fsdivd %fp0,%fp0
[ 0-9a-f]+:	f22e 4060 0008 	fsdivl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4460 0008 	fsdivs %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5060 0008 	fsdivw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5460 0008 	fsdivd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5860 0008 	fsdivb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0064      	fddivd %fp0,%fp0
[ 0-9a-f]+:	f22e 4064 0008 	fddivl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4464 0008 	fddivs %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5064 0008 	fddivw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5464 0008 	fddivd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5864 0008 	fddivb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0022      	faddd %fp0,%fp0
[ 0-9a-f]+:	f22e 4022 0008 	faddl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4422 0008 	fadds %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5022 0008 	faddw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5422 0008 	faddd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5822 0008 	faddb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0062      	fsaddd %fp0,%fp0
[ 0-9a-f]+:	f22e 4062 0008 	fsaddl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4462 0008 	fsadds %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5062 0008 	fsaddw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5462 0008 	fsaddd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5862 0008 	fsaddb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0066      	fdaddd %fp0,%fp0
[ 0-9a-f]+:	f22e 4066 0008 	fdaddl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4466 0008 	fdadds %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5066 0008 	fdaddw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5466 0008 	fdaddd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5866 0008 	fdaddb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0023      	fmuld %fp0,%fp0
[ 0-9a-f]+:	f22e 4023 0008 	fmull %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4423 0008 	fmuls %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5023 0008 	fmulw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5423 0008 	fmuld %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5823 0008 	fmulb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0063      	fsmuld %fp0,%fp0
[ 0-9a-f]+:	f22e 4063 0008 	fsmull %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4463 0008 	fsmuls %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5063 0008 	fsmulw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5463 0008 	fsmuld %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5863 0008 	fsmulb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0067      	fdmuld %fp0,%fp0
[ 0-9a-f]+:	f22e 4067 0008 	fdmull %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4467 0008 	fdmuls %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5067 0008 	fdmulw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5467 0008 	fdmuld %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5867 0008 	fdmulb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0028      	fsubd %fp0,%fp0
[ 0-9a-f]+:	f22e 4028 0008 	fsubl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4428 0008 	fsubs %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5028 0008 	fsubw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5428 0008 	fsubd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5828 0008 	fsubb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0068      	fssubd %fp0,%fp0
[ 0-9a-f]+:	f22e 4068 0008 	fssubl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4468 0008 	fssubs %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5068 0008 	fssubw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5468 0008 	fssubd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5868 0008 	fssubb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 006c      	fdsubd %fp0,%fp0
[ 0-9a-f]+:	f22e 406c 0008 	fdsubl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 446c 0008 	fdsubs %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 506c 0008 	fdsubw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 546c 0008 	fdsubd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 586c 0008 	fdsubb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0000      	fmoved %fp0,%fp0
[ 0-9a-f]+:	f22e 4000 0008 	fmovel %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4400 0008 	fmoves %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5000 0008 	fmovew %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5400 0008 	fmoved %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5800 0008 	fmoveb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0040      	fsmoved %fp0,%fp0
[ 0-9a-f]+:	f22e 4040 0008 	fsmovel %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4440 0008 	fsmoves %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5040 0008 	fsmovew %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5440 0008 	fsmoved %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5840 0008 	fsmoveb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0044      	fdmoved %fp0,%fp0
[ 0-9a-f]+:	f22e 4044 0008 	fdmovel %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4444 0008 	fdmoves %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5044 0008 	fdmovew %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5444 0008 	fdmoved %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5844 0008 	fdmoveb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0001      	fintd %fp0,%fp0
[ 0-9a-f]+:	f22e 4001 0008 	fintl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4401 0008 	fints %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5001 0008 	fintw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5401 0008 	fintd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5801 0008 	fintb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0003      	fintrzd %fp0,%fp0
[ 0-9a-f]+:	f22e 4003 0008 	fintrzl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4403 0008 	fintrzs %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5003 0008 	fintrzw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5403 0008 	fintrzd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5803 0008 	fintrzb %fp@\(8\),%fp0
[ 0-9a-f]+:	f200 0038      	fcmpd %fp0,%fp0
[ 0-9a-f]+:	f22e 4038 0008 	fcmpl %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 4438 0008 	fcmps %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5038 0008 	fcmpw %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5438 0008 	fcmpd %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e 5838 0008 	fcmpb %fp@\(8\),%fp0
[ 0-9a-f]+:	f22e f0f2 0008 	fmovemd %fp0-%fp3/%fp6,%fp@\(8\)
[ 0-9a-f]+:	f22e d02c 0008 	fmovemd %fp@\(8\),%fp2/%fp4-%fp5
[ 0-9a-f]+:	f22e f027 0008 	fmovemd %fp2/%fp5-%fp7,%fp@\(8\)
[ 0-9a-f]+:	f22e d0e1 0008 	fmovemd %fp@\(8\),%fp0-%fp2/%fp7
[ 0-9a-f]+:	f22e f0f2 0008 	fmovemd %fp0-%fp3/%fp6,%fp@\(8\)
[ 0-9a-f]+:	f22e d02c 0008 	fmovemd %fp@\(8\),%fp2/%fp4-%fp5
[ 0-9a-f]+:	f22e f027 0008 	fmovemd %fp2/%fp5-%fp7,%fp@\(8\)
[ 0-9a-f]+:	f22e d0e1 0008 	fmovemd %fp@\(8\),%fp0-%fp2/%fp7
