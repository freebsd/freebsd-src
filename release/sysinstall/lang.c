/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: lang.c,v 1.8 1996/04/13 13:31:46 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"

u_char default_scrnmap[256];

void
lang_set_Danish(char *str)
{
    systemChangeScreenmap(default_scrnmap);
    systemChangeLang("da_DK.ISO8859-1");
    systemChangeFont(font_iso_8x16);
    systemChangeTerminal("cons25l1", termcap_cons25l1,
			 "cons25l1-m", termcap_cons25l1_m);
}

void
lang_set_Dutch(char *str)
{
    systemChangeScreenmap(default_scrnmap);
    systemChangeLang("nl_NL.ISO8859-1");
    systemChangeFont(font_iso_8x16);
    systemChangeTerminal("cons25l1", termcap_cons25l1, "cons25l1-m", termcap_cons25l1_m);
}

void
lang_set_English(char *str)
{
    systemChangeScreenmap(default_scrnmap);
    systemChangeLang("en_US.ISO8859-1");
    systemChangeFont(font_cp850_8x16);
    systemChangeTerminal("cons25", termcap_cons25, "cons25-m", termcap_cons25_m);
}

void
lang_set_French(char *str)
{
    systemChangeScreenmap(default_scrnmap);
    systemChangeLang("fr_FR.ISO8859-1");
    systemChangeFont(font_iso_8x16);
    systemChangeTerminal("cons25l1", termcap_cons25l1, "cons25l1-m", termcap_cons25l1_m);
}

void
lang_set_German(char *str)
{
    systemChangeScreenmap(default_scrnmap);
    systemChangeLang("de_DE.ISO8859-1");
    systemChangeFont(font_iso_8x16);
    systemChangeTerminal("cons25l1", termcap_cons25l1, "cons25l1-m", termcap_cons25l1_m);
}

void
lang_set_Italian(char *str)
{
    systemChangeScreenmap(default_scrnmap);
    systemChangeLang("it_IT.ISO8859-1");
    systemChangeFont(font_iso_8x16);
    systemChangeTerminal("cons25l1", termcap_cons25l1, "cons25l1-m", termcap_cons25l1_m);
}

/* Someday we will have to do a lot better than this for Kanji text! */
void
lang_set_Japanese(char *str)
{
    systemChangeScreenmap(default_scrnmap);
    systemChangeLang("ja_JP.ROMAJI");
    systemChangeFont(font_cp850_8x16); /* must prepare JIS X0201 font? */
    systemChangeTerminal("cons25", termcap_cons25, "cons25-m", termcap_cons25_m);
}

void
lang_set_Norwegian(char *str)
{
    systemChangeScreenmap(default_scrnmap);
    systemChangeLang("no_NO.ISO8859-1");
    systemChangeFont(font_iso_8x16);
    systemChangeTerminal("cons25l1", termcap_cons25l1, "cons25l1-m", termcap_cons25l1_m);
}

void
lang_set_Russian(char *str)
{
    systemChangeScreenmap(koi8_r2cp866);
    systemChangeLang("ru_SU.KOI8-R");
    systemChangeFont(font_cp866_8x16);
    systemChangeTerminal("cons25r", termcap_cons25r, "cons25r-m", termcap_cons25r_m);
}

void
lang_set_Spanish(char *str)
{
    systemChangeScreenmap(default_scrnmap);
    systemChangeLang("es_ES.ISO8859-1");
    systemChangeFont(font_iso_8x16);
    systemChangeTerminal("cons25l1", termcap_cons25l1, "cons25l1-m", termcap_cons25l1_m);
}

void
lang_set_Swedish(char *str)
{
    systemChangeScreenmap(default_scrnmap);
    systemChangeLang("sv_SE.ISO8859-1");
    systemChangeFont(font_iso_8x16);
    systemChangeTerminal("cons25l1", termcap_cons25l1, "cons25l1-m", termcap_cons25l1_m);
}
