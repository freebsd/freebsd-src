NoEcho('
/******************************************************************************
 *
 * Module Name: aslrules.y - Main Bison/Yacc production rules
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
 * ASL Root and Secondary Terms
 *
 ******************************************************************************/

/*
 * Root term. Allow multiple #line directives before the definition block
 * to handle output from preprocessors
 */
AslCode
    : DefinitionBlockList           {$<n>$ = TrLinkChildren (
                                        TrCreateLeafNode (PARSEOP_ASL_CODE),1, $1);}
    | error                         {YYABORT; $$ = NULL;}
    ;


/*
 * Note concerning support for "module-level code".
 *
 * ACPI 1.0 allowed Type1 and Type2 executable opcodes outside of control
 * methods (the so-called module-level code.) This support was explicitly
 * removed in ACPI 2.0, but this type of code continues to be created by
 * BIOS vendors. In order to support the disassembly and recompilation of
 * such code (and the porting of ASL code to iASL), iASL supports this
 * code in violation of the current ACPI specification.
 *
 * The grammar change to support module-level code is to revert the
 * {ObjectList} portion of the DefinitionBlockTerm in ACPI 2.0 to the
 * original use of {TermList} instead (see below.) This allows the use
 * of Type1 and Type2 opcodes at module level.
 *
 * 04/2016: The module-level code is now allowed in the following terms:
 * DeviceTerm, PowerResTerm, ProcessorTerm, ScopeTerm, ThermalZoneTerm.
 * The ObjectList term is obsolete and has been removed.
 */
DefinitionBlockTerm
    : PARSEOP_DEFINITION_BLOCK
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafNode (PARSEOP_DEFINITION_BLOCK);}
        String ','
        String ','
        ByteConst ','
        String ','
        String ','
        DWordConst
        PARSEOP_CLOSE_PAREN         {TrSetEndLineNumber ($<n>3);}
            '{' TermList '}'        {$$ = TrLinkChildren ($<n>3,7,
                                        $4,$6,$8,$10,$12,$14,$18);}
    ;

DefinitionBlockList
    : DefinitionBlockTerm
    | DefinitionBlockTerm
        DefinitionBlockList         {$$ = TrLinkPeerNodes (2, $1,$2);}
    ;


/******* Basic ASCII identifiers **************************************************/

/* Allow IO, DMA, IRQ Resource macro and FOR macro names to also be used as identifiers */

NameString
    : NameSeg                       {}
    | PARSEOP_NAMESTRING            {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) $1);}
    | PARSEOP_IO                    {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) "IO");}
    | PARSEOP_DMA                   {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) "DMA");}
    | PARSEOP_IRQ                   {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) "IRQ");}
    | PARSEOP_FOR                   {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) "FOR");}
    ;
/*
NameSeg
    : PARSEOP_NAMESEG               {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESEG, (ACPI_NATIVE_INT)
                                        TrNormalizeNameSeg ($1));}
    ;
*/

NameSeg
    : PARSEOP_NAMESEG               {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESEG,
                                        (ACPI_NATIVE_INT) AslCompilerlval.s);}
    ;


/******* Fundamental argument/statement types ***********************************/

Term
    : Object                        {}
    | Type1Opcode                   {}
    | Type2Opcode                   {}
    | Type2IntegerOpcode            {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);}
    | Type2StringOpcode             {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);}
    | Type2BufferOpcode             {}
    | Type2BufferOrStringOpcode     {}
    | error                         {$$ = AslDoError(); yyclearin;}
    ;

SuperName
    : SimpleName                    {}
    | DebugTerm                     {}
    | Type6Opcode                   {}
    ;

Target
    :                               {$$ = TrCreateNullTarget ();} /* Placeholder is a ZeroOp object */
    | ','                           {$$ = TrCreateNullTarget ();} /* Placeholder is a ZeroOp object */
    | ',' SuperName                 {$$ = TrSetNodeFlags ($2, NODE_IS_TARGET);}
    ;

RequiredTarget
    : ',' SuperName                 {$$ = TrSetNodeFlags ($2, NODE_IS_TARGET);}
    ;

TermArg
    : SimpleName                    {$$ = TrSetNodeFlags ($1, NODE_IS_TERM_ARG);}
    | Type2Opcode                   {$$ = TrSetNodeFlags ($1, NODE_IS_TERM_ARG);}
    | DataObject                    {$$ = TrSetNodeFlags ($1, NODE_IS_TERM_ARG);}
/*
    | PARSEOP_OPEN_PAREN
        TermArg
        PARSEOP_CLOSE_PAREN         {}
*/
    ;

/*
 NOTE: Removed from TermArg due to reduce/reduce conflicts:
    | Type2IntegerOpcode            {$$ = TrSetNodeFlags ($1, NODE_IS_TERM_ARG);}
    | Type2StringOpcode             {$$ = TrSetNodeFlags ($1, NODE_IS_TERM_ARG);}
    | Type2BufferOpcode             {$$ = TrSetNodeFlags ($1, NODE_IS_TERM_ARG);}
    | Type2BufferOrStringOpcode     {$$ = TrSetNodeFlags ($1, NODE_IS_TERM_ARG);}

*/

MethodInvocationTerm
    : NameString
        PARSEOP_OPEN_PAREN          {TrUpdateNode (PARSEOP_METHODCALL, $1);}
        ArgList
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkChildNode ($1,$4);}
    ;

/* OptionalCount must appear before ByteList or an incorrect reduction will result */

OptionalCount
    :                               {$$ = TrCreateLeafNode (PARSEOP_ONES);}       /* Placeholder is a OnesOp object */
    | ','                           {$$ = TrCreateLeafNode (PARSEOP_ONES);}       /* Placeholder is a OnesOp object */
    | ',' TermArg                   {$$ = $2;}
    ;

/*
 * Data count for buffers and packages (byte count for buffers,
 * element count for packages).
 */
OptionalDataCount

        /* Legacy ASL */
    :                               {$$ = NULL;}
    | PARSEOP_OPEN_PAREN
        TermArg
        PARSEOP_CLOSE_PAREN         {$$ = $2;}
    | PARSEOP_OPEN_PAREN
        PARSEOP_CLOSE_PAREN         {$$ = NULL;}

        /* C-style (ASL+) -- adds equals term */

    |  PARSEOP_EXP_EQUALS           {$$ = NULL;}

    | PARSEOP_OPEN_PAREN
        TermArg
        PARSEOP_CLOSE_PAREN
        PARSEOP_EXP_EQUALS          {$$ = $2;}

    | PARSEOP_OPEN_PAREN
        PARSEOP_CLOSE_PAREN
        String
        PARSEOP_EXP_EQUALS          {$$ = NULL;}
    ;


/******* List Terms **************************************************/

    /* ACPI 3.0 -- allow semicolons between terms */

TermList
    :                               {$$ = NULL;}
    | TermList Term                 {$$ = TrLinkPeerNode (
                                        TrSetNodeFlags ($1, NODE_RESULT_NOT_USED),$2);}
    | TermList Term ';'             {$$ = TrLinkPeerNode (
                                        TrSetNodeFlags ($1, NODE_RESULT_NOT_USED),$2);}
    | TermList ';' Term             {$$ = TrLinkPeerNode (
                                        TrSetNodeFlags ($1, NODE_RESULT_NOT_USED),$3);}
    | TermList ';' Term ';'         {$$ = TrLinkPeerNode (
                                        TrSetNodeFlags ($1, NODE_RESULT_NOT_USED),$3);}
    ;

ArgList
    :                               {$$ = NULL;}
    | TermArg
    | ArgList ','                   /* Allows a trailing comma at list end */
    | ArgList ','
        TermArg                     {$$ = TrLinkPeerNode ($1,$3);}
    ;

ByteList
    :                               {$$ = NULL;}
    | ByteConstExpr
    | ByteList ','                  /* Allows a trailing comma at list end */
    | ByteList ','
        ByteConstExpr               {$$ = TrLinkPeerNode ($1,$3);}
    ;

DWordList
    :                               {$$ = NULL;}
    | DWordConstExpr
    | DWordList ','                 /* Allows a trailing comma at list end */
    | DWordList ','
        DWordConstExpr              {$$ = TrLinkPeerNode ($1,$3);}
    ;

FieldUnitList
    :                               {$$ = NULL;}
    | FieldUnit
    | FieldUnitList ','             /* Allows a trailing comma at list end */
    | FieldUnitList ','
        FieldUnit                   {$$ = TrLinkPeerNode ($1,$3);}
    ;

FieldUnit
    : FieldUnitEntry                {}
    | OffsetTerm                    {}
    | AccessAsTerm                  {}
    | ConnectionTerm                {}
    ;

FieldUnitEntry
    : ',' AmlPackageLengthTerm      {$$ = TrCreateNode (PARSEOP_RESERVED_BYTES,1,$2);}
    | NameSeg ','
        AmlPackageLengthTerm        {$$ = TrLinkChildNode ($1,$3);}
    ;

Object
    : CompilerDirective             {}
    | NamedObject                   {}
    | NameSpaceModifier             {}
//    | StructureTerm                 {}
    ;

PackageList
    :                               {$$ = NULL;}
    | PackageElement
    | PackageList ','               /* Allows a trailing comma at list end */
    | PackageList ','
        PackageElement              {$$ = TrLinkPeerNode ($1,$3);}
    ;

PackageElement
    : DataObject                    {}
    | NameString                    {}
    ;

    /* Rules for specifying the type of one method argument or return value */

ParameterTypePackage
    :                               {$$ = NULL;}
    | ObjectTypeKeyword             {$$ = $1;}
    | ParameterTypePackage ','
        ObjectTypeKeyword           {$$ = TrLinkPeerNodes (2,$1,$3);}
    ;

ParameterTypePackageList
    :                               {$$ = NULL;}
    | ObjectTypeKeyword             {$$ = $1;}
    | '{' ParameterTypePackage '}'  {$$ = $2;}
    ;

OptionalParameterTypePackage
    :                               {$$ = TrCreateLeafNode (PARSEOP_DEFAULT_ARG);}
    | ',' ParameterTypePackageList  {$$ = TrLinkChildren (
                                        TrCreateLeafNode (PARSEOP_DEFAULT_ARG),1,$2);}
    ;

    /* Rules for specifying the types for method arguments */

ParameterTypesPackage
    : ParameterTypePackageList      {$$ = $1;}
    | ParameterTypesPackage ','
        ParameterTypePackageList    {$$ = TrLinkPeerNodes (2,$1,$3);}
    ;

ParameterTypesPackageList
    :                               {$$ = NULL;}
    | ObjectTypeKeyword             {$$ = $1;}
    | '{' ParameterTypesPackage '}' {$$ = $2;}
    ;

OptionalParameterTypesPackage
    :                               {$$ = TrCreateLeafNode (PARSEOP_DEFAULT_ARG);}
    | ',' ParameterTypesPackageList {$$ = TrLinkChildren (
                                        TrCreateLeafNode (PARSEOP_DEFAULT_ARG),1,$2);}
    ;

/*
 * Case-Default list; allow only one Default term and unlimited Case terms
 */
CaseDefaultTermList
    :                               {$$ = NULL;}
    | CaseTerm                      {}
    | DefaultTerm                   {}
    | CaseDefaultTermList
        CaseTerm                    {$$ = TrLinkPeerNode ($1,$2);}
    | CaseDefaultTermList
        DefaultTerm                 {$$ = TrLinkPeerNode ($1,$2);}

/* Original - attempts to force zero or one default term within the switch */

/*
CaseDefaultTermList
    :                               {$$ = NULL;}
    | CaseTermList
        DefaultTerm
        CaseTermList                {$$ = TrLinkPeerNode ($1,TrLinkPeerNode ($2, $3));}
    | CaseTermList
        CaseTerm                    {$$ = TrLinkPeerNode ($1,$2);}
    ;

CaseTermList
    :                               {$$ = NULL;}
    | CaseTerm                      {}
    | CaseTermList
        CaseTerm                    {$$ = TrLinkPeerNode ($1,$2);}
    ;
*/


/*******************************************************************************
 *
 * ASL Data and Constant Terms
 *
 ******************************************************************************/

DataObject
    : BufferData                    {}
    | PackageData                   {}
    | IntegerData                   {}
    | StringData                    {}
    ;

BufferData
    : Type5Opcode                   {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);}
    | Type2BufferOrStringOpcode     {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);}
    | Type2BufferOpcode             {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);}
    | BufferTerm                    {}
    ;

PackageData
    : PackageTerm                   {}
    ;

IntegerData
    : Type2IntegerOpcode            {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);}
    | Type3Opcode                   {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);}
    | Integer                       {}
    | ConstTerm                     {}
    ;

StringData
    : Type2StringOpcode             {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);}
    | String                        {}
    ;

ByteConst
    : Integer                       {$$ = TrUpdateNode (PARSEOP_BYTECONST, $1);}
    ;

WordConst
    : Integer                       {$$ = TrUpdateNode (PARSEOP_WORDCONST, $1);}
    ;

DWordConst
    : Integer                       {$$ = TrUpdateNode (PARSEOP_DWORDCONST, $1);}
    ;

QWordConst
    : Integer                       {$$ = TrUpdateNode (PARSEOP_QWORDCONST, $1);}
    ;

/*
 * The NODE_COMPILE_TIME_CONST flag in the following constant expressions
 * enables compile-time constant folding to reduce the Type3Opcodes/Type2IntegerOpcodes
 * to simple integers. It is an error if these types of expressions cannot be
 * reduced, since the AML grammar for ****ConstExpr requires a simple constant.
 * Note: The required byte length of the constant is passed through to the
 * constant folding code in the node AmlLength field.
 */
ByteConstExpr
    : Type3Opcode                   {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);
                                        TrSetNodeAmlLength ($1, 1);}
    | Type2IntegerOpcode            {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);
                                        TrSetNodeAmlLength ($1, 1);}
    | ConstExprTerm                 {$$ = TrUpdateNode (PARSEOP_BYTECONST, $1);}
    | ByteConst                     {}
    ;

WordConstExpr
    : Type3Opcode                   {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);
                                        TrSetNodeAmlLength ($1, 2);}
    | Type2IntegerOpcode            {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);
                                        TrSetNodeAmlLength ($1, 2);}
    | ConstExprTerm                 {$$ = TrUpdateNode (PARSEOP_WORDCONST, $1);}
    | WordConst                     {}
    ;

DWordConstExpr
    : Type3Opcode                   {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);
                                        TrSetNodeAmlLength ($1, 4);}
    | Type2IntegerOpcode            {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);
                                        TrSetNodeAmlLength ($1, 4);}
    | ConstExprTerm                 {$$ = TrUpdateNode (PARSEOP_DWORDCONST, $1);}
    | DWordConst                    {}
    ;

QWordConstExpr
    : Type3Opcode                   {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);
                                        TrSetNodeAmlLength ($1, 8);}
    | Type2IntegerOpcode            {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);
                                        TrSetNodeAmlLength ($1, 8);}
    | ConstExprTerm                 {$$ = TrUpdateNode (PARSEOP_QWORDCONST, $1);}
    | QWordConst                    {}
    ;

ConstTerm
    : ConstExprTerm                 {}
    | PARSEOP_REVISION              {$$ = TrCreateLeafNode (PARSEOP_REVISION);}
    ;

ConstExprTerm
    : PARSEOP_ZERO                  {$$ = TrCreateValuedLeafNode (PARSEOP_ZERO, 0);}
    | PARSEOP_ONE                   {$$ = TrCreateValuedLeafNode (PARSEOP_ONE, 1);}
    | PARSEOP_ONES                  {$$ = TrCreateValuedLeafNode (PARSEOP_ONES, ACPI_UINT64_MAX);}
    | PARSEOP___DATE__              {$$ = TrCreateConstantLeafNode (PARSEOP___DATE__);}
    | PARSEOP___FILE__              {$$ = TrCreateConstantLeafNode (PARSEOP___FILE__);}
    | PARSEOP___LINE__              {$$ = TrCreateConstantLeafNode (PARSEOP___LINE__);}
    | PARSEOP___PATH__              {$$ = TrCreateConstantLeafNode (PARSEOP___PATH__);}
    ;

Integer
    : PARSEOP_INTEGER               {$$ = TrCreateValuedLeafNode (PARSEOP_INTEGER,
                                        AslCompilerlval.i);}
    ;

String
    : PARSEOP_STRING_LITERAL        {$$ = TrCreateValuedLeafNode (PARSEOP_STRING_LITERAL,
                                        (ACPI_NATIVE_INT) AslCompilerlval.s);}
    ;


/*******************************************************************************
 *
 * ASL Opcode Terms
 *
 ******************************************************************************/

CompilerDirective
    : IncludeTerm                   {}
    | IncludeEndTerm                {}
    | ExternalTerm                  {}
    ;

NamedObject
    : BankFieldTerm                 {}
    | CreateBitFieldTerm            {}
    | CreateByteFieldTerm           {}
    | CreateDWordFieldTerm          {}
    | CreateFieldTerm               {}
    | CreateQWordFieldTerm          {}
    | CreateWordFieldTerm           {}
    | DataRegionTerm                {}
    | DeviceTerm                    {}
    | EventTerm                     {}
    | FieldTerm                     {}
    | FunctionTerm                  {}
    | IndexFieldTerm                {}
    | MethodTerm                    {}
    | MutexTerm                     {}
    | OpRegionTerm                  {}
    | PowerResTerm                  {}
    | ProcessorTerm                 {}
    | ThermalZoneTerm               {}
    ;

NameSpaceModifier
    : AliasTerm                     {}
    | NameTerm                      {}
//    | NameTermAslPlus               {}
    | ScopeTerm                     {}
    ;

SimpleName
    : NameString                    {}
    | LocalTerm                     {}
    | ArgTerm                       {}
    ;

/* For ObjectType(), SuperName except for MethodInvocationTerm */

ObjectTypeSource
    : SimpleName                    {}
    | DebugTerm                     {}
    | RefOfTerm                     {}
    | DerefOfTerm                   {}
    | IndexTerm                     {}
    | IndexExpTerm                  {}
    ;

/* For DeRefOf(), SuperName except for DerefOf and Debug */

DerefOfSource
    : SimpleName                    {}
    | RefOfTerm                     {}
    | DerefOfTerm                   {}
    | IndexTerm                     {}
    | IndexExpTerm                  {}
    | StoreTerm                     {}
    | EqualsTerm                    {}
    | MethodInvocationTerm          {}
    ;

/* For RefOf(), SuperName except for RefOf and MethodInvocationTerm */

RefOfSource
    : SimpleName                    {}
    | DebugTerm                     {}
    | DerefOfTerm                   {}
    | IndexTerm                     {}
    | IndexExpTerm                  {}
    ;

/* For CondRefOf(), SuperName except for RefOf and MethodInvocationTerm */

CondRefOfSource
    : SimpleName                    {}
    | DebugTerm                     {}
    | DerefOfTerm                   {}
    | IndexTerm                     {}
    | IndexExpTerm                  {}
    ;

/*
 * Opcode types, as defined in the ACPI specification
 */
Type1Opcode
    : BreakTerm                     {}
    | BreakPointTerm                {}
    | ContinueTerm                  {}
    | FatalTerm                     {}
    | ForTerm                       {}
    | ElseIfTerm                    {}
    | LoadTerm                      {}
    | NoOpTerm                      {}
    | NotifyTerm                    {}
    | ReleaseTerm                   {}
    | ResetTerm                     {}
    | ReturnTerm                    {}
    | SignalTerm                    {}
    | SleepTerm                     {}
    | StallTerm                     {}
    | SwitchTerm                    {}
    | UnloadTerm                    {}
    | WhileTerm                     {}
    ;

Type2Opcode
    : AcquireTerm                   {}
    | CondRefOfTerm                 {}
    | CopyObjectTerm                {}
    | DerefOfTerm                   {}
    | ObjectTypeTerm                {}
    | RefOfTerm                     {}
    | SizeOfTerm                    {}
    | StoreTerm                     {}
    | EqualsTerm                    {}
    | TimerTerm                     {}
    | WaitTerm                      {}
    | MethodInvocationTerm          {}
    ;

/*
 * Type 3/4/5 opcodes
 */
Type2IntegerOpcode                  /* "Type3" opcodes */
    : Expression                    {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);}
    | AddTerm                       {}
    | AndTerm                       {}
    | DecTerm                       {}
    | DivideTerm                    {}
    | FindSetLeftBitTerm            {}
    | FindSetRightBitTerm           {}
    | FromBCDTerm                   {}
    | IncTerm                       {}
    | IndexTerm                     {}
//    | StructureIndexTerm            {}
//    | StructurePointerTerm          {}
    | LAndTerm                      {}
    | LEqualTerm                    {}
    | LGreaterTerm                  {}
    | LGreaterEqualTerm             {}
    | LLessTerm                     {}
    | LLessEqualTerm                {}
    | LNotTerm                      {}
    | LNotEqualTerm                 {}
    | LoadTableTerm                 {}
    | LOrTerm                       {}
    | MatchTerm                     {}
    | ModTerm                       {}
    | MultiplyTerm                  {}
    | NAndTerm                      {}
    | NOrTerm                       {}
    | NotTerm                       {}
    | OrTerm                        {}
    | ShiftLeftTerm                 {}
    | ShiftRightTerm                {}
    | SubtractTerm                  {}
    | ToBCDTerm                     {}
    | ToIntegerTerm                 {}
    | XOrTerm                       {}
    ;

Type2StringOpcode                   /* "Type4" Opcodes */
    : ToDecimalStringTerm           {}
    | ToHexStringTerm               {}
    | ToStringTerm                  {}
    ;

Type2BufferOpcode                   /* "Type5" Opcodes */
    : ToBufferTerm                  {}
    | ConcatResTerm                 {}
    ;

Type2BufferOrStringOpcode
    : ConcatTerm                    {$$ = TrSetNodeFlags ($1, NODE_COMPILE_TIME_CONST);}
    | PrintfTerm                    {}
    | FprintfTerm                   {}
    | MidTerm                       {}
    ;

/*
 * A type 3 opcode evaluates to an Integer and cannot have a destination operand
 */
Type3Opcode
    : EISAIDTerm                    {}
    ;

/* Obsolete
Type4Opcode
    : ConcatTerm                    {}
    | ToDecimalStringTerm           {}
    | ToHexStringTerm               {}
    | MidTerm                       {}
    | ToStringTerm                  {}
    ;
*/

/* Type 5 opcodes are a subset of Type2 opcodes, and return a constant */

Type5Opcode
    : ResourceTemplateTerm          {}
    | UnicodeTerm                   {}
    | ToPLDTerm                     {}
    | ToUUIDTerm                    {}
    ;

Type6Opcode
    : RefOfTerm                     {}
    | DerefOfTerm                   {}
    | IndexTerm                     {}
    | IndexExpTerm                  {}
//    | StructureIndexTerm            {}
//    | StructurePointerTerm          {}
    | MethodInvocationTerm          {}
    ;


/*******************************************************************************
 *
 * ASL Helper Terms
 *
 ******************************************************************************/

AmlPackageLengthTerm
    : Integer                       {$$ = TrUpdateNode (PARSEOP_PACKAGE_LENGTH,
                                        (ACPI_PARSE_OBJECT *) $1);}
    ;

NameStringItem
    : ',' NameString                {$$ = $2;}
    | ',' error                     {$$ = AslDoError (); yyclearin;}
    ;

TermArgItem
    : ',' TermArg                   {$$ = $2;}
    | ',' error                     {$$ = AslDoError (); yyclearin;}
    ;

OptionalReference
    :                               {$$ = TrCreateLeafNode (PARSEOP_ZERO);}       /* Placeholder is a ZeroOp object */
    | ','                           {$$ = TrCreateLeafNode (PARSEOP_ZERO);}       /* Placeholder is a ZeroOp object */
    | ',' TermArg                   {$$ = $2;}
    ;

OptionalReturnArg
    :                               {$$ = TrSetNodeFlags (TrCreateLeafNode (PARSEOP_ZERO),
                                            NODE_IS_NULL_RETURN);}       /* Placeholder is a ZeroOp object */
    | TermArg                       {$$ = $1;}
    ;

OptionalSerializeRuleKeyword
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' SerializeRuleKeyword      {$$ = $2;}
    ;

OptionalTermArg
    :                               {$$ = TrCreateLeafNode (PARSEOP_DEFAULT_ARG);}
    | TermArg                       {$$ = $1;}
    ;

OptionalWordConst
    :                               {$$ = NULL;}
    | WordConst                     {$$ = $1;}
    ;
