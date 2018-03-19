/*
 * Copyright (c) 2013-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "errcode.h"

const char *errstr[] = {
	"success",

	"cannot open file",
	"cannot read file",
	"cannot get file size",
	"cannot write file",
	"out of range",

	"label has no address",
	"yasm directive 'org' is required",
	"no pt directive",
	"no such label",
	"label name is too long",
	"label name is not unique",

	"failed to find section name",
	"failed to find value for section attribute",
	"unknown section attribute",

	"missing ')'",
	"missing '('",

	"parse error",
	"integer cannot be parsed",
	"integer too big",
	"ipc missing or has invalid value",
	"ip missing",
	"no arguments",
	"trailing tokens",
	"unknown character",
	"unknown directive",
	"missing directive",

	"unexpected sub C-state",
	"invalid C-state",

	"no open sideband file",
	"sideband format error",
	"configuration error",

	"pt library error",

	"run failed",

	"unspecified error",

	"out of memory",

	"internal error",

	"processing stopped",

	"max error code",
};
