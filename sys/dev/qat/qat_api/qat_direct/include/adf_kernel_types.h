/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_KERNEL_TYPES_H
#define ADF_KERNEL_TYPES_H

#define u64 uint64_t
#define u32 uint32_t
#define u16 uint16_t
#define u8 uint8_t
#define s64 int64_t
#define s32 int32_t
#define s16 int16_t
#define s8 int8_t

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

#endif
