/* bfin-lex.l  ADI Blackfin lexer
   Copyright 2005, 2006
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */
%{

#include "as.h"
#include "bfin-defs.h"
#include "bfin-parse.h"

static long parse_int (char **end);
static int parse_halfreg (Register *r, int cl, char *hr);
static int parse_reg (Register *r, int type, char *rt);
int yylex (void);

#define _REG yylval.reg


%}

/* Define Start States ... Actually we will use exclusion.
   If no start state is specified it should match any state
   and <INITIAL> would match some keyword rules only with
   initial.  */
%s KEYWORD

%%
[sS][fF][tT][rR][eE][sS][eE][tT]        _REG.regno = REG_sftreset;  return REG;
[oO][mM][oO][dD][eE]                    _REG.regno = REG_omode;     return REG;
[iI][dD][lL][eE]_[rR][eE][qQ]           _REG.regno = REG_idle_req;  return REG;
[hH][wW][eE][rR][rR][cC][aA][uU][sS][eE] _REG.regno = REG_hwerrcause; return REG;
[eE][xX][cC][aA][uU][sS][eE]            _REG.regno = REG_excause;   return REG;
[eE][mM][uU][cC][aA][uU][sS][eE]	_REG.regno = REG_emucause;  return REG;
[zZ]                                    return Z;
[xX]                                    return X;
[wW]32                                  yylval.value = M_W32; return MMOD;
[wW]                                    return W;
[vV][iI][tT]_[mM][aA][xX]               return VIT_MAX;
[vV]               return V; /* Special: V is a statflag and a modifier.  */
[uU][sS][pP]       _REG.regno = REG_USP; return REG;
[tT][lL]                                return TL;
[tT][hH]                                return TH;
[tT][fF][uU]                            yylval.value = M_TFU; return MMOD;
[tT][eE][sS][tT][sS][eE][tT]            return TESTSET;
[tT]                                    yylval.value = M_T; return MMOD;
[sS]                                                        return S;
[sS][yY][sS][cC][fF][gG]       _REG.regno = REG_SYSCFG; return REG;
[sS][tT][iI]                            return STI;
[sS][sS][yY][nN][cC]                    return SSYNC;
[sS][pP]"."[lL]                         _REG.regno = REG_SP; return HALF_REG;
[sS][pP]"."[hH]                         _REG.regno = REG_SP | F_REG_HIGH; return HALF_REG;
[sS][pP]                                _REG.regno = REG_SP; return REG;
[sS][iI][gG][nN][bB][iI][tT][sS]        return SIGNBITS;
[sS][iI][gG][nN]                        return SIGN;
[sS][eE][qQ][sS][tT][aA][tT]     _REG.regno = REG_SEQSTAT; return REG;
[sS][eE][aA][rR][cC][hH]                return SEARCH;
[sS][hH][iI][fF][tT]                    return SHIFT;
[sS][cC][oO]                            return SCO;

[sS][aA][aA]                            return SAA;
[sS]2[rR][nN][dD]                       yylval.value = M_S2RND; return MMOD;
[rR][tT][xX]                            return RTX;
[rR][tT][sS]                            return RTS;
[rR][tT][nN]                            return RTN;
[rR][tT][iI]                            return RTI;
[rR][tT][eE]                            return RTE;
[rR][oO][tT]                            return ROT;
[rR][nN][dD]20                          return RND20;
[rR][nN][dD]12                          return RND12;
[rR][nN][dD][lL]                        return RNDL;
[rR][nN][dD][hH]                        return RNDH;
[rR][nN][dD]                            return RND;

[rR][0-7]"."[lLhHbB]  return parse_halfreg(&yylval.reg, T_REG_R, yytext);

[rR][eE][tT][sS]		_REG.regno = REG_RETS; return REG;
[rR][eE][tT][iI]		_REG.regno = REG_RETI; return REG;
[rR][eE][tT][xX]		_REG.regno = REG_RETX; return REG;
[rR][eE][tT][nN]		_REG.regno = REG_RETN; return REG;
[rR][eE][tT][eE]		_REG.regno = REG_RETE; return REG;
[eE][mM][uU][dD][aA][tT]	_REG.regno = REG_EMUDAT; return REG;
[rR][aA][iI][sS][eE]		return RAISE;

[rR][0-7]           return parse_reg (&yylval.reg, T_REG_R, yytext);

[rR]      return R;
[pP][rR][nN][tT]                        return PRNT;
[pP][cC]                                return PC;
[pP][aA][cC][kK]                        return PACK;

[pP][0-5]"."[lLhH]  return parse_halfreg (&yylval.reg, T_REG_P, yytext);
[pP][0-5]           return parse_reg (&yylval.reg, T_REG_P, yytext);

[oO][uU][tT][cC]                        return OUTC;
[oO][nN][eE][sS]                        return ONES;

[nN][oO][tT]                            return NOT;
[nN][oO][pP]                            return NOP;
[mM][nN][oO][pP]                        return MNOP;
[nN][sS]                                return NS;


[mM][iI][nN]                            return MIN;
[mM][aA][xX]                            return MAX;

[mM][0-3]"."[lLhH]  return parse_halfreg (&yylval.reg, T_REG_M, yytext);
[mM][0-3]           return parse_reg (&yylval.reg, T_REG_M, yytext);

[mM]                                    return M;
[lL][tT]                                return LT;
[lL][sS][hH][iI][fF][tT]                return LSHIFT;
[lL][sS][eE][tT][uU][pP]                return LSETUP;
[lL][oO][oO][pP]			return LOOP;
[lL][oO][oO][pP]_[bB][eE][gG][iI][nN]	return LOOP_BEGIN;
[lL][oO][oO][pP]_[eE][nN][dD]		return LOOP_END;

[lL][eE]                                return LE;
[lL][cC]0 _REG.regno = REG_LC0; return REG;
[lL][tT]0 _REG.regno = REG_LT0; return REG;
[lL][bB]0 _REG.regno = REG_LB0; return REG;
[lL][cC]1 _REG.regno = REG_LC1; return REG;
[lL][tT]1 _REG.regno = REG_LT1; return REG;
[lL][bB]1 _REG.regno = REG_LB1; return REG;

[lL][0-3]"."[lLhH]  return parse_halfreg (&yylval.reg, T_REG_L, yytext);
[lL][0-3]           return parse_reg (&yylval.reg, T_REG_L, yytext);
[lL][oO]                                return LO;
[jJ][uU][mM][pP]"."[sS]                 { BEGIN 0; return JUMP_DOT_S;}
[jJ][uU][mM][pP]"."[lL]                 { BEGIN 0; return JUMP_DOT_L;}
[jJ][uU][mM][pP]                        { BEGIN 0; return JUMP;}
[jJ][uU][mM][pP]"."[xX]                 { BEGIN 0; return JUMP_DOT_L; }
[iI][uU]                                yylval.value = M_IU;   return MMOD;
[iI][sS][sS]2                           yylval.value = M_ISS2; return MMOD;
[iI][sS]                                yylval.value = M_IS;   return MMOD;
[iI][hH]                                yylval.value = M_IH;   return MMOD;
[iI][fF]                                return IF;
[iI][0-3]"."[lLhH]  return parse_halfreg (&yylval.reg, T_REG_I, yytext);
[iI][0-3]           return parse_reg (&yylval.reg, T_REG_I, yytext);
[hH][lL][tT]                            return HLT;
[hH][iI]                                return HI;
[gG][tT]                                return GT;
[gG][eE]                                return GE;
[fF][uU]                                yylval.value = M_FU; return MMOD;
[fF][pP]         _REG.regno = REG_FP; return REG;
[fF][pP]"."[lL]  _REG.regno = REG_FP; return HALF_REG;
[fF][pP]"."[hH]  _REG.regno = REG_FP | F_REG_HIGH; return HALF_REG;

[eE][xX][tT][rR][aA][cC][tT]            return EXTRACT;
[eE][xX][pP][aA][dD][jJ]                return EXPADJ;
[eE][xX][cC][pP][tT]                    return EXCPT;
[eE][mM][uU][eE][xX][cC][pP][tT]        return EMUEXCPT;
[dD][iI][vV][sS]                        return DIVS;
[dD][iI][vV][qQ]                        return DIVQ;
[dD][iI][sS][aA][lL][gG][nN][eE][xX][cC][pP][tT]  return DISALGNEXCPT;
[dD][eE][pP][oO][sS][iI][tT]            return DEPOSIT;
[dD][bB][gG][hH][aA][lL][tT]            return DBGHALT;
[dD][bB][gG][cC][mM][pP][lL][xX]        return DBGCMPLX;
[dD][bB][gG][aA][lL]                    return DBGAL;
[dD][bB][gG][aA][hH]                    return DBGAH;
[dD][bB][gG][aA]                        return DBGA;
[dD][bB][gG]                            return DBG;
[cC][yY][cC][lL][eE][sS]2  { _REG.regno = REG_CYCLES2; return REG; }
[cC][yY][cC][lL][eE][sS]  { _REG.regno = REG_CYCLES; return REG; }
[cC][sS][yY][nN][cC]                    return CSYNC;
[cC][oO]                                return CO;
[cC][lL][iI]                            return CLI;

[cC][cC]     _REG.regno = REG_CC; return CCREG;
[cC][aA][lL][lL]"."[xX]                 { BEGIN 0; return CALL;}
[cC][aA][lL][lL]                        { BEGIN 0; return CALL;}
[bB][yY][tT][eE][uU][nN][pP][aA][cC][kK] return BYTEUNPACK;
[bB][yY][tT][eE][pP][aA][cC][kK]        return BYTEPACK;
[bB][yY][tT][eE][oO][pP]16[mM]          return BYTEOP16M;
[bB][yY][tT][eE][oO][pP]16[pP]          return BYTEOP16P;
[bB][yY][tT][eE][oO][pP]3[pP]           return BYTEOP3P;
[bB][yY][tT][eE][oO][pP]2[mM]           return BYTEOP2M;
[bB][yY][tT][eE][oO][pP]2[pP]           return BYTEOP2P;
[bB][yY][tT][eE][oO][pP]1[pP]           return BYTEOP1P;
[bB][yY]                                return BY;
[bB][xX][oO][rR][sS][hH][iI][fF][tT]    return BXORSHIFT;
[bB][xX][oO][rR]                        return BXOR;

[bB][rR][eE][vV]                        return BREV;
[bB][pP]                                return BP;
[bB][iI][tT][tT][sS][tT]                return BITTST;
[bB][iI][tT][tT][gG][lL]                return BITTGL;
[bB][iI][tT][sS][eE][tT]                return BITSET;
[bB][iI][tT][mM][uU][xX]                return BITMUX;
[bB][iI][tT][cC][lL][rR]                return BITCLR;
[bB][0-3]"."[lLhH]  return parse_halfreg (&yylval.reg, T_REG_B, yytext);
[bB][0-3]           return parse_reg (&yylval.reg, T_REG_B, yytext);
[bB]                                    return B;
[aA][zZ]  _REG.regno = S_AZ;   return STATUS_REG;
[aA][nN]  _REG.regno = S_AN;   return STATUS_REG;
[aA][qQ]  _REG.regno = S_AQ;   return STATUS_REG;
[aA][cC]0 _REG.regno = S_AC0;  return STATUS_REG;
[aA][cC]1 _REG.regno = S_AC1;  return STATUS_REG;
[aA][vV]0 _REG.regno = S_AV0;  return STATUS_REG;
[aA][vV]0[sS] _REG.regno = S_AV0S; return STATUS_REG;
[aA][vV]1 _REG.regno = S_AV1;  return STATUS_REG;
[aA][vV]1[sS] _REG.regno = S_AV1S; return STATUS_REG;
[vV] _REG.regno = S_V; return STATUS_REG;
[vV][sS]  _REG.regno = S_VS;   return STATUS_REG;


[aA][sS][tT][aA][tT]   _REG.regno = REG_ASTAT; return REG;
[aA][sS][hH][iI][fF][tT]                return ASHIFT;
[aA][sS][lL]                            return ASL;
[aA][sS][rR]                            return ASR;
[aA][lL][iI][gG][nN]8                   return ALIGN8;
[aA][lL][iI][gG][nN]16                  return ALIGN16;
[aA][lL][iI][gG][nN]24                  return ALIGN24;
[aA]1"."[lL]    return A_ONE_DOT_L;
[aA]0"."[lL]    return A_ZERO_DOT_L;
[aA]1"."[hH]    return A_ONE_DOT_H;
[aA]0"."[hH]    return A_ZERO_DOT_H;
[aA][bB][sS]                            return ABS;
abort                                   return ABORT;
[aA]1"."[xX]    _REG.regno = REG_A1x; return REG;
[aA]1"."[wW]    _REG.regno = REG_A1w; return REG;
[aA]1           _REG.regno = REG_A1;  return REG_A_DOUBLE_ONE;
[aA]0"."[xX]    _REG.regno = REG_A0x; return REG;
[aA]0"."[wW]    _REG.regno = REG_A0w; return REG;
[aA]0           _REG.regno = REG_A0;  return REG_A_DOUBLE_ZERO;
[Gg][Oo][Tt]	return GOT;
[Gg][Oo][Tt]"17"[Mm]"4" return GOT17M4;
[Ff][Uu][Nn][Cc][Dd][Ee][Ss][Cc]"_"[Gg][Oo][Tt]"17"[Mm]"4" return FUNCDESC_GOT17M4;
[Pp][Ll][Tt][Pp][Cc]	return PLTPC;


"~"                     return TILDA;
"|="                    return _BAR_ASSIGN;
"|"                     return BAR;
"^="                    return _CARET_ASSIGN;
"^"                     return CARET;
"]"                     return RBRACK;
"["                     return LBRACK;
">>>="                  return _GREATER_GREATER_GREATER_THAN_ASSIGN;
">>="                   return _GREATER_GREATER_ASSIGN;
">>>"                   return _GREATER_GREATER_GREATER;
">>"                    return GREATER_GREATER;
"=="                    return _ASSIGN_ASSIGN;
"="                     return ASSIGN;
"<="                    return _LESS_THAN_ASSIGN;
"<<="                   return _LESS_LESS_ASSIGN;
"<<"                    return LESS_LESS;
"<"                     return LESS_THAN;
"("                     return LPAREN;
")"                     return RPAREN;
":"                     return COLON;
"/"                     return SLASH;
"-="                    return _MINUS_ASSIGN;
"+|+"					return _PLUS_BAR_PLUS;
"-|+"					return _MINUS_BAR_PLUS;
"+|-"					return _PLUS_BAR_MINUS;
"-|-"					return _MINUS_BAR_MINUS;
"--"                    return _MINUS_MINUS;
"-"                     return MINUS;
","                     return COMMA;
"+="                    return _PLUS_ASSIGN;
"++"                    return _PLUS_PLUS;
"+"                     return PLUS;
"*="                    return _STAR_ASSIGN;
"*"                     return STAR;
"&="                    return _AMPERSAND_ASSIGN;
"&"                     return AMPERSAND;
"%"                     return PERCENT;
"!"                     return BANG;
";"                     return SEMICOLON;
"=!"			return _ASSIGN_BANG;
"||"			return DOUBLE_BAR;
"@"			return AT;
<KEYWORD>[pP][rR][eE][fF][eE][tT][cC][hH]        return PREFETCH;
<KEYWORD>[uU][nN][lL][iI][nN][kK]                return UNLINK;
<KEYWORD>[lL][iI][nN][kK]                        return LINK;
<KEYWORD>[iI][dD][lL][eE]                        return IDLE;
<KEYWORD>[iI][fF][lL][uU][sS][hH]                return IFLUSH;
<KEYWORD>[fF][lL][uU][sS][hH][iI][nN][vV]        return FLUSHINV;
<KEYWORD>[fF][lL][uU][sS][hH]                    return FLUSH;
([0-9]+)|(0[xX][0-9a-fA-F]+)|([bhfodBHOFD]#[0-9a-fA-F]+)|(0.[0-9]+) {
    yylval.value = parse_int (&yytext);
    return NUMBER;
  }
[A-Za-z_$.][A-Za-z0-9_$.]* { 
    yylval.symbol = symbol_find_or_make (yytext);
    symbol_mark_used (yylval.symbol);
    return SYMBOL; 
  }
[0-9][bfBF] {
    char *name;
    char *ref = strdup (yytext);
    if (ref[1] == 'b' || ref[1] == 'B')
      {
        name = fb_label_name ((int) (ref[0] - '0'), 0);
	yylval.symbol = symbol_find (name);

	if ((yylval.symbol != NULL)
             && (S_IS_DEFINED (yylval.symbol)))
          return SYMBOL;
	as_bad ("backward reference to unknown label %d:", 
						  (int) (ref[0] - '0')); 
      }
    else if (ref[1] == 'f' || ref[1] == 'F')
      {
        /* Forward reference.  Expect symbol to be undefined or
           unknown.  undefined: seen it before.  unknown: never seen
           it before.

           Construct a local label name, then an undefined symbol.
           Just return it as never seen before.  */

        name = fb_label_name ((int) (ref[0] - '0'), 1);
	yylval.symbol = symbol_find_or_make (name);
	/* We have no need to check symbol properties.  */
	return SYMBOL;
      }                                  
  }                                  
[ \t\n]                                    ;
"/*".*"*/"                                 ;
.                                          return yytext[0];
%%
static long parse_int (char **end)
{
  char fmt = '\0';
  int not_done = 1;
  int shiftvalue = 0;
  char * char_bag;
  long value = 0;
  char c;
  char *arg = *end;

  while (*arg && *arg == ' ')
    arg++;

  switch (*arg)
    {
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        fmt = 'd';
        break;

      case '0':  /* Accept different formated integers hex octal and binary. */
        {
	  c = *++arg;
          arg++;
	  if (c == 'x' || c == 'X') /* Hex input.  */
	    fmt = 'h';
	  else if (c == 'b' || c == 'B')
	    fmt = 'b';
	  else if (c == '.')
	    fmt = 'f';
	  else
            {             /* Octal.  */
	      arg--;
	      fmt = 'o';
	    }
	  break;
        }

      case 'd':
      case 'D':
      case 'h':
      case 'H':
      case 'o':
      case 'O':
      case 'b':
      case 'B':
      case 'f':
      case 'F':
        {
	  fmt = *arg++;
	  if (*arg == '#')
	    arg++;
        }
    }

  switch (fmt)
    {
      case 'h':
      case 'H':
        shiftvalue = 4;
        char_bag = "0123456789ABCDEFabcdef";
        break;

      case 'o':
      case 'O':
        shiftvalue = 3;
        char_bag = "01234567";
        break;

      case 'b':
      case 'B':
        shiftvalue = 1;
        char_bag = "01";
        break;

/* The assembler allows for fractional constants to be created
   by either the 0.xxxx or the f#xxxx format 

   i.e.   0.5 would result in 0x4000

   note .5 would result in the identifier .5.

   The assembler converts to fractional format 1.15 by the simple rule:

             value = (short) (finput * (1 << 15)).  */

      case 'f':
      case 'F':
        {
          float fval = 0.0;
          float pos = 10.0;
          while (1)
            {
              int c;
              c = *arg++;

              if (c >= '0' && c <= '9')
                {
                  float digit = (c - '0') / pos;
                  fval = fval + digit;
                  pos = pos * 10.0;
                }
              else
                {
	          *--arg = c;
                  value = (short) (fval * (1 << 15));
                  break;
                }
            }
          *end = arg+1;
          return value;
        }

      case 'd':
      case 'D':
      default:
        {
          while (1)
            {
              int c;
              c = *arg++;
              if (c >= '0' && c <= '9')
                value = (value * 10) + (c - '0');
              else
                {
                  /* Constants that are suffixed with k|K are multiplied by 1024
                     This suffix is only allowed on decimal constants. */
                  if (c == 'k' || c == 'K')
                    value *= 1024;
                  else
                    *--arg = c;
                  break;
                }
            }
          *end = arg+1;
          return value;
        }
    }

  while (not_done)
    {
      char c;
      c = *arg++;
      if (c == 0 || !index (char_bag, c))
	{
          not_done = 0;
          *--arg = c;
        }
      else
        {
          if (c >= 'a' && c <= 'z')
            c = c - ('a' - '9') + 1;
          else if (c >= 'A' && c <= 'Z')
            c = c - ('A' - '9') + 1;

          c -= '0';
          value = (value << shiftvalue) + c;
        }
    }
  *end = arg+1;
  return value;
}


static int parse_reg (Register *r, int cl, char *rt)
{
  r->regno = cl | (rt[1] - '0');
  return REG;
}

static int parse_halfreg (Register *r, int cl, char *rt)
{
  r->regno = cl | (rt[1] - '0');

  switch (rt[3])
    {
      case 'b':
      case 'B':
	return BYTE_DREG;

      case 'l':
      case 'L':
	break;

      case 'h':
      case 'H':
	r->regno |= F_REG_HIGH;
	break;
    }

  return HALF_REG;
}

/* Our start state is KEYWORD as we have
   command keywords such as PREFETCH.  */

void 
set_start_state (void)
{
  BEGIN KEYWORD;
}


#ifndef yywrap
int
yywrap ()
{ 
  return 1;
}
#endif
