/*
 * Copyright (c) 2001 Katsurajima Naoto <raven@katsurajima.seya.yokohama.jp>
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
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHERIN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THEPOSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* supported CODECs */
#define AK452X_TYPE_4524 0
#define AK452X_TYPE_4528 1

/* AK4524/AK4528 control registers */
#define AK4524_POWER 0x00
#define AK4528_POWER 0x00
#define   AK452X_POWER_PWDA 0x01
#define   AK452X_POWER_PWAD 0x02
#define   AK452X_POWER_PWVR 0x04
#define AK4524_RESET 0x01
#define AK4528_RESET 0x01
#define   AK452X_RESET_RSDA 0x01
#define   AK452X_RESET_RSAD 0x02
#define AK4524_FORMAT 0x02
#define AK4528_FORMAT 0x02
#define   AK452X_FORMAT_1X       0x00
#define   AK452X_FORMAT_2X       0x01
#define   AK452X_FORMAT_4X1      0x02
#define   AK452X_FORMAT_4X2      0x03
#define   AK452X_FORMAT_256FSN   0x00
#define   AK452X_FORMAT_512FSN   0x04
#define   AK452X_FORMAT_1024FSN  0x08
#define   AK452X_FORMAT_384FSN   0x10
#define   AK452X_FORMAT_768FSN   0x14
#define   AK452X_FORMAT_OM24IL16 0x00
#define   AK452X_FORMAT_OM24IL20 0x20
#define   AK452X_FORMAT_OM24IM24 0x40
#define   AK452X_FORMAT_I2S      0x60
#define   AK452X_FORMAT_OM24IL24 0x80
#define AK4524_DVC 0x03
#define   AK452X_DVC_DEM441  0x00
#define   AK452X_DVC_DEMOFF  0x01
#define   AK452X_DVC_DEM48   0x02
#define   AK452X_DVC_DEM32   0x03
#define   AK452X_DVC_ZTM256  0x00
#define   AK452X_DVC_ZTM512  0x04
#define   AK452X_DVC_ZTM1024 0x08
#define   AK452X_DVC_ZTM2048 0x0c
#define   AK452X_DVC_ZCE     0x10
#define   AK452X_DVC_HPFL    0x04
#define   AK452X_DVC_HPFR    0x08
#define   AK452X_DVC_SMUTE   0x80
#define AK4524_LIPGA 0x04
#define AK4524_RIPGA 0x05
#define AK4524_LOATT 0x06
#define AK4524_ROATT 0x07
#define AK4528_LOATT 0x04
#define AK4528_ROATT 0x05

struct ak452x_info;

typedef void (*ak452x_ctrl)(void *, unsigned int, unsigned int, unsigned int);

struct ak452x_info *ak452x_create(device_t dev, void *devinfo, int num, ak452x_ctrl);
void ak452x_destroy(struct ak452x_info *codec);
void ak452x_settype(struct ak452x_info *codec, unsigned int type);
void ak452x_setcif(struct ak452x_info *codec, unsigned int cif);
void ak452x_setformat(struct ak452x_info *codec, unsigned int format);
void ak452x_setdvc(struct ak452x_info *codec, unsigned int dvc);
void ak452x_init(struct ak452x_info *codec);
void ak452x_reinit(struct ak452x_info *codec);
void ak452x_set(struct ak452x_info *codec, int dir, unsigned int left, unsigned int right);
