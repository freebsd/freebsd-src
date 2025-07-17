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

#include <libifconfig_sfp.h>
#include <libifconfig_sfp_tables.h>

struct sfp_enum_metadata;
const struct sfp_enum_metadata *find_metadata(const struct sfp_enum_metadata *,
    int);

{%
for _, ent in ipairs(enums) do
    if type(ent) == "table" then
        local enum = ent
        local name = "sfp_"..enum.name
%}
extern const struct sfp_enum_metadata *{*name*}_table;
{%
    end
end
%}

static inline void
get_sfp_info_strings(const struct ifconfig_sfp_info *sfp,
    struct ifconfig_sfp_info_strings *strings)
{
{%
for _, ent in ipairs(enums) do
    if type(ent) == "table" then
        local enum = ent
        local name = "sfp_"..enum.name
%}
	strings->{*name*} = ifconfig_{*name*}_description(sfp->{*name*});
{%
    end
end
%}
}
