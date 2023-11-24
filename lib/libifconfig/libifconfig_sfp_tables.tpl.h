/*-
 * Copyright (c) 2020, Ryan Moeller <freqlabs@FreeBSD.org>
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
 */

{# THIS IS A TEMPLATE PROCESSED BY lib/libifconfig/sfp.lua #}

#pragma once

#include <stdint.h>

{%
for _, ent in ipairs(enums) do
    if type(ent) == "string" then
%}
/*
 * {*ent*}
 */

{%
    else
        local enum = ent
        local name = "sfp_"..enum.name
        local num, sym, desc, disp
%}
/** {*enum.description*} */
enum {*name*} {
{%
        for _, item in ipairs(enum.values) do
            val, sym, desc, disp = table.unpack(item)
            local symbol = string.upper(name).."_"..sym
%}
	{*symbol*} = {*val*}, /**< {*desc*} */
{%
        end
%}
};

/** Get the symbolic name of a given {*name*} value */
const char *ifconfig_{*name*}_symbol(enum {*name*});

/** Get a brief description of a given {*name*} value */
const char *ifconfig_{*name*}_description(enum {*name*});

{%
        if disp then
%}
/** Get a shortened user-friendly display name for a given {*name*} value */
const char *ifconfig_{*name*}_display(enum {*name*});

{%
        end
    end
end
%}
/*
 * Descriptions of each enum
 */

{%
for _, ent in ipairs(enums) do
    if type(ent) == "table" then
        local enum = ent
        local name = "sfp_"..enum.name
%}
/** Get a brief description of the {*name*} enum */
static inline const char *
ifconfig_enum_{*name*}_description(void)
{
	return ("{*enum.description*}");
}

{%
    end
end
%}
/*
 * Info struct definitions
 */

struct ifconfig_sfp_info {
{%
for _, ent in ipairs(enums) do
    if type(ent) == "table" then
        local enum = ent
        local name = "sfp_"..enum.name
        local t = string.format("uint%d_t", enum.bits)
%}
	{*t*} {*name*}; /**< {*enum.description*} */
{%
    end
end
%}
};

struct ifconfig_sfp_info_strings {
{%
for _, ent in ipairs(enums) do
    if type(ent) == "table" then
        local enum = ent
        local name = "sfp_"..enum.name
%}
	const char *{*name*}; /**< {*enum.description*} */
{%
    end
end
%}
};
