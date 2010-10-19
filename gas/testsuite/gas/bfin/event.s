	.text
	.global idle
idle:
	IDle;

	.text
	.global csync
csync:
	csync;

	.text
	.global ssync
ssync:
	SSYNC;

	.text
	.global emuexcpt
emuexcpt:
	EMuExCpt;

	.text
	.global cli
cli:
	cli r7;
	CLI R0;

	.text
	.global sti
sti:
	STI r1;
	stI r2;

	.text
	.global raise
raise:
	raise 15;
	RAISE 0;

	.text
	.global excpt
excpt:
	excpt 15;
	EXCPT 0;
	
	.text
	.global testset
testset:
	testset(p5);
	TESTset (P0);

	.text
	.global nop
nop:
	nop;
	MNOP;

