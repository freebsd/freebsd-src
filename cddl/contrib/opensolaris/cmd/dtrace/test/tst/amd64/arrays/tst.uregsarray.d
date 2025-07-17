/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2019 Mariusz Zaborski <oshogbo@FreeBSD.org>
 * Use is subject to license terms.
 */

/*
 * ASSERTION:
 *	Positive test to make sure that we can invoke amd64
 *	ureg[] aliases.
 *
 * SECTION: User Process Tracing/uregs Array
 *
 * NOTES: This test does no verification - the value of the output
 *	is not deterministic.
 */

#pragma D option quiet

BEGIN
{
	printf("R_GS = 0x%x\n", uregs[R_GS]);
	printf("R_ES = 0x%x\n", uregs[R_ES]);
	printf("R_DS = 0x%x\n", uregs[R_DS]);
	printf("R_CS = 0x%x\n", uregs[R_CS]);
	printf("R_RFL = 0x%x\n", uregs[R_RFL]);
	printf("R_SS = 0x%x\n", uregs[R_SS]);
	printf("R_TRAPNO = 0x%x\n", uregs[R_TRAPNO]);

	printf("R_URSP = 0x%x\n", uregs[R_RSP]);
	printf("R_RDI = 0x%x\n", uregs[R_RDI]);
	printf("R_RSI = 0x%x\n", uregs[R_RSI]);
	printf("R_RBP = 0x%x\n", uregs[R_RBP]);
	printf("R_RBX = 0x%x\n", uregs[R_RBX]);
	printf("R_RDX = 0x%x\n", uregs[R_RDX]);
	printf("R_RCX = 0x%x\n", uregs[R_RCX]);
	printf("R_RAX = 0x%x\n", uregs[R_RAX]);
	printf("R_RIP = 0x%x\n", uregs[R_RIP]);
	printf("R_RDI = 0x%x\n", uregs[R_RDI]);
	printf("R_R9 = 0x%x\n", uregs[R_R9]);
	printf("R_R10 = 0x%x\n", uregs[R_R10]);
	printf("R_R11 = 0x%x\n", uregs[R_R11]);
	printf("R_R12 = 0x%x\n", uregs[R_R12]);
	printf("R_R13 = 0x%x\n", uregs[R_R13]);
	printf("R_R14 = 0x%x\n", uregs[R_R14]);
	printf("R_R15 = 0x%x\n", uregs[R_R15]);

	/* 32 bits */
	printf("R_EFL = 0x%x\n", uregs[R_EFL]);
	printf("R_UESP = 0x%x\n", uregs[R_UESP]);
	printf("R_ERR = 0x%x\n", uregs[R_ERR]);
	printf("R_EIP = 0x%x\n", uregs[R_EIP]);
	printf("R_EDI = 0x%x\n", uregs[R_EDI]);
	printf("R_ESI = 0x%x\n", uregs[R_ESI]);
	printf("R_EBP = 0x%x\n", uregs[R_EBP]);
	printf("R_EBX = 0x%x\n", uregs[R_EBX]);
	printf("R_EDX = 0x%x\n", uregs[R_EDX]);
	printf("R_ECX = 0x%x\n", uregs[R_ECX]);
	printf("R_EAX = 0x%x\n", uregs[R_EAX]);

	exit(0);
}
