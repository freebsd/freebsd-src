/*
 * if_sr.h
 *
 * Copyright (C) 1997-1999 Whistle Communications Inc.
 * All rights reserved.
 *
 * $FreeBSD: src/sys/i386/isa/if_sr.h,v 1.1.2.1 1999/11/19 07:35:41 julian Exp $
 */

#ifndef _I386_ISA_IF_SR_H_
#define _I386_ISA_IF_SR_H_

/* Node type name and type cookie */
#define NG_SR_NODE_TYPE		"sync_sr"
#define NG_SR_COOKIE		860552148

/* Netgraph hooks */
#define NG_SR_HOOK_DEBUG	"debug"
#define NG_SR_HOOK_CONTROL	"control"
#define NG_SR_HOOK_RAW		"rawdata"

#endif /* _I386_ISA_IF_SR_H_ */

