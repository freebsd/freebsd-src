NoEcho('
/******************************************************************************
 *
 * Module Name: aslprimaries.y - Rules for primary ASL operators
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

')


/*******************************************************************************
 *
 * ASL Primary Terms
 *
 ******************************************************************************/

AccessAsTerm
    : PARSEOP_ACCESSAS
        PARSEOP_OPEN_PAREN
        AccessTypeKeyword
        OptionalAccessAttribTerm
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateNode (PARSEOP_ACCESSAS,2,$3,$4);}
    | PARSEOP_ACCESSAS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

AcquireTerm
    : PARSEOP_ACQUIRE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_ACQUIRE);}
        SuperName
        ',' WordConstExpr
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$6);}
    | PARSEOP_ACQUIRE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

AddTerm
    : PARSEOP_ADD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_ADD);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_ADD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

AliasTerm
    : PARSEOP_ALIAS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_ALIAS);}
        NameString
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,
                                        TrSetNodeFlags ($5, NODE_IS_NAME_DECLARATION));}
    | PARSEOP_ALIAS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

AndTerm
    : PARSEOP_AND
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_AND);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_AND
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ArgTerm
    : PARSEOP_ARG0                  {$$ = TrCreateLeafNode (PARSEOP_ARG0);}
    | PARSEOP_ARG1                  {$$ = TrCreateLeafNode (PARSEOP_ARG1);}
    | PARSEOP_ARG2                  {$$ = TrCreateLeafNode (PARSEOP_ARG2);}
    | PARSEOP_ARG3                  {$$ = TrCreateLeafNode (PARSEOP_ARG3);}
    | PARSEOP_ARG4                  {$$ = TrCreateLeafNode (PARSEOP_ARG4);}
    | PARSEOP_ARG5                  {$$ = TrCreateLeafNode (PARSEOP_ARG5);}
    | PARSEOP_ARG6                  {$$ = TrCreateLeafNode (PARSEOP_ARG6);}
    ;

BankFieldTerm
    : PARSEOP_BANKFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_BANKFIELD);}
        NameString
        NameStringItem
        TermArgItem
        ',' AccessTypeKeyword
        ',' LockRuleKeyword
        ',' UpdateRuleKeyword
        PARSEOP_CLOSE_PAREN '{'
            FieldUnitList '}'       {$$ = TrLinkChildren ($<n>3,7,
                                        $4,$5,$6,$8,$10,$12,$15);}
    | PARSEOP_BANKFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN
        '{' error '}'               {$$ = AslDoError(); yyclearin;}
    ;

BreakTerm
    : PARSEOP_BREAK                 {$$ = TrCreateNode (PARSEOP_BREAK, 0);}
    ;

BreakPointTerm
    : PARSEOP_BREAKPOINT            {$$ = TrCreateNode (PARSEOP_BREAKPOINT, 0);}
    ;

BufferTerm
    : PARSEOP_BUFFER                {$<n>$ = TrCreateLeafNode (PARSEOP_BUFFER);}
        OptionalDataCount
        '{' BufferTermData '}'      {$$ = TrLinkChildren ($<n>2,2,$3,$5);}
    ;

BufferTermData
    : ByteList                      {}
    | StringData                    {}
    ;

CaseTerm
    : PARSEOP_CASE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_CASE);}
        DataObject
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkChildren ($<n>3,2,$4,$7);}
    | PARSEOP_CASE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ConcatTerm
    : PARSEOP_CONCATENATE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_CONCATENATE);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_CONCATENATE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ConcatResTerm
    : PARSEOP_CONCATENATERESTEMPLATE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (
                                        PARSEOP_CONCATENATERESTEMPLATE);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_CONCATENATERESTEMPLATE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CondRefOfTerm
    : PARSEOP_CONDREFOF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_CONDREFOF);}
        CondRefOfSource
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_CONDREFOF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ConnectionTerm
    : PARSEOP_CONNECTION
        PARSEOP_OPEN_PAREN
        NameString
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateNode (PARSEOP_CONNECTION,1,$3);}
    | PARSEOP_CONNECTION
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_CONNECTION);}
        ResourceMacroTerm
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3, 1,
                                        TrLinkChildren (
                                            TrCreateLeafNode (PARSEOP_RESOURCETEMPLATE), 3,
                                            TrCreateLeafNode (PARSEOP_DEFAULT_ARG),
                                            TrCreateLeafNode (PARSEOP_DEFAULT_ARG),
                                            $4));}
    | PARSEOP_CONNECTION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ContinueTerm
    : PARSEOP_CONTINUE              {$$ = TrCreateNode (PARSEOP_CONTINUE, 0);}
    ;

CopyObjectTerm
    : PARSEOP_COPYOBJECT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_COPYOBJECT);}
        TermArg
        ',' SimpleName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,
                                        TrSetNodeFlags ($6, NODE_IS_TARGET));}
    | PARSEOP_COPYOBJECT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateBitFieldTerm
    : PARSEOP_CREATEBITFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_CREATEBITFIELD);}
        TermArg
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,
                                        TrSetNodeFlags ($6, NODE_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEBITFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateByteFieldTerm
    : PARSEOP_CREATEBYTEFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_CREATEBYTEFIELD);}
        TermArg
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,
                                        TrSetNodeFlags ($6, NODE_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEBYTEFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateDWordFieldTerm
    : PARSEOP_CREATEDWORDFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_CREATEDWORDFIELD);}
        TermArg
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,
                                        TrSetNodeFlags ($6, NODE_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEDWORDFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateFieldTerm
    : PARSEOP_CREATEFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_CREATEFIELD);}
        TermArg
        TermArgItem
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,4,$4,$5,$6,
                                        TrSetNodeFlags ($7, NODE_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateQWordFieldTerm
    : PARSEOP_CREATEQWORDFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_CREATEQWORDFIELD);}
        TermArg
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,
                                        TrSetNodeFlags ($6, NODE_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEQWORDFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

CreateWordFieldTerm
    : PARSEOP_CREATEWORDFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_CREATEWORDFIELD);}
        TermArg
        TermArgItem
        NameStringItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,
                                        TrSetNodeFlags ($6, NODE_IS_NAME_DECLARATION));}
    | PARSEOP_CREATEWORDFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DataRegionTerm
    : PARSEOP_DATATABLEREGION
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_DATATABLEREGION);}
        NameString
        TermArgItem
        TermArgItem
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,4,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION),$5,$6,$7);}
    | PARSEOP_DATATABLEREGION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DebugTerm
    : PARSEOP_DEBUG                 {$$ = TrCreateLeafNode (PARSEOP_DEBUG);}
    ;

DecTerm
    : PARSEOP_DECREMENT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_DECREMENT);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_DECREMENT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DefaultTerm
    : PARSEOP_DEFAULT '{'           {$<n>$ = TrCreateLeafNode (PARSEOP_DEFAULT);}
        TermList '}'                {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_DEFAULT '{'
        error '}'                   {$$ = AslDoError(); yyclearin;}
    ;

DerefOfTerm
    : PARSEOP_DEREFOF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_DEREFOF);}
        DerefOfSource
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_DEREFOF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DeviceTerm
    : PARSEOP_DEVICE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_DEVICE);}
        NameString
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkChildren ($<n>3,2,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION),$7);}
    | PARSEOP_DEVICE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DivideTerm
    : PARSEOP_DIVIDE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_DIVIDE);}
        TermArg
        TermArgItem
        Target
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,4,$4,$5,$6,$7);}
    | PARSEOP_DIVIDE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

EISAIDTerm
    : PARSEOP_EISAID
        PARSEOP_OPEN_PAREN
        StringData
        PARSEOP_CLOSE_PAREN         {$$ = TrUpdateNode (PARSEOP_EISAID, $3);}
    | PARSEOP_EISAID
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ElseIfTerm
    : IfTerm ElseTerm               {$$ = TrLinkPeerNode ($1,$2);}
    ;

ElseTerm
    :                               {$$ = NULL;}
    | PARSEOP_ELSE '{'              {$<n>$ = TrCreateLeafNode (PARSEOP_ELSE);}
        TermList '}'                {$$ = TrLinkChildren ($<n>3,1,$4);}

    | PARSEOP_ELSE '{'
        error '}'                   {$$ = AslDoError(); yyclearin;}

    | PARSEOP_ELSE
        error                       {$$ = AslDoError(); yyclearin;}

    | PARSEOP_ELSEIF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_ELSE);}
        TermArg                     {$<n>$ = TrCreateLeafNode (PARSEOP_IF);}
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {TrLinkChildren ($<n>5,2,$4,$8);}
        ElseTerm                    {TrLinkPeerNode ($<n>5,$11);}
                                    {$$ = TrLinkChildren ($<n>3,1,$<n>5);}

    | PARSEOP_ELSEIF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}

    | PARSEOP_ELSEIF
        error                       {$$ = AslDoError(); yyclearin;}
    ;

EventTerm
    : PARSEOP_EVENT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_EVENT);}
        NameString
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION));}
    | PARSEOP_EVENT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ExternalTerm
    : PARSEOP_EXTERNAL
        PARSEOP_OPEN_PAREN
        NameString
        OptionalObjectTypeKeyword
        OptionalParameterTypePackage
        OptionalParameterTypesPackage
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateNode (PARSEOP_EXTERNAL,4,$3,$4,$5,$6);}
    | PARSEOP_EXTERNAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

FatalTerm
    : PARSEOP_FATAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_FATAL);}
        ByteConstExpr
        ',' DWordConstExpr
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$6,$7);}
    | PARSEOP_FATAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

FieldTerm
    : PARSEOP_FIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_FIELD);}
        NameString
        ',' AccessTypeKeyword
        ',' LockRuleKeyword
        ',' UpdateRuleKeyword
        PARSEOP_CLOSE_PAREN '{'
            FieldUnitList '}'       {$$ = TrLinkChildren ($<n>3,5,$4,$6,$8,$10,$13);}
    | PARSEOP_FIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN
        '{' error '}'               {$$ = AslDoError(); yyclearin;}
    ;

FindSetLeftBitTerm
    : PARSEOP_FINDSETLEFTBIT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_FINDSETLEFTBIT);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_FINDSETLEFTBIT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

FindSetRightBitTerm
    : PARSEOP_FINDSETRIGHTBIT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_FINDSETRIGHTBIT);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_FINDSETRIGHTBIT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

    /* Convert a For() loop to a While() loop */
ForTerm
    : PARSEOP_FOR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_WHILE);}
        OptionalTermArg ','         {}
        OptionalPredicate ','
        OptionalTermArg             {$<n>$ = TrLinkPeerNode ($4,$<n>3);
                                            TrSetParent ($9,$<n>3);}                /* New parent is WHILE */
        PARSEOP_CLOSE_PAREN
        '{' TermList '}'            {$<n>$ = TrLinkChildren ($<n>3,2,$7,$13);}
                                    {$<n>$ = TrLinkPeerNode ($13,$9);
                                        $$ = $<n>10;}
    ;

OptionalPredicate
    :                               {$$ = TrCreateValuedLeafNode (PARSEOP_INTEGER, 1);}
    | TermArg                       {$$ = $1;}
    ;

FprintfTerm
    : PARSEOP_FPRINTF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_FPRINTF);}
        TermArg ','
        StringData
        PrintfArgList
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$6,$7);}
    | PARSEOP_FPRINTF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

FromBCDTerm
    : PARSEOP_FROMBCD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_FROMBCD);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_FROMBCD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

FunctionTerm
    : PARSEOP_FUNCTION
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_METHOD);}
        NameString
        OptionalParameterTypePackage
        OptionalParameterTypesPackage
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkChildren ($<n>3,7,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION),
                                        TrCreateValuedLeafNode (PARSEOP_BYTECONST, 0),
                                        TrCreateLeafNode (PARSEOP_SERIALIZERULE_NOTSERIAL),
                                        TrCreateValuedLeafNode (PARSEOP_BYTECONST, 0),$5,$6,$9);}
    | PARSEOP_FUNCTION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IfTerm
    : PARSEOP_IF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_IF);}
        TermArg
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkChildren ($<n>3,2,$4,$7);}

    | PARSEOP_IF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IncludeTerm
    : PARSEOP_INCLUDE
        PARSEOP_OPEN_PAREN
        String
        PARSEOP_CLOSE_PAREN         {$$ = TrUpdateNode (PARSEOP_INCLUDE, $3);
                                        FlOpenIncludeFile ($3);}
    ;

IncludeEndTerm
    : PARSEOP_INCLUDE_END           {$<n>$ = TrCreateLeafNode (PARSEOP_INCLUDE_END);
                                        TrSetCurrentFilename ($$);}
    ;

IncTerm
    : PARSEOP_INCREMENT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_INCREMENT);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_INCREMENT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IndexFieldTerm
    : PARSEOP_INDEXFIELD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_INDEXFIELD);}
        NameString
        NameStringItem
        ',' AccessTypeKeyword
        ',' LockRuleKeyword
        ',' UpdateRuleKeyword
        PARSEOP_CLOSE_PAREN '{'
            FieldUnitList '}'       {$$ = TrLinkChildren ($<n>3,6,$4,$5,$7,$9,$11,$14);}
    | PARSEOP_INDEXFIELD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN
        '{' error '}'               {$$ = AslDoError(); yyclearin;}
    ;

IndexTerm
    : PARSEOP_INDEX
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_INDEX);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_INDEX
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LAndTerm
    : PARSEOP_LAND
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_LAND);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LAND
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LEqualTerm
    : PARSEOP_LEQUAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_LEQUAL);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LEQUAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LGreaterEqualTerm
    : PARSEOP_LGREATEREQUAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_LLESS);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateNode (PARSEOP_LNOT, 1,
                                        TrLinkChildren ($<n>3,2,$4,$5));}
    | PARSEOP_LGREATEREQUAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LGreaterTerm
    : PARSEOP_LGREATER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_LGREATER);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LGREATER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LLessEqualTerm
    : PARSEOP_LLESSEQUAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_LGREATER);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateNode (PARSEOP_LNOT, 1,
                                        TrLinkChildren ($<n>3,2,$4,$5));}
    | PARSEOP_LLESSEQUAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LLessTerm
    : PARSEOP_LLESS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_LLESS);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LLESS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LNotEqualTerm
    : PARSEOP_LNOTEQUAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_LEQUAL);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateNode (PARSEOP_LNOT, 1,
                                        TrLinkChildren ($<n>3,2,$4,$5));}
    | PARSEOP_LNOTEQUAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LNotTerm
    : PARSEOP_LNOT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_LNOT);}
        TermArg
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_LNOT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LoadTableTerm
    : PARSEOP_LOADTABLE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_LOADTABLE);}
        TermArg
        TermArgItem
        TermArgItem
        OptionalListString
        OptionalListString
        OptionalReference
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,6,$4,$5,$6,$7,$8,$9);}
    | PARSEOP_LOADTABLE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LoadTerm
    : PARSEOP_LOAD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_LOAD);}
        NameString
        RequiredTarget
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LOAD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

LocalTerm
    : PARSEOP_LOCAL0                {$$ = TrCreateLeafNode (PARSEOP_LOCAL0);}
    | PARSEOP_LOCAL1                {$$ = TrCreateLeafNode (PARSEOP_LOCAL1);}
    | PARSEOP_LOCAL2                {$$ = TrCreateLeafNode (PARSEOP_LOCAL2);}
    | PARSEOP_LOCAL3                {$$ = TrCreateLeafNode (PARSEOP_LOCAL3);}
    | PARSEOP_LOCAL4                {$$ = TrCreateLeafNode (PARSEOP_LOCAL4);}
    | PARSEOP_LOCAL5                {$$ = TrCreateLeafNode (PARSEOP_LOCAL5);}
    | PARSEOP_LOCAL6                {$$ = TrCreateLeafNode (PARSEOP_LOCAL6);}
    | PARSEOP_LOCAL7                {$$ = TrCreateLeafNode (PARSEOP_LOCAL7);}
    ;

LOrTerm
    : PARSEOP_LOR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_LOR);}
        TermArg
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_LOR
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

MatchTerm
    : PARSEOP_MATCH
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_MATCH);}
        TermArg
        ',' MatchOpKeyword
        TermArgItem
        ',' MatchOpKeyword
        TermArgItem
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,6,$4,$6,$7,$9,$10,$11);}
    | PARSEOP_MATCH
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

MethodTerm
    : PARSEOP_METHOD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_METHOD);}
        NameString
        OptionalByteConstExpr       {UtCheckIntegerRange ($5, 0, 7);}
        OptionalSerializeRuleKeyword
        OptionalByteConstExpr
        OptionalParameterTypePackage
        OptionalParameterTypesPackage
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkChildren ($<n>3,7,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION),
                                        $5,$7,$8,$9,$10,$13);}
    | PARSEOP_METHOD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

MidTerm
    : PARSEOP_MID
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_MID);}
        TermArg
        TermArgItem
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,4,$4,$5,$6,$7);}
    | PARSEOP_MID
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ModTerm
    : PARSEOP_MOD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_MOD);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_MOD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

MultiplyTerm
    : PARSEOP_MULTIPLY
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_MULTIPLY);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_MULTIPLY
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

MutexTerm
    : PARSEOP_MUTEX
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_MUTEX);}
        NameString
        ',' ByteConstExpr
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION),$6);}
    | PARSEOP_MUTEX
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

NameTerm
    : PARSEOP_NAME
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_NAME);}
        NameString
        ',' DataObject
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION),$6);}
    | PARSEOP_NAME
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

NAndTerm
    : PARSEOP_NAND
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_NAND);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_NAND
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

NoOpTerm
    : PARSEOP_NOOP                  {$$ = TrCreateNode (PARSEOP_NOOP, 0);}
    ;

NOrTerm
    : PARSEOP_NOR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_NOR);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_NOR
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

NotifyTerm
    : PARSEOP_NOTIFY
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_NOTIFY);}
        SuperName
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_NOTIFY
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

NotTerm
    : PARSEOP_NOT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_NOT);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_NOT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ObjectTypeTerm
    : PARSEOP_OBJECTTYPE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE);}
        ObjectTypeSource
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_OBJECTTYPE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

OffsetTerm
    : PARSEOP_OFFSET
        PARSEOP_OPEN_PAREN
        AmlPackageLengthTerm
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateNode (PARSEOP_OFFSET,1,$3);}
    | PARSEOP_OFFSET
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

OpRegionTerm
    : PARSEOP_OPERATIONREGION
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_OPERATIONREGION);}
        NameString
        ',' OpRegionSpaceIdTerm
        TermArgItem
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,4,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION),
                                        $6,$7,$8);}
    | PARSEOP_OPERATIONREGION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

OpRegionSpaceIdTerm
    : RegionSpaceKeyword            {}
    | ByteConst                     {$$ = UtCheckIntegerRange ($1, 0x80, 0xFF);}
    ;

OrTerm
    : PARSEOP_OR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_OR);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_OR
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PackageTerm
    : PARSEOP_PACKAGE               {$<n>$ = TrCreateLeafNode (PARSEOP_VAR_PACKAGE);}
        OptionalDataCount
        '{' PackageList '}'         {$$ = TrLinkChildren ($<n>2,2,$3,$5);}

PowerResTerm
    : PARSEOP_POWERRESOURCE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_POWERRESOURCE);}
        NameString
        ',' ByteConstExpr
        ',' WordConstExpr
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkChildren ($<n>3,4,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION),
                                        $6,$8,$11);}
    | PARSEOP_POWERRESOURCE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PrintfTerm
    : PARSEOP_PRINTF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_PRINTF);}
        StringData
        PrintfArgList
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_PRINTF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PrintfArgList
    :                               {$$ = NULL;}
    | TermArg                       {$$ = $1;}
    | PrintfArgList ','
       TermArg                      {$$ = TrLinkPeerNode ($1, $3);}
    ;

ProcessorTerm
    : PARSEOP_PROCESSOR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_PROCESSOR);}
        NameString
        ',' ByteConstExpr
        OptionalDWordConstExpr
        OptionalByteConstExpr
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkChildren ($<n>3,5,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION),
                                        $6,$7,$8,$11);}
    | PARSEOP_PROCESSOR
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

RawDataBufferTerm
    : PARSEOP_DATABUFFER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_DATABUFFER);}
        OptionalWordConst
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkChildren ($<n>3,2,$4,$7);}
    | PARSEOP_DATABUFFER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

/*
 * In RefOf, the node isn't really a target, but we can't keep track of it after
 * we've taken a pointer to it. (hard to tell if a local becomes initialized this way.)
 */
RefOfTerm
    : PARSEOP_REFOF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_REFOF);}
        RefOfSource
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,
                                        TrSetNodeFlags ($4, NODE_IS_TARGET));}
    | PARSEOP_REFOF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ReleaseTerm
    : PARSEOP_RELEASE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_RELEASE);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_RELEASE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ResetTerm
    : PARSEOP_RESET
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_RESET);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_RESET
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ReturnTerm
    : PARSEOP_RETURN
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_RETURN);}
        OptionalReturnArg
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_RETURN                {$$ = TrLinkChildren (
                                        TrCreateLeafNode (PARSEOP_RETURN),1,
                                        TrSetNodeFlags (TrCreateLeafNode (PARSEOP_ZERO),
                                            NODE_IS_NULL_RETURN));}
    | PARSEOP_RETURN
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ScopeTerm
    : PARSEOP_SCOPE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_SCOPE);}
        NameString
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkChildren ($<n>3,2,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION),$7);}
    | PARSEOP_SCOPE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ShiftLeftTerm
    : PARSEOP_SHIFTLEFT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_SHIFTLEFT);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_SHIFTLEFT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ShiftRightTerm
    : PARSEOP_SHIFTRIGHT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_SHIFTRIGHT);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_SHIFTRIGHT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SignalTerm
    : PARSEOP_SIGNAL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_SIGNAL);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_SIGNAL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SizeOfTerm
    : PARSEOP_SIZEOF
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_SIZEOF);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_SIZEOF
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SleepTerm
    : PARSEOP_SLEEP
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_SLEEP);}
        TermArg
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_SLEEP
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

StallTerm
    : PARSEOP_STALL
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_STALL);}
        TermArg
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_STALL
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

StoreTerm
    : PARSEOP_STORE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_STORE);}
        TermArg
        ',' SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,
                                            TrSetNodeFlags ($6, NODE_IS_TARGET));}
    | PARSEOP_STORE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SubtractTerm
    : PARSEOP_SUBTRACT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_SUBTRACT);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_SUBTRACT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SwitchTerm
    : PARSEOP_SWITCH
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_SWITCH);}
        TermArg
        PARSEOP_CLOSE_PAREN '{'
            CaseDefaultTermList '}' {$$ = TrLinkChildren ($<n>3,2,$4,$7);}
    | PARSEOP_SWITCH
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ThermalZoneTerm
    : PARSEOP_THERMALZONE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_THERMALZONE);}
        NameString
        PARSEOP_CLOSE_PAREN '{'
            TermList '}'            {$$ = TrLinkChildren ($<n>3,2,
                                        TrSetNodeFlags ($4, NODE_IS_NAME_DECLARATION),$7);}
    | PARSEOP_THERMALZONE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

TimerTerm
    : PARSEOP_TIMER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_TIMER);}
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,0);}
    | PARSEOP_TIMER                 {$$ = TrLinkChildren (
                                        TrCreateLeafNode (PARSEOP_TIMER),0);}
    | PARSEOP_TIMER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToBCDTerm
    : PARSEOP_TOBCD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_TOBCD);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_TOBCD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToBufferTerm
    : PARSEOP_TOBUFFER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_TOBUFFER);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_TOBUFFER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToDecimalStringTerm
    : PARSEOP_TODECIMALSTRING
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_TODECIMALSTRING);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_TODECIMALSTRING
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToHexStringTerm
    : PARSEOP_TOHEXSTRING
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_TOHEXSTRING);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_TOHEXSTRING
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToIntegerTerm
    : PARSEOP_TOINTEGER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_TOINTEGER);}
        TermArg
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_TOINTEGER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToPLDTerm
    : PARSEOP_TOPLD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_TOPLD);}
        PldKeywordList
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_TOPLD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PldKeywordList
    :                               {$$ = NULL;}
    | PldKeyword
        PARSEOP_EXP_EQUALS Integer  {$$ = TrLinkChildren ($1,1,$3);}
    | PldKeyword
        PARSEOP_EXP_EQUALS String   {$$ = TrLinkChildren ($1,1,$3);}
    | PldKeywordList ','            /* Allows a trailing comma at list end */
    | PldKeywordList ','
        PldKeyword
        PARSEOP_EXP_EQUALS Integer  {$$ = TrLinkPeerNode ($1,TrLinkChildren ($3,1,$5));}
    | PldKeywordList ','
        PldKeyword
        PARSEOP_EXP_EQUALS String   {$$ = TrLinkPeerNode ($1,TrLinkChildren ($3,1,$5));}
    ;


ToStringTerm
    : PARSEOP_TOSTRING
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_TOSTRING);}
        TermArg
        OptionalCount
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_TOSTRING
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ToUUIDTerm
    : PARSEOP_TOUUID
        PARSEOP_OPEN_PAREN
        StringData
        PARSEOP_CLOSE_PAREN         {$$ = TrUpdateNode (PARSEOP_TOUUID, $3);}
    | PARSEOP_TOUUID
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

UnicodeTerm
    : PARSEOP_UNICODE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_UNICODE);}
        StringData
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,0,$4);}
    | PARSEOP_UNICODE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

UnloadTerm
    : PARSEOP_UNLOAD
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_UNLOAD);}
        SuperName
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_UNLOAD
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

WaitTerm
    : PARSEOP_WAIT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_WAIT);}
        SuperName
        TermArgItem
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,2,$4,$5);}
    | PARSEOP_WAIT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

XOrTerm
    : PARSEOP_XOR
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_XOR);}
        TermArg
        TermArgItem
        Target
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildren ($<n>3,3,$4,$5,$6);}
    | PARSEOP_XOR
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

WhileTerm
    : PARSEOP_WHILE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_WHILE);}
        TermArg
        PARSEOP_CLOSE_PAREN
            '{' TermList '}'        {$$ = TrLinkChildren ($<n>3,2,$4,$7);}
    | PARSEOP_WHILE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;
