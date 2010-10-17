#objdump: -dr
#name: break

.*:[ 	]+file format .*-cris

Disassembly of section \.text:
0+ <start>:
[	 ]+0:[	 ]+32e9[ 	]+break[ ]+2
[	 ]+2:[	 ]+30e9[ 	]+break[ ]+0
[	 ]+4:[	 ]+31e9[ 	]+break[ ]+1
[	 ]+6:[	 ]+32e9[ 	]+break[ ]+2
[	 ]+8:[	 ]+33e9[ 	]+break[ ]+3
[	 ]+a:[	 ]+34e9[ 	]+break[ ]+4
[	 ]+c:[	 ]+35e9[ 	]+break[ ]+5
[	 ]+e:[	 ]+36e9[ 	]+break[ ]+6
[	 ]+10:[	 ]+37e9[ 	]+break[ ]+7
[	 ]+12:[	 ]+38e9[ 	]+break[ ]+8
[	 ]+14:[	 ]+39e9[ 	]+break[ ]+9
[	 ]+16:[	 ]+3ae9[ 	]+break[ ]+10
[	 ]+18:[	 ]+3be9[ 	]+break[ ]+11
[	 ]+1a:[	 ]+3ce9[ 	]+break[ ]+12
[	 ]+1c:[	 ]+3de9[ 	]+break[ ]+13
[	 ]+1e:[	 ]+3ee9[ 	]+break[ ]+14
[	 ]+20:[	 ]+3fe9[ 	]+break[ ]+15

0+22 <end>:
	\.\.\.
