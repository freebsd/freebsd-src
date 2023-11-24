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

#include <libifconfig_sfp_tables.h>
#include <libifconfig_sfp_tables_internal.h>

struct sfp_enum_metadata {
	int		value;		/* numeric discriminant value */
	const char	*symbol;	/* symbolic name */
	const char	*description;	/* brief description */
	const char	*display;	/* shortened display name */
};

const struct sfp_enum_metadata *
find_metadata(const struct sfp_enum_metadata *table, int value)
{
	while (table->value != value && table->symbol != NULL)
		++table;
	return (table->symbol != NULL ? table : NULL);
}

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
        local sym, desc, disp
%}
static const struct sfp_enum_metadata {*name*}_table_[] = {
{%
        for _, item in ipairs(enum.values) do
            _, sym, desc, disp = table.unpack(item)
            local symbol = string.upper(name).."_"..sym
%}
	{
		.value = {*symbol*},
		.symbol = "{*symbol*}",
		.description = "{*desc*}",
{%
            if disp then
%}
		.display = "{*disp*}",
{%
            end
%}
	},
{%
        end
%}
	{0}
};
const struct sfp_enum_metadata *{*name*}_table = {*name*}_table_;

const char *
ifconfig_{*name*}_symbol(enum {*name*} v)
{
	const struct sfp_enum_metadata *metadata;

	if ((metadata = find_metadata({*name*}_table, v)) == NULL)
		return (NULL);
	return (metadata->symbol);
}

const char *
ifconfig_{*name*}_description(enum {*name*} v)
{
	const struct sfp_enum_metadata *metadata;

	if ((metadata = find_metadata({*name*}_table, v)) == NULL)
		return (NULL);
	return (metadata->description);
}

{%
        if disp then
%}
const char *
ifconfig_{*name*}_display(enum {*name*} v)
{
	const struct sfp_enum_metadata *metadata;

	if ((metadata = find_metadata({*name*}_table, v)) == NULL)
		return (NULL);
	return (metadata->display);
}

{%
        end
    end
end
%}
