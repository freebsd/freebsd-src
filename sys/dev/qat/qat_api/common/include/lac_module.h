/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef __LAC_MODULE_H__
#define __LAC_MODULE_H__

#include "icp_qat_hw.h"

/* Lac module getter/setter for TUNABLE_INT in lac_module.c */
icp_qat_hw_auth_mode_t Lac_GetQatHmacMode(void);
void Lac_SetQatHmacMode(const icp_qat_hw_auth_mode_t);

#endif
