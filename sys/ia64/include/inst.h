/*-
 * Copyright (c) 2000 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#ifndef _MACHINE_INST_H_
#define _MACHINE_INST_H_

union ia64_instruction {
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;

		u_int64_t	x4	:4;
		u_int64_t	v	:1;
		u_int64_t	x2a	:2;
		u_int64_t	resv	:1;
		u_int64_t	op	:4;
	} A1;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	ct2d	:2;
		u_int64_t	x4	:4;
		u_int64_t	v	:1;
		u_int64_t	x2a	:2;
		u_int64_t	resv	:1;
		u_int64_t	op	:4;
	} A2;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	imm7b	:7;
		u_int64_t	r3	:7;
		u_int64_t	x2b	:2;
		u_int64_t	x4	:4;
		u_int64_t	v	:1;
		u_int64_t	x2a	:2;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} A3;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	imm7b	:7;
		u_int64_t	r3	:7;
		u_int64_t	imm6d	:6;
		u_int64_t	v	:1;
		u_int64_t	x2a	:2;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} A4;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	imm7b	:7;
		u_int64_t	r3	:2;
		u_int64_t	imm5c	:5;
		u_int64_t	imm9d	:9;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} A5;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	p1	:6;
		u_int64_t	c	:1;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	p2	:6;
		u_int64_t	ta	:1;
		u_int64_t	x2	:2;
		u_int64_t	tb	:1;
		u_int64_t	op	:4;
	} A6;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	p1	:6;
		u_int64_t	c	:1;
		u_int64_t	zero	:7;
		u_int64_t	r3	:7;
		u_int64_t	p2	:6;
		u_int64_t	ta	:1;
		u_int64_t	x2	:2;
		u_int64_t	tb	:1;
		u_int64_t	op	:4;
	} A7;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	p1	:6;
		u_int64_t	c	:1;
		u_int64_t	imm7b	:7;
		u_int64_t	r3	:7;
		u_int64_t	p2	:6;
		u_int64_t	ta	:1;
		u_int64_t	x2	:2;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} A8;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	x2b	:2;
		u_int64_t	x4	:4;
		u_int64_t	zb	:1;
		u_int64_t	x2a	:2;
		u_int64_t	za	:1;
		u_int64_t	op	:4;
	} A9;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	ct2d	:2;
		u_int64_t	x4	:4;
		u_int64_t	zb	:1;
		u_int64_t	x2a	:2;
		u_int64_t	za	:1;
		u_int64_t	op	:4;
	} A10;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	resv	:1;
		u_int64_t	x2b	:2;
		u_int64_t	ct2d	:2;
		u_int64_t	ve	:1;
		u_int64_t	zb	:1;
		u_int64_t	x2a	:2;
		u_int64_t	za	:1;
		u_int64_t	op	:4;
	} I1;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	resv	:1;
		u_int64_t	x2b	:2;
		u_int64_t	x2c	:2;
		u_int64_t	ve	:1;
		u_int64_t	zb	:1;
		u_int64_t	x2a	:2;
		u_int64_t	za	:1;
		u_int64_t	op	:4;
	} I2;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	mbt4c	:4;
		u_int64_t	resv	:4;
		u_int64_t	x2b	:2;
		u_int64_t	x2c	:2;
		u_int64_t	ve	:1;
		u_int64_t	zb	:1;
		u_int64_t	x2a	:2;
		u_int64_t	za	:1;
		u_int64_t	op	:4;
	} I3;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	mht8c	:8;
		u_int64_t	x2b	:2;
		u_int64_t	x2c	:2;
		u_int64_t	ve	:1;
		u_int64_t	zb	:1;
		u_int64_t	x2a	:2;
		u_int64_t	za	:1;
		u_int64_t	op	:4;
	} I4;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	resv	:1;
		u_int64_t	x2b	:2;
		u_int64_t	x2c	:2;
		u_int64_t	ve	:1;
		u_int64_t	zb	:1;
		u_int64_t	x2a	:2;
		u_int64_t	za	:1;
		u_int64_t	op	:4;
	} I5;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	resv1	:1;
		u_int64_t	count5b	:5;
		u_int64_t	resv2	:1;
		u_int64_t	r3	:7;
		u_int64_t	resv3	:1;
		u_int64_t	x2b	:2;
		u_int64_t	x2c	:2;
		u_int64_t	ve	:1;
		u_int64_t	zb	:1;
		u_int64_t	x2a	:2;
		u_int64_t	za	:1;
		u_int64_t	op	:4;
	} I6;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	resv	:1;
		u_int64_t	x2b	:2;
		u_int64_t	x2c	:2;
		u_int64_t	ve	:1;
		u_int64_t	zb	:1;
		u_int64_t	x2a	:2;
		u_int64_t	za	:1;
		u_int64_t	op	:4;
	} I7;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	count5c	:5;
		u_int64_t	resv	:2;
		u_int64_t	x2b	:2;
		u_int64_t	x2c	:2;
		u_int64_t	ve	:1;
		u_int64_t	zb	:1;
		u_int64_t	x2a	:2;
		u_int64_t	za	:1;
		u_int64_t	op	:4;
	} I8;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	zero	:7;
		u_int64_t	r3	:7;
		u_int64_t	resv	:1;
		u_int64_t	x2b	:2;
		u_int64_t	x2c	:2;
		u_int64_t	ve	:1;
		u_int64_t	zb	:1;
		u_int64_t	x2a	:2;
		u_int64_t	za	:1;
		u_int64_t	op	:4;
	} I9;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	count6d	:6;
		u_int64_t	x	:1;
		u_int64_t	x2	:2;
		u_int64_t	resv	:1;
		u_int64_t	op	:4;
	} I10;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	y	:1;
		u_int64_t	pos6b	:6;
		u_int64_t	r3	:7;
		u_int64_t	len6d	:6;
		u_int64_t	x	:1;
		u_int64_t	x2	:2;
		u_int64_t	resv	:1;
		u_int64_t	op	:4;
	} I11;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	cpos6c	:6;
		u_int64_t	y	:1;
		u_int64_t	len6d	:6;
		u_int64_t	x	:1;
		u_int64_t	x2	:2;
		u_int64_t	resv	:1;
		u_int64_t	op	:4;
	} I12;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	imm7b	:7;
		u_int64_t	cpos6c	:6;
		u_int64_t	y	:1;
		u_int64_t	len6d	:6;
		u_int64_t	x	:1;
		u_int64_t	x2	:2;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} I13;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	resv	:1;
		u_int64_t	cpos6b	:6;
		u_int64_t	r3	:7;
		u_int64_t	len6d	:6;
		u_int64_t	x	:1;
		u_int64_t	x2	:2;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} I14;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	len4d	:6;
		u_int64_t	cpos6d	:6;
		u_int64_t	op	:4;
	} I15;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	p1	:6;
		u_int64_t	c	:1;
		u_int64_t	y	:1;
		u_int64_t	pos6b	:6;
		u_int64_t	r3	:7;
		u_int64_t	p2	:6;
		u_int64_t	ta	:1;
		u_int64_t	x2	:2;
		u_int64_t	tb	:1;
		u_int64_t	op	:4;
	} I16;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	p1	:6;
		u_int64_t	c	:1;
		u_int64_t	y	:1;
		u_int64_t	resv	:6;
		u_int64_t	r3	:7;
		u_int64_t	p2	:6;
		u_int64_t	ta	:1;
		u_int64_t	x2	:2;
		u_int64_t	tb	:1;
		u_int64_t	op	:4;
	} I17;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm20a	:20;
		u_int64_t	resv	:1;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	i	:1;
		u_int64_t	op	:4;
	} I19;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm7a	:7;
		u_int64_t	r2	:7;
		u_int64_t	imm13c	:13;
		u_int64_t	x3	:3;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} I20;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	b1	:3;
		u_int64_t	resv1	:4;
		u_int64_t	r2	:7;
		u_int64_t	wh	:2;
		u_int64_t	x	:1;
		u_int64_t	ih	:1;
		u_int64_t	timm9c	:9;
		u_int64_t	x3	:3;
		u_int64_t	resv2	:1;
		u_int64_t	op	:4;
	} I21;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	b2	:3;
		u_int64_t	resv1	:11;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv2	:1;
		u_int64_t	op	:4;
	} I22;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	mask7a	:7;
		u_int64_t	r2	:7;
		u_int64_t	resv1	:4;
		u_int64_t	mask8c	:8;
		u_int64_t	resv2	:1;
		u_int64_t	x3	:3;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} I23;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm27a	:27;
		u_int64_t	x3	:3;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} I24;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	resv1	:14;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv2	:1;
		u_int64_t	op	:4;
	} I25;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv1	:7;
		u_int64_t	r2	:7;
		u_int64_t	ar3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv2	:1;
		u_int64_t	op	:4;
	} I26;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv1	:7;
		u_int64_t	imm7b	:7;
		u_int64_t	ar3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} I27;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	resv1	:7;
		u_int64_t	ar3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv2	:1;
		u_int64_t	op	:4;
	} I28;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	resv1	:7;
		u_int64_t	r3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv2	:1;
		u_int64_t	op	:4;
	} I29;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	resv7	:7;
		u_int64_t	r3	:7;
		u_int64_t	x	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M1;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	x	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M2, M16;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	imm7b	:7;
		u_int64_t	r3	:7;
		u_int64_t	i	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} M3;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	x	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M4;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm7a	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	i	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} M5;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	resv7	:7;
		u_int64_t	r3	:7;
		u_int64_t	x	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M6;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	x	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M7;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	imm7b	:7;
		u_int64_t	r3	:7;
		u_int64_t	i	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} M8;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7	:7;
		u_int64_t	f2	:7;
		u_int64_t	r3	:7;
		u_int64_t	x	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M9;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm7a	:7;
		u_int64_t	f2	:7;
		u_int64_t	r3	:7;
		u_int64_t	i	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} M10;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	f2	:7;
		u_int64_t	r3	:7;
		u_int64_t	x	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M11, M12;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv14	:14;
		u_int64_t	r3	:7;
		u_int64_t	x	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M13;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	x	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M14;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7	:7;
		u_int64_t	imm7b	:7;
		u_int64_t	r3	:7;
		u_int64_t	i	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} M15;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	i2b	:2;
		u_int64_t	s	:1;
		u_int64_t	resv4	:4;
		u_int64_t	r3	:7;
		u_int64_t	x	:1;
		u_int64_t	hint	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M17;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	r2	:7;
		u_int64_t	resv7	:7;
		u_int64_t	x	:1;
		u_int64_t	resv2	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M18;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	f2	:7;
		u_int64_t	resv7	:7;
		u_int64_t	x	:1;
		u_int64_t	resv2	:2;
		u_int64_t	x6	:6;
		u_int64_t	m	:1;
		u_int64_t	op	:4;
	} M19;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm7a	:7;
		u_int64_t	r2	:7;
		u_int64_t	imm13c	:13;
		u_int64_t	x3	:3;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} M20;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm7a	:7;
		u_int64_t	f2	:7;
		u_int64_t	imm13c	:13;
		u_int64_t	x3	:3;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} M21;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	imm20b	:20;
		u_int64_t	x3	:3;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} M22;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	imm20b	:20;
		u_int64_t	x3	:3;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} M23;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv21	:21;
		u_int64_t	x4	:4;
		u_int64_t	x2	:2;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M24, M25;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	resv14	:14;
		u_int64_t	x4	:4;
		u_int64_t	x2	:2;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M26;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	resv14	:14;
		u_int64_t	x4	:4;
		u_int64_t	x2	:2;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M27;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv14	:14;
		u_int64_t	r3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M28;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7	:7;
		u_int64_t	r2	:7;
		u_int64_t	ar3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M29;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7	:7;
		u_int64_t	imm7b	:7;
		u_int64_t	ar3	:7;
		u_int64_t	x4	:4;
		u_int64_t	x2	:2;
		u_int64_t	x3	:3;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} M30;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	resv7	:7;
		u_int64_t	ar3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M31;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7	:7;
		u_int64_t	r2	:7;
		u_int64_t	cr3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M32;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	resv7	:7;
		u_int64_t	cr3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M33;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	sof	:7;
		u_int64_t	sol	:7;
		u_int64_t	sor	:4;
		u_int64_t	resv2	:2;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M34;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7a	:7;
		u_int64_t	r2	:7;
		u_int64_t	resv7b	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M35;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	resv14	:14;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M36;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm20a	:20;
		u_int64_t	resv1	:1;
		u_int64_t	x4	:4;
		u_int64_t	x2	:2;
		u_int64_t	x3	:3;
		u_int64_t	i	:1;
		u_int64_t	op	:4;
	} M37;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M38;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	i2b	:2;
		u_int64_t	resv5	:5;
		u_int64_t	r3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M39;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7	:7;
		u_int64_t	i2b	:2;
		u_int64_t	resv5	:5;
		u_int64_t	r3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M40;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7a	:7;
		u_int64_t	r2	:7;
		u_int64_t	resv7b	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M41;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7	:7;
		u_int64_t	r2	:7;
		u_int64_t	r3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M42, M45;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	resv7	:7;
		u_int64_t	r3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} M43, M46;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm21a	:21;
		u_int64_t	x4	:4;
		u_int64_t	i2d	:2;
		u_int64_t	x3	:3;
		u_int64_t	i	:1;
		u_int64_t	op	:4;
	} M44;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	btype	:3;
		u_int64_t	resv3	:3;
		u_int64_t	p	:1;
		u_int64_t	imm20b	:20;
		u_int64_t	wh	:2;
		u_int64_t	d	:1;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} B1, B2;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	b1	:3;
		u_int64_t	resv3	:3;
		u_int64_t	p	:1;
		u_int64_t	imm20b	:20;
		u_int64_t	wh	:2;
		u_int64_t	d	:1;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} B3;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	btype	:3;
		u_int64_t	resv3	:3;
		u_int64_t	p	:1;
		u_int64_t	b2	:3;
		u_int64_t	resv11	:11;
		u_int64_t	x6	:6;
		u_int64_t	wh	:2;
		u_int64_t	d	:1;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} B4;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	b1	:3;
		u_int64_t	resv3	:3;
		u_int64_t	p	:1;
		u_int64_t	b2	:3;
		u_int64_t	resv17	:17;
		u_int64_t	wh	:2;
		u_int64_t	d	:1;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} B5;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	wh	:3;
		u_int64_t	resv1	:1;
		u_int64_t	timm7a	:7;
		u_int64_t	imm20b	:20;
		u_int64_t	t2e	:2;
		u_int64_t	ih	:1;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} B6;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	wh	:3;
		u_int64_t	resv1a	:1;
		u_int64_t	timm7a	:7;
		u_int64_t	b2	:3;
		u_int64_t	resv11	:11;
		u_int64_t	x6	:6;
		u_int64_t	t2e	:2;
		u_int64_t	ih	:1;
		u_int64_t	resv1b	:1;
		u_int64_t	op	:4;
	} B7;
	struct {
		u_int64_t	zero	:6;
		u_int64_t	resv21	:21;
		u_int64_t	x6	:6;
		u_int64_t	resv4	:4;
		u_int64_t	op	:4;
	} B8;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm20a	:20;
		u_int64_t	resv1	:1;
		u_int64_t	x6	:6;
		u_int64_t	resv3	:3;
		u_int64_t	i	:1;
		u_int64_t	op	:4;
	} B9;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	f2	:7;
		u_int64_t	f3	:7;
		u_int64_t	f4	:7;
		u_int64_t	sf	:2;
		u_int64_t	x	:1;
		u_int64_t	op	:4;
	} F1;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	f2	:7;
		u_int64_t	f3	:7;
		u_int64_t	f4	:7;
		u_int64_t	x2	:2;
		u_int64_t	x	:1;
		u_int64_t	op	:4;
	} F2;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	f2	:7;
		u_int64_t	f3	:7;
		u_int64_t	f4	:7;
		u_int64_t	resv2	:2;
		u_int64_t	x	:1;
		u_int64_t	op	:4;
	} F3;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	p1	:6;
		u_int64_t	ta	:1;
		u_int64_t	f2	:7;
		u_int64_t	f3	:7;
		u_int64_t	p2	:6;
		u_int64_t	ra	:1;
		u_int64_t	sf	:2;
		u_int64_t	rb	:1;
		u_int64_t	op	:4;
	} F4;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	p1	:6;
		u_int64_t	ta	:1;
		u_int64_t	f2	:7;
		u_int64_t	fclass7c:7;
		u_int64_t	p2	:6;
		u_int64_t	fc2	:2;
		u_int64_t	resv2	:2;
		u_int64_t	op	:4;
	} F5;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	f2	:7;
		u_int64_t	f3	:7;
		u_int64_t	p2	:6;
		u_int64_t	x	:1;
		u_int64_t	sf	:2;
		u_int64_t	q	:1;
		u_int64_t	op	:4;
	} F6;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	resv7	:7;
		u_int64_t	f3	:7;
		u_int64_t	p2	:6;
		u_int64_t	x	:1;
		u_int64_t	sf	:2;
		u_int64_t	q	:1;
		u_int64_t	op	:4;
	} F7;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	f2	:7;
		u_int64_t	f3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x	:1;
		u_int64_t	sf	:2;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} F8;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	f2	:7;
		u_int64_t	f3	:7;
		u_int64_t	x6	:6;
		u_int64_t	x	:1;
		u_int64_t	resv3	:3;
		u_int64_t	op	:4;
	} F9;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	f2	:7;
		u_int64_t	resv7	:7;
		u_int64_t	x6	:6;
		u_int64_t	x	:1;
		u_int64_t	sf	:2;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} F10;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	f1	:7;
		u_int64_t	f2	:7;
		u_int64_t	resv7	:7;
		u_int64_t	x6	:6;
		u_int64_t	x	:1;
		u_int64_t	resv3	:2;
		u_int64_t	op	:4;
	} F11;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv7	:7;
		u_int64_t	amask7b	:7;
		u_int64_t	omask7c	:7;
		u_int64_t	x6	:6;
		u_int64_t	x	:1;
		u_int64_t	sf	:2;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} F12;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	resv21	:7;
		u_int64_t	x6	:6;
		u_int64_t	x	:1;
		u_int64_t	sf	:2;
		u_int64_t	resv1	:1;
		u_int64_t	op	:4;
	} F13;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm20a	:20;
		u_int64_t	resv1	:1;
		u_int64_t	x6	:6;
		u_int64_t	x	:1;
		u_int64_t	sf	:2;
		u_int64_t	s	:1;
		u_int64_t	op	:4;
	} F14;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm20a	:20;
		u_int64_t	resv1	:1;
		u_int64_t	x6	:6;
		u_int64_t	x	:1;
		u_int64_t	resv2	:2;
		u_int64_t	i	:1;
		u_int64_t	op	:4;
	} F15;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	imm20a	:20;
		u_int64_t	resv1	:1;
		u_int64_t	x6	:6;
		u_int64_t	x3	:3;
		u_int64_t	i	:1;
		u_int64_t	op	:4;
	} X1;
	struct {
		u_int64_t	qp	:6;
		u_int64_t	r1	:7;
		u_int64_t	imm7b	:7;
		u_int64_t	vc	:1;
		u_int64_t	ic	:1;
		u_int64_t	imm5c	:5;
		u_int64_t	imm9d	:9;
		u_int64_t	i	:1;
		u_int64_t	op	:4;
	} X2;
	u_int64_t ins;
};

struct ia64_bundle {
	u_int64_t	slot[3];
	int		template;
};

extern void ia64_unpack_bundle(u_int64_t low, u_int64_t high,
			       struct ia64_bundle *bp);
extern void ia64_pack_bundle(u_int64_t *lowp, u_int64_t *highp,
			     const struct ia64_bundle *bp);

#endif /* _MACHINE_INST_H_ */
