#objdump: -d --architecture=m68k:cfv4e
#as: -mcfv4e

.*:     file format .*

Disassembly of section .text:

0+ <.text>:
[ 0-9a-f]+:	f200 0004      	fsqrtd %fp0,%fp0
[ 0-9a-f]+:	f205 4004      	fsqrtl %d5,%fp0
[ 0-9a-f]+:	f214 4004      	fsqrtl %a4@,%fp0
[ 0-9a-f]+:	f21b 4004      	fsqrtl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4004      	fsqrtl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4004 0008 	fsqrtl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4004 1234 	fsqrtl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4404      	fsqrts %d5,%fp0
[ 0-9a-f]+:	f214 4404      	fsqrts %a4@,%fp0
[ 0-9a-f]+:	f21b 4404      	fsqrts %a3@\+,%fp0
[ 0-9a-f]+:	f222 4404      	fsqrts %a2@-,%fp0
[ 0-9a-f]+:	f22e 4404 0008 	fsqrts %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4404 1234 	fsqrts %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5004      	fsqrtw %d5,%fp0
[ 0-9a-f]+:	f214 5004      	fsqrtw %a4@,%fp0
[ 0-9a-f]+:	f21b 5004      	fsqrtw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5004      	fsqrtw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5004 0008 	fsqrtw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5004 1234 	fsqrtw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5404      	fsqrtd %a4@,%fp0
[ 0-9a-f]+:	f21b 5404      	fsqrtd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5404      	fsqrtd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5404 0008 	fsqrtd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5404 1234 	fsqrtd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5804      	fsqrtb %d5,%fp0
[ 0-9a-f]+:	f214 5804      	fsqrtb %a4@,%fp0
[ 0-9a-f]+:	f21b 5804      	fsqrtb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5804      	fsqrtb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5804 0008 	fsqrtb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5804 1234 	fsqrtb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0041      	fssqrtd %fp0,%fp0
[ 0-9a-f]+:	f205 4041      	fssqrtl %d5,%fp0
[ 0-9a-f]+:	f214 4041      	fssqrtl %a4@,%fp0
[ 0-9a-f]+:	f21b 4041      	fssqrtl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4041      	fssqrtl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4041 0008 	fssqrtl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4041 1234 	fssqrtl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4441      	fssqrts %d5,%fp0
[ 0-9a-f]+:	f214 4441      	fssqrts %a4@,%fp0
[ 0-9a-f]+:	f21b 4441      	fssqrts %a3@\+,%fp0
[ 0-9a-f]+:	f222 4441      	fssqrts %a2@-,%fp0
[ 0-9a-f]+:	f22e 4441 0008 	fssqrts %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4441 1234 	fssqrts %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5041      	fssqrtw %d5,%fp0
[ 0-9a-f]+:	f214 5041      	fssqrtw %a4@,%fp0
[ 0-9a-f]+:	f21b 5041      	fssqrtw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5041      	fssqrtw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5041 0008 	fssqrtw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5041 1234 	fssqrtw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5441      	fssqrtd %a4@,%fp0
[ 0-9a-f]+:	f21b 5441      	fssqrtd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5441      	fssqrtd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5441 0008 	fssqrtd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5441 1234 	fssqrtd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5841      	fssqrtb %d5,%fp0
[ 0-9a-f]+:	f214 5841      	fssqrtb %a4@,%fp0
[ 0-9a-f]+:	f21b 5841      	fssqrtb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5841      	fssqrtb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5841 0008 	fssqrtb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5841 1234 	fssqrtb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0045      	fdsqrtd %fp0,%fp0
[ 0-9a-f]+:	f205 4045      	fdsqrtl %d5,%fp0
[ 0-9a-f]+:	f214 4045      	fdsqrtl %a4@,%fp0
[ 0-9a-f]+:	f21b 4045      	fdsqrtl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4045      	fdsqrtl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4045 0008 	fdsqrtl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4045 1234 	fdsqrtl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4445      	fdsqrts %d5,%fp0
[ 0-9a-f]+:	f214 4445      	fdsqrts %a4@,%fp0
[ 0-9a-f]+:	f21b 4445      	fdsqrts %a3@\+,%fp0
[ 0-9a-f]+:	f222 4445      	fdsqrts %a2@-,%fp0
[ 0-9a-f]+:	f22e 4445 0008 	fdsqrts %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4445 1234 	fdsqrts %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5045      	fdsqrtw %d5,%fp0
[ 0-9a-f]+:	f214 5045      	fdsqrtw %a4@,%fp0
[ 0-9a-f]+:	f21b 5045      	fdsqrtw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5045      	fdsqrtw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5045 0008 	fdsqrtw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5045 1234 	fdsqrtw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5445      	fdsqrtd %a4@,%fp0
[ 0-9a-f]+:	f21b 5445      	fdsqrtd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5445      	fdsqrtd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5445 0008 	fdsqrtd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5445 1234 	fdsqrtd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5845      	fdsqrtb %d5,%fp0
[ 0-9a-f]+:	f214 5845      	fdsqrtb %a4@,%fp0
[ 0-9a-f]+:	f21b 5845      	fdsqrtb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5845      	fdsqrtb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5845 0008 	fdsqrtb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5845 1234 	fdsqrtb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0018      	fabsd %fp0,%fp0
[ 0-9a-f]+:	f205 4018      	fabsl %d5,%fp0
[ 0-9a-f]+:	f214 4018      	fabsl %a4@,%fp0
[ 0-9a-f]+:	f21b 4018      	fabsl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4018      	fabsl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4018 0008 	fabsl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4018 1234 	fabsl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4418      	fabss %d5,%fp0
[ 0-9a-f]+:	f214 4418      	fabss %a4@,%fp0
[ 0-9a-f]+:	f21b 4418      	fabss %a3@\+,%fp0
[ 0-9a-f]+:	f222 4418      	fabss %a2@-,%fp0
[ 0-9a-f]+:	f22e 4418 0008 	fabss %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4418 1234 	fabss %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5018      	fabsw %d5,%fp0
[ 0-9a-f]+:	f214 5018      	fabsw %a4@,%fp0
[ 0-9a-f]+:	f21b 5018      	fabsw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5018      	fabsw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5018 0008 	fabsw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5018 1234 	fabsw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5418      	fabsd %a4@,%fp0
[ 0-9a-f]+:	f21b 5418      	fabsd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5418      	fabsd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5418 0008 	fabsd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5418 1234 	fabsd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5818      	fabsb %d5,%fp0
[ 0-9a-f]+:	f214 5818      	fabsb %a4@,%fp0
[ 0-9a-f]+:	f21b 5818      	fabsb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5818      	fabsb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5818 0008 	fabsb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5818 1234 	fabsb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0058      	fsabsd %fp0,%fp0
[ 0-9a-f]+:	f205 4058      	fsabsl %d5,%fp0
[ 0-9a-f]+:	f214 4058      	fsabsl %a4@,%fp0
[ 0-9a-f]+:	f21b 4058      	fsabsl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4058      	fsabsl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4058 0008 	fsabsl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4058 1234 	fsabsl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4458      	fsabss %d5,%fp0
[ 0-9a-f]+:	f214 4458      	fsabss %a4@,%fp0
[ 0-9a-f]+:	f21b 4458      	fsabss %a3@\+,%fp0
[ 0-9a-f]+:	f222 4458      	fsabss %a2@-,%fp0
[ 0-9a-f]+:	f22e 4458 0008 	fsabss %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4458 1234 	fsabss %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5058      	fsabsw %d5,%fp0
[ 0-9a-f]+:	f214 5058      	fsabsw %a4@,%fp0
[ 0-9a-f]+:	f21b 5058      	fsabsw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5058      	fsabsw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5058 0008 	fsabsw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5058 1234 	fsabsw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5458      	fsabsd %a4@,%fp0
[ 0-9a-f]+:	f21b 5458      	fsabsd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5458      	fsabsd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5458 0008 	fsabsd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5458 1234 	fsabsd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5858      	fsabsb %d5,%fp0
[ 0-9a-f]+:	f214 5858      	fsabsb %a4@,%fp0
[ 0-9a-f]+:	f21b 5858      	fsabsb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5858      	fsabsb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5858 0008 	fsabsb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5858 1234 	fsabsb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 005c      	fdabsd %fp0,%fp0
[ 0-9a-f]+:	f205 405c      	fdabsl %d5,%fp0
[ 0-9a-f]+:	f214 405c      	fdabsl %a4@,%fp0
[ 0-9a-f]+:	f21b 405c      	fdabsl %a3@\+,%fp0
[ 0-9a-f]+:	f222 405c      	fdabsl %a2@-,%fp0
[ 0-9a-f]+:	f22e 405c 0008 	fdabsl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 405c 1234 	fdabsl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 445c      	fdabss %d5,%fp0
[ 0-9a-f]+:	f214 445c      	fdabss %a4@,%fp0
[ 0-9a-f]+:	f21b 445c      	fdabss %a3@\+,%fp0
[ 0-9a-f]+:	f222 445c      	fdabss %a2@-,%fp0
[ 0-9a-f]+:	f22e 445c 0008 	fdabss %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 445c 1234 	fdabss %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 505c      	fdabsw %d5,%fp0
[ 0-9a-f]+:	f214 505c      	fdabsw %a4@,%fp0
[ 0-9a-f]+:	f21b 505c      	fdabsw %a3@\+,%fp0
[ 0-9a-f]+:	f222 505c      	fdabsw %a2@-,%fp0
[ 0-9a-f]+:	f22e 505c 0008 	fdabsw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 505c 1234 	fdabsw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 545c      	fdabsd %a4@,%fp0
[ 0-9a-f]+:	f21b 545c      	fdabsd %a3@\+,%fp0
[ 0-9a-f]+:	f222 545c      	fdabsd %a2@-,%fp0
[ 0-9a-f]+:	f22e 545c 0008 	fdabsd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 545c 1234 	fdabsd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 585c      	fdabsb %d5,%fp0
[ 0-9a-f]+:	f214 585c      	fdabsb %a4@,%fp0
[ 0-9a-f]+:	f21b 585c      	fdabsb %a3@\+,%fp0
[ 0-9a-f]+:	f222 585c      	fdabsb %a2@-,%fp0
[ 0-9a-f]+:	f22e 585c 0008 	fdabsb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 585c 1234 	fdabsb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 001a      	fnegd %fp0,%fp0
[ 0-9a-f]+:	f205 401a      	fnegl %d5,%fp0
[ 0-9a-f]+:	f214 401a      	fnegl %a4@,%fp0
[ 0-9a-f]+:	f21b 401a      	fnegl %a3@\+,%fp0
[ 0-9a-f]+:	f222 401a      	fnegl %a2@-,%fp0
[ 0-9a-f]+:	f22e 401a 0008 	fnegl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 401a 1234 	fnegl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 441a      	fnegs %d5,%fp0
[ 0-9a-f]+:	f214 441a      	fnegs %a4@,%fp0
[ 0-9a-f]+:	f21b 441a      	fnegs %a3@\+,%fp0
[ 0-9a-f]+:	f222 441a      	fnegs %a2@-,%fp0
[ 0-9a-f]+:	f22e 441a 0008 	fnegs %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 441a 1234 	fnegs %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 501a      	fnegw %d5,%fp0
[ 0-9a-f]+:	f214 501a      	fnegw %a4@,%fp0
[ 0-9a-f]+:	f21b 501a      	fnegw %a3@\+,%fp0
[ 0-9a-f]+:	f222 501a      	fnegw %a2@-,%fp0
[ 0-9a-f]+:	f22e 501a 0008 	fnegw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 501a 1234 	fnegw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 541a      	fnegd %a4@,%fp0
[ 0-9a-f]+:	f21b 541a      	fnegd %a3@\+,%fp0
[ 0-9a-f]+:	f222 541a      	fnegd %a2@-,%fp0
[ 0-9a-f]+:	f22e 541a 0008 	fnegd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 541a 1234 	fnegd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 581a      	fnegb %d5,%fp0
[ 0-9a-f]+:	f214 581a      	fnegb %a4@,%fp0
[ 0-9a-f]+:	f21b 581a      	fnegb %a3@\+,%fp0
[ 0-9a-f]+:	f222 581a      	fnegb %a2@-,%fp0
[ 0-9a-f]+:	f22e 581a 0008 	fnegb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 581a 1234 	fnegb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 005a      	fsnegd %fp0,%fp0
[ 0-9a-f]+:	f205 405a      	fsnegl %d5,%fp0
[ 0-9a-f]+:	f214 405a      	fsnegl %a4@,%fp0
[ 0-9a-f]+:	f21b 405a      	fsnegl %a3@\+,%fp0
[ 0-9a-f]+:	f222 405a      	fsnegl %a2@-,%fp0
[ 0-9a-f]+:	f22e 405a 0008 	fsnegl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 405a 1234 	fsnegl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 445a      	fsnegs %d5,%fp0
[ 0-9a-f]+:	f214 445a      	fsnegs %a4@,%fp0
[ 0-9a-f]+:	f21b 445a      	fsnegs %a3@\+,%fp0
[ 0-9a-f]+:	f222 445a      	fsnegs %a2@-,%fp0
[ 0-9a-f]+:	f22e 445a 0008 	fsnegs %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 445a 1234 	fsnegs %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 505a      	fsnegw %d5,%fp0
[ 0-9a-f]+:	f214 505a      	fsnegw %a4@,%fp0
[ 0-9a-f]+:	f21b 505a      	fsnegw %a3@\+,%fp0
[ 0-9a-f]+:	f222 505a      	fsnegw %a2@-,%fp0
[ 0-9a-f]+:	f22e 505a 0008 	fsnegw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 505a 1234 	fsnegw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 545a      	fsnegd %a4@,%fp0
[ 0-9a-f]+:	f21b 545a      	fsnegd %a3@\+,%fp0
[ 0-9a-f]+:	f222 545a      	fsnegd %a2@-,%fp0
[ 0-9a-f]+:	f22e 545a 0008 	fsnegd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 545a 1234 	fsnegd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 585a      	fsnegb %d5,%fp0
[ 0-9a-f]+:	f214 585a      	fsnegb %a4@,%fp0
[ 0-9a-f]+:	f21b 585a      	fsnegb %a3@\+,%fp0
[ 0-9a-f]+:	f222 585a      	fsnegb %a2@-,%fp0
[ 0-9a-f]+:	f22e 585a 0008 	fsnegb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 585a 1234 	fsnegb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 005e      	fdnegd %fp0,%fp0
[ 0-9a-f]+:	f205 405e      	fdnegl %d5,%fp0
[ 0-9a-f]+:	f214 405e      	fdnegl %a4@,%fp0
[ 0-9a-f]+:	f21b 405e      	fdnegl %a3@\+,%fp0
[ 0-9a-f]+:	f222 405e      	fdnegl %a2@-,%fp0
[ 0-9a-f]+:	f22e 405e 0008 	fdnegl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 405e 1234 	fdnegl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 445e      	fdnegs %d5,%fp0
[ 0-9a-f]+:	f214 445e      	fdnegs %a4@,%fp0
[ 0-9a-f]+:	f21b 445e      	fdnegs %a3@\+,%fp0
[ 0-9a-f]+:	f222 445e      	fdnegs %a2@-,%fp0
[ 0-9a-f]+:	f22e 445e 0008 	fdnegs %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 445e 1234 	fdnegs %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 505e      	fdnegw %d5,%fp0
[ 0-9a-f]+:	f214 505e      	fdnegw %a4@,%fp0
[ 0-9a-f]+:	f21b 505e      	fdnegw %a3@\+,%fp0
[ 0-9a-f]+:	f222 505e      	fdnegw %a2@-,%fp0
[ 0-9a-f]+:	f22e 505e 0008 	fdnegw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 505e 1234 	fdnegw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 545e      	fdnegd %a4@,%fp0
[ 0-9a-f]+:	f21b 545e      	fdnegd %a3@\+,%fp0
[ 0-9a-f]+:	f222 545e      	fdnegd %a2@-,%fp0
[ 0-9a-f]+:	f22e 545e 0008 	fdnegd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 545e 1234 	fdnegd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 585e      	fdnegb %d5,%fp0
[ 0-9a-f]+:	f214 585e      	fdnegb %a4@,%fp0
[ 0-9a-f]+:	f21b 585e      	fdnegb %a3@\+,%fp0
[ 0-9a-f]+:	f222 585e      	fdnegb %a2@-,%fp0
[ 0-9a-f]+:	f22e 585e 0008 	fdnegb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 585e 1234 	fdnegb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0020      	fdivd %fp0,%fp0
[ 0-9a-f]+:	f205 4020      	fdivl %d5,%fp0
[ 0-9a-f]+:	f214 4020      	fdivl %a4@,%fp0
[ 0-9a-f]+:	f21b 4020      	fdivl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4020      	fdivl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4020 0008 	fdivl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4020 1234 	fdivl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4420      	fdivs %d5,%fp0
[ 0-9a-f]+:	f214 4420      	fdivs %a4@,%fp0
[ 0-9a-f]+:	f21b 4420      	fdivs %a3@\+,%fp0
[ 0-9a-f]+:	f222 4420      	fdivs %a2@-,%fp0
[ 0-9a-f]+:	f22e 4420 0008 	fdivs %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4420 1234 	fdivs %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5020      	fdivw %d5,%fp0
[ 0-9a-f]+:	f214 5020      	fdivw %a4@,%fp0
[ 0-9a-f]+:	f21b 5020      	fdivw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5020      	fdivw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5020 0008 	fdivw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5020 1234 	fdivw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5420      	fdivd %a4@,%fp0
[ 0-9a-f]+:	f21b 5420      	fdivd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5420      	fdivd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5420 0008 	fdivd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5420 1234 	fdivd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5820      	fdivb %d5,%fp0
[ 0-9a-f]+:	f214 5820      	fdivb %a4@,%fp0
[ 0-9a-f]+:	f21b 5820      	fdivb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5820      	fdivb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5820 0008 	fdivb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5820 1234 	fdivb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0060      	fsdivd %fp0,%fp0
[ 0-9a-f]+:	f205 4060      	fsdivl %d5,%fp0
[ 0-9a-f]+:	f214 4060      	fsdivl %a4@,%fp0
[ 0-9a-f]+:	f21b 4060      	fsdivl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4060      	fsdivl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4060 0008 	fsdivl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4060 1234 	fsdivl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4460      	fsdivs %d5,%fp0
[ 0-9a-f]+:	f214 4460      	fsdivs %a4@,%fp0
[ 0-9a-f]+:	f21b 4460      	fsdivs %a3@\+,%fp0
[ 0-9a-f]+:	f222 4460      	fsdivs %a2@-,%fp0
[ 0-9a-f]+:	f22e 4460 0008 	fsdivs %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4460 1234 	fsdivs %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5060      	fsdivw %d5,%fp0
[ 0-9a-f]+:	f214 5060      	fsdivw %a4@,%fp0
[ 0-9a-f]+:	f21b 5060      	fsdivw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5060      	fsdivw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5060 0008 	fsdivw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5060 1234 	fsdivw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5460      	fsdivd %a4@,%fp0
[ 0-9a-f]+:	f21b 5460      	fsdivd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5460      	fsdivd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5460 0008 	fsdivd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5460 1234 	fsdivd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5860      	fsdivb %d5,%fp0
[ 0-9a-f]+:	f214 5860      	fsdivb %a4@,%fp0
[ 0-9a-f]+:	f21b 5860      	fsdivb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5860      	fsdivb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5860 0008 	fsdivb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5860 1234 	fsdivb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0064      	fddivd %fp0,%fp0
[ 0-9a-f]+:	f205 4064      	fddivl %d5,%fp0
[ 0-9a-f]+:	f214 4064      	fddivl %a4@,%fp0
[ 0-9a-f]+:	f21b 4064      	fddivl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4064      	fddivl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4064 0008 	fddivl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4064 1234 	fddivl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4464      	fddivs %d5,%fp0
[ 0-9a-f]+:	f214 4464      	fddivs %a4@,%fp0
[ 0-9a-f]+:	f21b 4464      	fddivs %a3@\+,%fp0
[ 0-9a-f]+:	f222 4464      	fddivs %a2@-,%fp0
[ 0-9a-f]+:	f22e 4464 0008 	fddivs %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4464 1234 	fddivs %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5064      	fddivw %d5,%fp0
[ 0-9a-f]+:	f214 5064      	fddivw %a4@,%fp0
[ 0-9a-f]+:	f21b 5064      	fddivw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5064      	fddivw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5064 0008 	fddivw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5064 1234 	fddivw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5464      	fddivd %a4@,%fp0
[ 0-9a-f]+:	f21b 5464      	fddivd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5464      	fddivd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5464 0008 	fddivd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5464 1234 	fddivd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5864      	fddivb %d5,%fp0
[ 0-9a-f]+:	f214 5864      	fddivb %a4@,%fp0
[ 0-9a-f]+:	f21b 5864      	fddivb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5864      	fddivb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5864 0008 	fddivb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5864 1234 	fddivb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0022      	faddd %fp0,%fp0
[ 0-9a-f]+:	f205 4022      	faddl %d5,%fp0
[ 0-9a-f]+:	f214 4022      	faddl %a4@,%fp0
[ 0-9a-f]+:	f21b 4022      	faddl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4022      	faddl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4022 0008 	faddl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4022 1234 	faddl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4422      	fadds %d5,%fp0
[ 0-9a-f]+:	f214 4422      	fadds %a4@,%fp0
[ 0-9a-f]+:	f21b 4422      	fadds %a3@\+,%fp0
[ 0-9a-f]+:	f222 4422      	fadds %a2@-,%fp0
[ 0-9a-f]+:	f22e 4422 0008 	fadds %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4422 1234 	fadds %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5022      	faddw %d5,%fp0
[ 0-9a-f]+:	f214 5022      	faddw %a4@,%fp0
[ 0-9a-f]+:	f21b 5022      	faddw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5022      	faddw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5022 0008 	faddw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5022 1234 	faddw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5422      	faddd %a4@,%fp0
[ 0-9a-f]+:	f21b 5422      	faddd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5422      	faddd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5422 0008 	faddd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5422 1234 	faddd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5822      	faddb %d5,%fp0
[ 0-9a-f]+:	f214 5822      	faddb %a4@,%fp0
[ 0-9a-f]+:	f21b 5822      	faddb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5822      	faddb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5822 0008 	faddb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5822 1234 	faddb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0062      	fsaddd %fp0,%fp0
[ 0-9a-f]+:	f205 4062      	fsaddl %d5,%fp0
[ 0-9a-f]+:	f214 4062      	fsaddl %a4@,%fp0
[ 0-9a-f]+:	f21b 4062      	fsaddl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4062      	fsaddl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4062 0008 	fsaddl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4062 1234 	fsaddl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4462      	fsadds %d5,%fp0
[ 0-9a-f]+:	f214 4462      	fsadds %a4@,%fp0
[ 0-9a-f]+:	f21b 4462      	fsadds %a3@\+,%fp0
[ 0-9a-f]+:	f222 4462      	fsadds %a2@-,%fp0
[ 0-9a-f]+:	f22e 4462 0008 	fsadds %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4462 1234 	fsadds %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5062      	fsaddw %d5,%fp0
[ 0-9a-f]+:	f214 5062      	fsaddw %a4@,%fp0
[ 0-9a-f]+:	f21b 5062      	fsaddw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5062      	fsaddw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5062 0008 	fsaddw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5062 1234 	fsaddw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5462      	fsaddd %a4@,%fp0
[ 0-9a-f]+:	f21b 5462      	fsaddd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5462      	fsaddd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5462 0008 	fsaddd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5462 1234 	fsaddd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5862      	fsaddb %d5,%fp0
[ 0-9a-f]+:	f214 5862      	fsaddb %a4@,%fp0
[ 0-9a-f]+:	f21b 5862      	fsaddb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5862      	fsaddb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5862 0008 	fsaddb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5862 1234 	fsaddb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0066      	fdaddd %fp0,%fp0
[ 0-9a-f]+:	f205 4066      	fdaddl %d5,%fp0
[ 0-9a-f]+:	f214 4066      	fdaddl %a4@,%fp0
[ 0-9a-f]+:	f21b 4066      	fdaddl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4066      	fdaddl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4066 0008 	fdaddl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4066 1234 	fdaddl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4466      	fdadds %d5,%fp0
[ 0-9a-f]+:	f214 4466      	fdadds %a4@,%fp0
[ 0-9a-f]+:	f21b 4466      	fdadds %a3@\+,%fp0
[ 0-9a-f]+:	f222 4466      	fdadds %a2@-,%fp0
[ 0-9a-f]+:	f22e 4466 0008 	fdadds %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4466 1234 	fdadds %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5066      	fdaddw %d5,%fp0
[ 0-9a-f]+:	f214 5066      	fdaddw %a4@,%fp0
[ 0-9a-f]+:	f21b 5066      	fdaddw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5066      	fdaddw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5066 0008 	fdaddw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5066 1234 	fdaddw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5466      	fdaddd %a4@,%fp0
[ 0-9a-f]+:	f21b 5466      	fdaddd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5466      	fdaddd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5466 0008 	fdaddd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5466 1234 	fdaddd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5866      	fdaddb %d5,%fp0
[ 0-9a-f]+:	f214 5866      	fdaddb %a4@,%fp0
[ 0-9a-f]+:	f21b 5866      	fdaddb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5866      	fdaddb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5866 0008 	fdaddb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5866 1234 	fdaddb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0023      	fmuld %fp0,%fp0
[ 0-9a-f]+:	f205 4023      	fmull %d5,%fp0
[ 0-9a-f]+:	f214 4023      	fmull %a4@,%fp0
[ 0-9a-f]+:	f21b 4023      	fmull %a3@\+,%fp0
[ 0-9a-f]+:	f222 4023      	fmull %a2@-,%fp0
[ 0-9a-f]+:	f22e 4023 0008 	fmull %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4023 1234 	fmull %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4423      	fmuls %d5,%fp0
[ 0-9a-f]+:	f214 4423      	fmuls %a4@,%fp0
[ 0-9a-f]+:	f21b 4423      	fmuls %a3@\+,%fp0
[ 0-9a-f]+:	f222 4423      	fmuls %a2@-,%fp0
[ 0-9a-f]+:	f22e 4423 0008 	fmuls %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4423 1234 	fmuls %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5023      	fmulw %d5,%fp0
[ 0-9a-f]+:	f214 5023      	fmulw %a4@,%fp0
[ 0-9a-f]+:	f21b 5023      	fmulw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5023      	fmulw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5023 0008 	fmulw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5023 1234 	fmulw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5423      	fmuld %a4@,%fp0
[ 0-9a-f]+:	f21b 5423      	fmuld %a3@\+,%fp0
[ 0-9a-f]+:	f222 5423      	fmuld %a2@-,%fp0
[ 0-9a-f]+:	f22e 5423 0008 	fmuld %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5423 1234 	fmuld %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5823      	fmulb %d5,%fp0
[ 0-9a-f]+:	f214 5823      	fmulb %a4@,%fp0
[ 0-9a-f]+:	f21b 5823      	fmulb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5823      	fmulb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5823 0008 	fmulb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5823 1234 	fmulb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0063      	fsmuld %fp0,%fp0
[ 0-9a-f]+:	f205 4063      	fsmull %d5,%fp0
[ 0-9a-f]+:	f214 4063      	fsmull %a4@,%fp0
[ 0-9a-f]+:	f21b 4063      	fsmull %a3@\+,%fp0
[ 0-9a-f]+:	f222 4063      	fsmull %a2@-,%fp0
[ 0-9a-f]+:	f22e 4063 0008 	fsmull %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4063 1234 	fsmull %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4463      	fsmuls %d5,%fp0
[ 0-9a-f]+:	f214 4463      	fsmuls %a4@,%fp0
[ 0-9a-f]+:	f21b 4463      	fsmuls %a3@\+,%fp0
[ 0-9a-f]+:	f222 4463      	fsmuls %a2@-,%fp0
[ 0-9a-f]+:	f22e 4463 0008 	fsmuls %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4463 1234 	fsmuls %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5063      	fsmulw %d5,%fp0
[ 0-9a-f]+:	f214 5063      	fsmulw %a4@,%fp0
[ 0-9a-f]+:	f21b 5063      	fsmulw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5063      	fsmulw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5063 0008 	fsmulw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5063 1234 	fsmulw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5463      	fsmuld %a4@,%fp0
[ 0-9a-f]+:	f21b 5463      	fsmuld %a3@\+,%fp0
[ 0-9a-f]+:	f222 5463      	fsmuld %a2@-,%fp0
[ 0-9a-f]+:	f22e 5463 0008 	fsmuld %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5463 1234 	fsmuld %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5863      	fsmulb %d5,%fp0
[ 0-9a-f]+:	f214 5863      	fsmulb %a4@,%fp0
[ 0-9a-f]+:	f21b 5863      	fsmulb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5863      	fsmulb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5863 0008 	fsmulb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5863 1234 	fsmulb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0067      	fdmuld %fp0,%fp0
[ 0-9a-f]+:	f205 4067      	fdmull %d5,%fp0
[ 0-9a-f]+:	f214 4067      	fdmull %a4@,%fp0
[ 0-9a-f]+:	f21b 4067      	fdmull %a3@\+,%fp0
[ 0-9a-f]+:	f222 4067      	fdmull %a2@-,%fp0
[ 0-9a-f]+:	f22e 4067 0008 	fdmull %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4067 1234 	fdmull %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4467      	fdmuls %d5,%fp0
[ 0-9a-f]+:	f214 4467      	fdmuls %a4@,%fp0
[ 0-9a-f]+:	f21b 4467      	fdmuls %a3@\+,%fp0
[ 0-9a-f]+:	f222 4467      	fdmuls %a2@-,%fp0
[ 0-9a-f]+:	f22e 4467 0008 	fdmuls %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4467 1234 	fdmuls %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5067      	fdmulw %d5,%fp0
[ 0-9a-f]+:	f214 5067      	fdmulw %a4@,%fp0
[ 0-9a-f]+:	f21b 5067      	fdmulw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5067      	fdmulw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5067 0008 	fdmulw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5067 1234 	fdmulw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5467      	fdmuld %a4@,%fp0
[ 0-9a-f]+:	f21b 5467      	fdmuld %a3@\+,%fp0
[ 0-9a-f]+:	f222 5467      	fdmuld %a2@-,%fp0
[ 0-9a-f]+:	f22e 5467 0008 	fdmuld %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5467 1234 	fdmuld %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5867      	fdmulb %d5,%fp0
[ 0-9a-f]+:	f214 5867      	fdmulb %a4@,%fp0
[ 0-9a-f]+:	f21b 5867      	fdmulb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5867      	fdmulb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5867 0008 	fdmulb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5867 1234 	fdmulb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0028      	fsubd %fp0,%fp0
[ 0-9a-f]+:	f205 4028      	fsubl %d5,%fp0
[ 0-9a-f]+:	f214 4028      	fsubl %a4@,%fp0
[ 0-9a-f]+:	f21b 4028      	fsubl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4028      	fsubl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4028 0008 	fsubl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4028 1234 	fsubl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4428      	fsubs %d5,%fp0
[ 0-9a-f]+:	f214 4428      	fsubs %a4@,%fp0
[ 0-9a-f]+:	f21b 4428      	fsubs %a3@\+,%fp0
[ 0-9a-f]+:	f222 4428      	fsubs %a2@-,%fp0
[ 0-9a-f]+:	f22e 4428 0008 	fsubs %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4428 1234 	fsubs %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5028      	fsubw %d5,%fp0
[ 0-9a-f]+:	f214 5028      	fsubw %a4@,%fp0
[ 0-9a-f]+:	f21b 5028      	fsubw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5028      	fsubw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5028 0008 	fsubw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5028 1234 	fsubw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5428      	fsubd %a4@,%fp0
[ 0-9a-f]+:	f21b 5428      	fsubd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5428      	fsubd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5428 0008 	fsubd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5428 1234 	fsubd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5828      	fsubb %d5,%fp0
[ 0-9a-f]+:	f214 5828      	fsubb %a4@,%fp0
[ 0-9a-f]+:	f21b 5828      	fsubb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5828      	fsubb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5828 0008 	fsubb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5828 1234 	fsubb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0068      	fssubd %fp0,%fp0
[ 0-9a-f]+:	f205 4068      	fssubl %d5,%fp0
[ 0-9a-f]+:	f214 4068      	fssubl %a4@,%fp0
[ 0-9a-f]+:	f21b 4068      	fssubl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4068      	fssubl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4068 0008 	fssubl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4068 1234 	fssubl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4468      	fssubs %d5,%fp0
[ 0-9a-f]+:	f214 4468      	fssubs %a4@,%fp0
[ 0-9a-f]+:	f21b 4468      	fssubs %a3@\+,%fp0
[ 0-9a-f]+:	f222 4468      	fssubs %a2@-,%fp0
[ 0-9a-f]+:	f22e 4468 0008 	fssubs %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4468 1234 	fssubs %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5068      	fssubw %d5,%fp0
[ 0-9a-f]+:	f214 5068      	fssubw %a4@,%fp0
[ 0-9a-f]+:	f21b 5068      	fssubw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5068      	fssubw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5068 0008 	fssubw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5068 1234 	fssubw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5468      	fssubd %a4@,%fp0
[ 0-9a-f]+:	f21b 5468      	fssubd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5468      	fssubd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5468 0008 	fssubd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5468 1234 	fssubd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5868      	fssubb %d5,%fp0
[ 0-9a-f]+:	f214 5868      	fssubb %a4@,%fp0
[ 0-9a-f]+:	f21b 5868      	fssubb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5868      	fssubb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5868 0008 	fssubb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5868 1234 	fssubb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 006c      	fdsubd %fp0,%fp0
[ 0-9a-f]+:	f205 406c      	fdsubl %d5,%fp0
[ 0-9a-f]+:	f214 406c      	fdsubl %a4@,%fp0
[ 0-9a-f]+:	f21b 406c      	fdsubl %a3@\+,%fp0
[ 0-9a-f]+:	f222 406c      	fdsubl %a2@-,%fp0
[ 0-9a-f]+:	f22e 406c 0008 	fdsubl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 406c 1234 	fdsubl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 446c      	fdsubs %d5,%fp0
[ 0-9a-f]+:	f214 446c      	fdsubs %a4@,%fp0
[ 0-9a-f]+:	f21b 446c      	fdsubs %a3@\+,%fp0
[ 0-9a-f]+:	f222 446c      	fdsubs %a2@-,%fp0
[ 0-9a-f]+:	f22e 446c 0008 	fdsubs %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 446c 1234 	fdsubs %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 506c      	fdsubw %d5,%fp0
[ 0-9a-f]+:	f214 506c      	fdsubw %a4@,%fp0
[ 0-9a-f]+:	f21b 506c      	fdsubw %a3@\+,%fp0
[ 0-9a-f]+:	f222 506c      	fdsubw %a2@-,%fp0
[ 0-9a-f]+:	f22e 506c 0008 	fdsubw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 506c 1234 	fdsubw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 546c      	fdsubd %a4@,%fp0
[ 0-9a-f]+:	f21b 546c      	fdsubd %a3@\+,%fp0
[ 0-9a-f]+:	f222 546c      	fdsubd %a2@-,%fp0
[ 0-9a-f]+:	f22e 546c 0008 	fdsubd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 546c 1234 	fdsubd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 586c      	fdsubb %d5,%fp0
[ 0-9a-f]+:	f214 586c      	fdsubb %a4@,%fp0
[ 0-9a-f]+:	f21b 586c      	fdsubb %a3@\+,%fp0
[ 0-9a-f]+:	f222 586c      	fdsubb %a2@-,%fp0
[ 0-9a-f]+:	f22e 586c 0008 	fdsubb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 586c 1234 	fdsubb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0000      	fmoved %fp0,%fp0
[ 0-9a-f]+:	f205 4000      	fmovel %d5,%fp0
[ 0-9a-f]+:	f214 4000      	fmovel %a4@,%fp0
[ 0-9a-f]+:	f21b 4000      	fmovel %a3@\+,%fp0
[ 0-9a-f]+:	f222 4000      	fmovel %a2@-,%fp0
[ 0-9a-f]+:	f22e 4000 0008 	fmovel %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4000 1234 	fmovel %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4400      	fmoves %d5,%fp0
[ 0-9a-f]+:	f214 4400      	fmoves %a4@,%fp0
[ 0-9a-f]+:	f21b 4400      	fmoves %a3@\+,%fp0
[ 0-9a-f]+:	f222 4400      	fmoves %a2@-,%fp0
[ 0-9a-f]+:	f22e 4400 0008 	fmoves %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4400 1234 	fmoves %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5000      	fmovew %d5,%fp0
[ 0-9a-f]+:	f214 5000      	fmovew %a4@,%fp0
[ 0-9a-f]+:	f21b 5000      	fmovew %a3@\+,%fp0
[ 0-9a-f]+:	f222 5000      	fmovew %a2@-,%fp0
[ 0-9a-f]+:	f22e 5000 0008 	fmovew %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5000 1234 	fmovew %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5400      	fmoved %a4@,%fp0
[ 0-9a-f]+:	f21b 5400      	fmoved %a3@\+,%fp0
[ 0-9a-f]+:	f222 5400      	fmoved %a2@-,%fp0
[ 0-9a-f]+:	f22e 5400 0008 	fmoved %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5400 1234 	fmoved %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5800      	fmoveb %d5,%fp0
[ 0-9a-f]+:	f214 5800      	fmoveb %a4@,%fp0
[ 0-9a-f]+:	f21b 5800      	fmoveb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5800      	fmoveb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5800 0008 	fmoveb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5800 1234 	fmoveb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0040      	fsmoved %fp0,%fp0
[ 0-9a-f]+:	f205 4040      	fsmovel %d5,%fp0
[ 0-9a-f]+:	f214 4040      	fsmovel %a4@,%fp0
[ 0-9a-f]+:	f21b 4040      	fsmovel %a3@\+,%fp0
[ 0-9a-f]+:	f222 4040      	fsmovel %a2@-,%fp0
[ 0-9a-f]+:	f22e 4040 0008 	fsmovel %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4040 1234 	fsmovel %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4440      	fsmoves %d5,%fp0
[ 0-9a-f]+:	f214 4440      	fsmoves %a4@,%fp0
[ 0-9a-f]+:	f21b 4440      	fsmoves %a3@\+,%fp0
[ 0-9a-f]+:	f222 4440      	fsmoves %a2@-,%fp0
[ 0-9a-f]+:	f22e 4440 0008 	fsmoves %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4440 1234 	fsmoves %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5040      	fsmovew %d5,%fp0
[ 0-9a-f]+:	f214 5040      	fsmovew %a4@,%fp0
[ 0-9a-f]+:	f21b 5040      	fsmovew %a3@\+,%fp0
[ 0-9a-f]+:	f222 5040      	fsmovew %a2@-,%fp0
[ 0-9a-f]+:	f22e 5040 0008 	fsmovew %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5040 1234 	fsmovew %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5440      	fsmoved %a4@,%fp0
[ 0-9a-f]+:	f21b 5440      	fsmoved %a3@\+,%fp0
[ 0-9a-f]+:	f222 5440      	fsmoved %a2@-,%fp0
[ 0-9a-f]+:	f22e 5440 0008 	fsmoved %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5440 1234 	fsmoved %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5840      	fsmoveb %d5,%fp0
[ 0-9a-f]+:	f214 5840      	fsmoveb %a4@,%fp0
[ 0-9a-f]+:	f21b 5840      	fsmoveb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5840      	fsmoveb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5840 0008 	fsmoveb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5840 1234 	fsmoveb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0044      	fdmoved %fp0,%fp0
[ 0-9a-f]+:	f205 4044      	fdmovel %d5,%fp0
[ 0-9a-f]+:	f214 4044      	fdmovel %a4@,%fp0
[ 0-9a-f]+:	f21b 4044      	fdmovel %a3@\+,%fp0
[ 0-9a-f]+:	f222 4044      	fdmovel %a2@-,%fp0
[ 0-9a-f]+:	f22e 4044 0008 	fdmovel %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4044 1234 	fdmovel %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4444      	fdmoves %d5,%fp0
[ 0-9a-f]+:	f214 4444      	fdmoves %a4@,%fp0
[ 0-9a-f]+:	f21b 4444      	fdmoves %a3@\+,%fp0
[ 0-9a-f]+:	f222 4444      	fdmoves %a2@-,%fp0
[ 0-9a-f]+:	f22e 4444 0008 	fdmoves %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4444 1234 	fdmoves %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5044      	fdmovew %d5,%fp0
[ 0-9a-f]+:	f214 5044      	fdmovew %a4@,%fp0
[ 0-9a-f]+:	f21b 5044      	fdmovew %a3@\+,%fp0
[ 0-9a-f]+:	f222 5044      	fdmovew %a2@-,%fp0
[ 0-9a-f]+:	f22e 5044 0008 	fdmovew %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5044 1234 	fdmovew %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5444      	fdmoved %a4@,%fp0
[ 0-9a-f]+:	f21b 5444      	fdmoved %a3@\+,%fp0
[ 0-9a-f]+:	f222 5444      	fdmoved %a2@-,%fp0
[ 0-9a-f]+:	f22e 5444 0008 	fdmoved %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5444 1234 	fdmoved %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5844      	fdmoveb %d5,%fp0
[ 0-9a-f]+:	f214 5844      	fdmoveb %a4@,%fp0
[ 0-9a-f]+:	f21b 5844      	fdmoveb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5844      	fdmoveb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5844 0008 	fdmoveb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5844 1234 	fdmoveb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0001      	fintd %fp0,%fp0
[ 0-9a-f]+:	f205 4001      	fintl %d5,%fp0
[ 0-9a-f]+:	f214 4001      	fintl %a4@,%fp0
[ 0-9a-f]+:	f21b 4001      	fintl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4001      	fintl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4001 0008 	fintl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4001 1234 	fintl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4401      	fints %d5,%fp0
[ 0-9a-f]+:	f214 4401      	fints %a4@,%fp0
[ 0-9a-f]+:	f21b 4401      	fints %a3@\+,%fp0
[ 0-9a-f]+:	f222 4401      	fints %a2@-,%fp0
[ 0-9a-f]+:	f22e 4401 0008 	fints %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4401 1234 	fints %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5001      	fintw %d5,%fp0
[ 0-9a-f]+:	f214 5001      	fintw %a4@,%fp0
[ 0-9a-f]+:	f21b 5001      	fintw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5001      	fintw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5001 0008 	fintw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5001 1234 	fintw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5401      	fintd %a4@,%fp0
[ 0-9a-f]+:	f21b 5401      	fintd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5401      	fintd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5401 0008 	fintd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5401 1234 	fintd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5801      	fintb %d5,%fp0
[ 0-9a-f]+:	f214 5801      	fintb %a4@,%fp0
[ 0-9a-f]+:	f21b 5801      	fintb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5801      	fintb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5801 0008 	fintb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5801 1234 	fintb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0003      	fintrzd %fp0,%fp0
[ 0-9a-f]+:	f205 4003      	fintrzl %d5,%fp0
[ 0-9a-f]+:	f214 4003      	fintrzl %a4@,%fp0
[ 0-9a-f]+:	f21b 4003      	fintrzl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4003      	fintrzl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4003 0008 	fintrzl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4003 1234 	fintrzl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4403      	fintrzs %d5,%fp0
[ 0-9a-f]+:	f214 4403      	fintrzs %a4@,%fp0
[ 0-9a-f]+:	f21b 4403      	fintrzs %a3@\+,%fp0
[ 0-9a-f]+:	f222 4403      	fintrzs %a2@-,%fp0
[ 0-9a-f]+:	f22e 4403 0008 	fintrzs %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4403 1234 	fintrzs %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5003      	fintrzw %d5,%fp0
[ 0-9a-f]+:	f214 5003      	fintrzw %a4@,%fp0
[ 0-9a-f]+:	f21b 5003      	fintrzw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5003      	fintrzw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5003 0008 	fintrzw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5003 1234 	fintrzw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5403      	fintrzd %a4@,%fp0
[ 0-9a-f]+:	f21b 5403      	fintrzd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5403      	fintrzd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5403 0008 	fintrzd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5403 1234 	fintrzd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5803      	fintrzb %d5,%fp0
[ 0-9a-f]+:	f214 5803      	fintrzb %a4@,%fp0
[ 0-9a-f]+:	f21b 5803      	fintrzb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5803      	fintrzb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5803 0008 	fintrzb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5803 1234 	fintrzb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f200 0038      	fcmpd %fp0,%fp0
[ 0-9a-f]+:	f205 4038      	fcmpl %d5,%fp0
[ 0-9a-f]+:	f214 4038      	fcmpl %a4@,%fp0
[ 0-9a-f]+:	f21b 4038      	fcmpl %a3@\+,%fp0
[ 0-9a-f]+:	f222 4038      	fcmpl %a2@-,%fp0
[ 0-9a-f]+:	f22e 4038 0008 	fcmpl %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4038 1234 	fcmpl %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 4438      	fcmps %d5,%fp0
[ 0-9a-f]+:	f214 4438      	fcmps %a4@,%fp0
[ 0-9a-f]+:	f21b 4438      	fcmps %a3@\+,%fp0
[ 0-9a-f]+:	f222 4438      	fcmps %a2@-,%fp0
[ 0-9a-f]+:	f22e 4438 0008 	fcmps %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 4438 1234 	fcmps %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5038      	fcmpw %d5,%fp0
[ 0-9a-f]+:	f214 5038      	fcmpw %a4@,%fp0
[ 0-9a-f]+:	f21b 5038      	fcmpw %a3@\+,%fp0
[ 0-9a-f]+:	f222 5038      	fcmpw %a2@-,%fp0
[ 0-9a-f]+:	f22e 5038 0008 	fcmpw %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5038 1234 	fcmpw %pc@\(.*\),%fp0
[ 0-9a-f]+:	f214 5438      	fcmpd %a4@,%fp0
[ 0-9a-f]+:	f21b 5438      	fcmpd %a3@\+,%fp0
[ 0-9a-f]+:	f222 5438      	fcmpd %a2@-,%fp0
[ 0-9a-f]+:	f22e 5438 0008 	fcmpd %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5438 1234 	fcmpd %pc@\(.*\),%fp0
[ 0-9a-f]+:	f205 5838      	fcmpb %d5,%fp0
[ 0-9a-f]+:	f214 5838      	fcmpb %a4@,%fp0
[ 0-9a-f]+:	f21b 5838      	fcmpb %a3@\+,%fp0
[ 0-9a-f]+:	f222 5838      	fcmpb %a2@-,%fp0
[ 0-9a-f]+:	f22e 5838 0008 	fcmpb %fp@\(8\),%fp0
[ 0-9a-f]+:	f23a 5838 1234 	fcmpb %pc@\(.*\),%fp0
[ 0-9a-f]+:	f22e f0f2 0008 	fmovemd %fp0-%fp3/%fp6,%fp@\(8\)
[ 0-9a-f]+:	f22e d02c 0008 	fmovemd %fp@\(8\),%fp2/%fp4-%fp5
[ 0-9a-f]+:	f22e f027 0008 	fmovemd %fp2/%fp5-%fp7,%fp@\(8\)
[ 0-9a-f]+:	f22e d0e1 0008 	fmovemd %fp@\(8\),%fp0-%fp2/%fp7
[ 0-9a-f]+:	f22e f0f2 0008 	fmovemd %fp0-%fp3/%fp6,%fp@\(8\)
[ 0-9a-f]+:	f22e d02c 0008 	fmovemd %fp@\(8\),%fp2/%fp4-%fp5
[ 0-9a-f]+:	f22e f027 0008 	fmovemd %fp2/%fp5-%fp7,%fp@\(8\)
[ 0-9a-f]+:	f22e d0e1 0008 	fmovemd %fp@\(8\),%fp0-%fp2/%fp7
