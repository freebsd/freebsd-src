NoEcho('
/******************************************************************************
 *
 * Module Name: aslresources.y - Bison/Yacc production rules for resources
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
 * ASL Parameter Keyword Terms
 *
 ******************************************************************************/

AccessAttribKeyword
    : PARSEOP_ACCESSATTRIB_BLOCK            {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_BLOCK);}
    | PARSEOP_ACCESSATTRIB_BLOCK_CALL       {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_BLOCK_CALL);}
    | PARSEOP_ACCESSATTRIB_BYTE             {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_BYTE);}
    | PARSEOP_ACCESSATTRIB_QUICK            {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_QUICK );}
    | PARSEOP_ACCESSATTRIB_SND_RCV          {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_SND_RCV);}
    | PARSEOP_ACCESSATTRIB_WORD             {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_WORD);}
    | PARSEOP_ACCESSATTRIB_WORD_CALL        {$$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_WORD_CALL);}
    | PARSEOP_ACCESSATTRIB_MULTIBYTE '('    {$<n>$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_MULTIBYTE);}
        ByteConst
        ')'                                 {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_ACCESSATTRIB_RAW_BYTES '('    {$<n>$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_RAW_BYTES);}
        ByteConst
        ')'                                 {$$ = TrLinkChildren ($<n>3,1,$4);}
    | PARSEOP_ACCESSATTRIB_RAW_PROCESS '('  {$<n>$ = TrCreateLeafNode (PARSEOP_ACCESSATTRIB_RAW_PROCESS);}
        ByteConst
        ')'                                 {$$ = TrLinkChildren ($<n>3,1,$4);}
    ;

AccessTypeKeyword
    : PARSEOP_ACCESSTYPE_ANY                {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_ANY);}
    | PARSEOP_ACCESSTYPE_BYTE               {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_BYTE);}
    | PARSEOP_ACCESSTYPE_WORD               {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_WORD);}
    | PARSEOP_ACCESSTYPE_DWORD              {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_DWORD);}
    | PARSEOP_ACCESSTYPE_QWORD              {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_QWORD);}
    | PARSEOP_ACCESSTYPE_BUF                {$$ = TrCreateLeafNode (PARSEOP_ACCESSTYPE_BUF);}
    ;

AddressingModeKeyword
    : PARSEOP_ADDRESSINGMODE_7BIT           {$$ = TrCreateLeafNode (PARSEOP_ADDRESSINGMODE_7BIT);}
    | PARSEOP_ADDRESSINGMODE_10BIT          {$$ = TrCreateLeafNode (PARSEOP_ADDRESSINGMODE_10BIT);}
    ;

AddressKeyword
    : PARSEOP_ADDRESSTYPE_MEMORY            {$$ = TrCreateLeafNode (PARSEOP_ADDRESSTYPE_MEMORY);}
    | PARSEOP_ADDRESSTYPE_RESERVED          {$$ = TrCreateLeafNode (PARSEOP_ADDRESSTYPE_RESERVED);}
    | PARSEOP_ADDRESSTYPE_NVS               {$$ = TrCreateLeafNode (PARSEOP_ADDRESSTYPE_NVS);}
    | PARSEOP_ADDRESSTYPE_ACPI              {$$ = TrCreateLeafNode (PARSEOP_ADDRESSTYPE_ACPI);}
    ;

AddressSpaceKeyword
    : ByteConst                             {$$ = UtCheckIntegerRange ($1, 0x0A, 0xFF);}
    | RegionSpaceKeyword                    {}
    ;

BitsPerByteKeyword
    : PARSEOP_BITSPERBYTE_FIVE              {$$ = TrCreateLeafNode (PARSEOP_BITSPERBYTE_FIVE);}
    | PARSEOP_BITSPERBYTE_SIX               {$$ = TrCreateLeafNode (PARSEOP_BITSPERBYTE_SIX);}
    | PARSEOP_BITSPERBYTE_SEVEN             {$$ = TrCreateLeafNode (PARSEOP_BITSPERBYTE_SEVEN);}
    | PARSEOP_BITSPERBYTE_EIGHT             {$$ = TrCreateLeafNode (PARSEOP_BITSPERBYTE_EIGHT);}
    | PARSEOP_BITSPERBYTE_NINE              {$$ = TrCreateLeafNode (PARSEOP_BITSPERBYTE_NINE);}
    ;

ClockPhaseKeyword
    : PARSEOP_CLOCKPHASE_FIRST              {$$ = TrCreateLeafNode (PARSEOP_CLOCKPHASE_FIRST);}
    | PARSEOP_CLOCKPHASE_SECOND             {$$ = TrCreateLeafNode (PARSEOP_CLOCKPHASE_SECOND);}
    ;

ClockPolarityKeyword
    : PARSEOP_CLOCKPOLARITY_LOW             {$$ = TrCreateLeafNode (PARSEOP_CLOCKPOLARITY_LOW);}
    | PARSEOP_CLOCKPOLARITY_HIGH            {$$ = TrCreateLeafNode (PARSEOP_CLOCKPOLARITY_HIGH);}
    ;

DecodeKeyword
    : PARSEOP_DECODETYPE_POS                {$$ = TrCreateLeafNode (PARSEOP_DECODETYPE_POS);}
    | PARSEOP_DECODETYPE_SUB                {$$ = TrCreateLeafNode (PARSEOP_DECODETYPE_SUB);}
    ;

DevicePolarityKeyword
    : PARSEOP_DEVICEPOLARITY_LOW            {$$ = TrCreateLeafNode (PARSEOP_DEVICEPOLARITY_LOW);}
    | PARSEOP_DEVICEPOLARITY_HIGH           {$$ = TrCreateLeafNode (PARSEOP_DEVICEPOLARITY_HIGH);}
    ;

DMATypeKeyword
    : PARSEOP_DMATYPE_A                     {$$ = TrCreateLeafNode (PARSEOP_DMATYPE_A);}
    | PARSEOP_DMATYPE_COMPATIBILITY         {$$ = TrCreateLeafNode (PARSEOP_DMATYPE_COMPATIBILITY);}
    | PARSEOP_DMATYPE_B                     {$$ = TrCreateLeafNode (PARSEOP_DMATYPE_B);}
    | PARSEOP_DMATYPE_F                     {$$ = TrCreateLeafNode (PARSEOP_DMATYPE_F);}
    ;

EndianKeyword
    : PARSEOP_ENDIAN_LITTLE                 {$$ = TrCreateLeafNode (PARSEOP_ENDIAN_LITTLE);}
    | PARSEOP_ENDIAN_BIG                    {$$ = TrCreateLeafNode (PARSEOP_ENDIAN_BIG);}
    ;

FlowControlKeyword
    : PARSEOP_FLOWCONTROL_HW                {$$ = TrCreateLeafNode (PARSEOP_FLOWCONTROL_HW);}
    | PARSEOP_FLOWCONTROL_NONE              {$$ = TrCreateLeafNode (PARSEOP_FLOWCONTROL_NONE);}
    | PARSEOP_FLOWCONTROL_SW                {$$ = TrCreateLeafNode (PARSEOP_FLOWCONTROL_SW);}
    ;

InterruptLevel
    : PARSEOP_INTLEVEL_ACTIVEBOTH           {$$ = TrCreateLeafNode (PARSEOP_INTLEVEL_ACTIVEBOTH);}
    | PARSEOP_INTLEVEL_ACTIVEHIGH           {$$ = TrCreateLeafNode (PARSEOP_INTLEVEL_ACTIVEHIGH);}
    | PARSEOP_INTLEVEL_ACTIVELOW            {$$ = TrCreateLeafNode (PARSEOP_INTLEVEL_ACTIVELOW);}
    ;

InterruptTypeKeyword
    : PARSEOP_INTTYPE_EDGE                  {$$ = TrCreateLeafNode (PARSEOP_INTTYPE_EDGE);}
    | PARSEOP_INTTYPE_LEVEL                 {$$ = TrCreateLeafNode (PARSEOP_INTTYPE_LEVEL);}
    ;

IODecodeKeyword
    : PARSEOP_IODECODETYPE_16               {$$ = TrCreateLeafNode (PARSEOP_IODECODETYPE_16);}
    | PARSEOP_IODECODETYPE_10               {$$ = TrCreateLeafNode (PARSEOP_IODECODETYPE_10);}
    ;

IoRestrictionKeyword
    : PARSEOP_IORESTRICT_IN                 {$$ = TrCreateLeafNode (PARSEOP_IORESTRICT_IN);}
    | PARSEOP_IORESTRICT_OUT                {$$ = TrCreateLeafNode (PARSEOP_IORESTRICT_OUT);}
    | PARSEOP_IORESTRICT_NONE               {$$ = TrCreateLeafNode (PARSEOP_IORESTRICT_NONE);}
    | PARSEOP_IORESTRICT_PRESERVE           {$$ = TrCreateLeafNode (PARSEOP_IORESTRICT_PRESERVE);}
    ;

LockRuleKeyword
    : PARSEOP_LOCKRULE_LOCK                 {$$ = TrCreateLeafNode (PARSEOP_LOCKRULE_LOCK);}
    | PARSEOP_LOCKRULE_NOLOCK               {$$ = TrCreateLeafNode (PARSEOP_LOCKRULE_NOLOCK);}
    ;

MatchOpKeyword
    : PARSEOP_MATCHTYPE_MTR                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MTR);}
    | PARSEOP_MATCHTYPE_MEQ                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MEQ);}
    | PARSEOP_MATCHTYPE_MLE                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MLE);}
    | PARSEOP_MATCHTYPE_MLT                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MLT);}
    | PARSEOP_MATCHTYPE_MGE                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MGE);}
    | PARSEOP_MATCHTYPE_MGT                 {$$ = TrCreateLeafNode (PARSEOP_MATCHTYPE_MGT);}
    ;

MaxKeyword
    : PARSEOP_MAXTYPE_FIXED                 {$$ = TrCreateLeafNode (PARSEOP_MAXTYPE_FIXED);}
    | PARSEOP_MAXTYPE_NOTFIXED              {$$ = TrCreateLeafNode (PARSEOP_MAXTYPE_NOTFIXED);}
    ;

MemTypeKeyword
    : PARSEOP_MEMTYPE_CACHEABLE             {$$ = TrCreateLeafNode (PARSEOP_MEMTYPE_CACHEABLE);}
    | PARSEOP_MEMTYPE_WRITECOMBINING        {$$ = TrCreateLeafNode (PARSEOP_MEMTYPE_WRITECOMBINING);}
    | PARSEOP_MEMTYPE_PREFETCHABLE          {$$ = TrCreateLeafNode (PARSEOP_MEMTYPE_PREFETCHABLE);}
    | PARSEOP_MEMTYPE_NONCACHEABLE          {$$ = TrCreateLeafNode (PARSEOP_MEMTYPE_NONCACHEABLE);}
    ;

MinKeyword
    : PARSEOP_MINTYPE_FIXED                 {$$ = TrCreateLeafNode (PARSEOP_MINTYPE_FIXED);}
    | PARSEOP_MINTYPE_NOTFIXED              {$$ = TrCreateLeafNode (PARSEOP_MINTYPE_NOTFIXED);}
    ;

ObjectTypeKeyword
    : PARSEOP_OBJECTTYPE_UNK                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_UNK);}
    | PARSEOP_OBJECTTYPE_INT                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_INT);}
    | PARSEOP_OBJECTTYPE_STR                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_STR);}
    | PARSEOP_OBJECTTYPE_BUF                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_BUF);}
    | PARSEOP_OBJECTTYPE_PKG                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_PKG);}
    | PARSEOP_OBJECTTYPE_FLD                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_FLD);}
    | PARSEOP_OBJECTTYPE_DEV                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_DEV);}
    | PARSEOP_OBJECTTYPE_EVT                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_EVT);}
    | PARSEOP_OBJECTTYPE_MTH                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_MTH);}
    | PARSEOP_OBJECTTYPE_MTX                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_MTX);}
    | PARSEOP_OBJECTTYPE_OPR                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_OPR);}
    | PARSEOP_OBJECTTYPE_POW                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_POW);}
    | PARSEOP_OBJECTTYPE_PRO                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_PRO);}
    | PARSEOP_OBJECTTYPE_THZ                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_THZ);}
    | PARSEOP_OBJECTTYPE_BFF                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_BFF);}
    | PARSEOP_OBJECTTYPE_DDB                {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_DDB);}
    ;

ParityTypeKeyword
    : PARSEOP_PARITYTYPE_SPACE              {$$ = TrCreateLeafNode (PARSEOP_PARITYTYPE_SPACE);}
    | PARSEOP_PARITYTYPE_MARK               {$$ = TrCreateLeafNode (PARSEOP_PARITYTYPE_MARK);}
    | PARSEOP_PARITYTYPE_ODD                {$$ = TrCreateLeafNode (PARSEOP_PARITYTYPE_ODD);}
    | PARSEOP_PARITYTYPE_EVEN               {$$ = TrCreateLeafNode (PARSEOP_PARITYTYPE_EVEN);}
    | PARSEOP_PARITYTYPE_NONE               {$$ = TrCreateLeafNode (PARSEOP_PARITYTYPE_NONE);}
    ;

PinConfigByte
    : PinConfigKeyword                      {$$ = $1;}
    | ByteConstExpr                         {$$ = UtCheckIntegerRange ($1, 0x80, 0xFF);}
    ;

PinConfigKeyword
    : PARSEOP_PIN_NOPULL                    {$$ = TrCreateLeafNode (PARSEOP_PIN_NOPULL);}
    | PARSEOP_PIN_PULLDOWN                  {$$ = TrCreateLeafNode (PARSEOP_PIN_PULLDOWN);}
    | PARSEOP_PIN_PULLUP                    {$$ = TrCreateLeafNode (PARSEOP_PIN_PULLUP);}
    | PARSEOP_PIN_PULLDEFAULT               {$$ = TrCreateLeafNode (PARSEOP_PIN_PULLDEFAULT);}
    ;

PldKeyword
    : PARSEOP_PLD_REVISION                  {$$ = TrCreateLeafNode (PARSEOP_PLD_REVISION);}
    | PARSEOP_PLD_IGNORECOLOR               {$$ = TrCreateLeafNode (PARSEOP_PLD_IGNORECOLOR);}
    | PARSEOP_PLD_RED                       {$$ = TrCreateLeafNode (PARSEOP_PLD_RED);}
    | PARSEOP_PLD_GREEN                     {$$ = TrCreateLeafNode (PARSEOP_PLD_GREEN);}
    | PARSEOP_PLD_BLUE                      {$$ = TrCreateLeafNode (PARSEOP_PLD_BLUE);}
    | PARSEOP_PLD_WIDTH                     {$$ = TrCreateLeafNode (PARSEOP_PLD_WIDTH);}
    | PARSEOP_PLD_HEIGHT                    {$$ = TrCreateLeafNode (PARSEOP_PLD_HEIGHT);}
    | PARSEOP_PLD_USERVISIBLE               {$$ = TrCreateLeafNode (PARSEOP_PLD_USERVISIBLE);}
    | PARSEOP_PLD_DOCK                      {$$ = TrCreateLeafNode (PARSEOP_PLD_DOCK);}
    | PARSEOP_PLD_LID                       {$$ = TrCreateLeafNode (PARSEOP_PLD_LID);}
    | PARSEOP_PLD_PANEL                     {$$ = TrCreateLeafNode (PARSEOP_PLD_PANEL);}
    | PARSEOP_PLD_VERTICALPOSITION          {$$ = TrCreateLeafNode (PARSEOP_PLD_VERTICALPOSITION);}
    | PARSEOP_PLD_HORIZONTALPOSITION        {$$ = TrCreateLeafNode (PARSEOP_PLD_HORIZONTALPOSITION);}
    | PARSEOP_PLD_SHAPE                     {$$ = TrCreateLeafNode (PARSEOP_PLD_SHAPE);}
    | PARSEOP_PLD_GROUPORIENTATION          {$$ = TrCreateLeafNode (PARSEOP_PLD_GROUPORIENTATION);}
    | PARSEOP_PLD_GROUPTOKEN                {$$ = TrCreateLeafNode (PARSEOP_PLD_GROUPTOKEN);}
    | PARSEOP_PLD_GROUPPOSITION             {$$ = TrCreateLeafNode (PARSEOP_PLD_GROUPPOSITION);}
    | PARSEOP_PLD_BAY                       {$$ = TrCreateLeafNode (PARSEOP_PLD_BAY);}
    | PARSEOP_PLD_EJECTABLE                 {$$ = TrCreateLeafNode (PARSEOP_PLD_EJECTABLE);}
    | PARSEOP_PLD_EJECTREQUIRED             {$$ = TrCreateLeafNode (PARSEOP_PLD_EJECTREQUIRED);}
    | PARSEOP_PLD_CABINETNUMBER             {$$ = TrCreateLeafNode (PARSEOP_PLD_CABINETNUMBER);}
    | PARSEOP_PLD_CARDCAGENUMBER            {$$ = TrCreateLeafNode (PARSEOP_PLD_CARDCAGENUMBER);}
    | PARSEOP_PLD_REFERENCE                 {$$ = TrCreateLeafNode (PARSEOP_PLD_REFERENCE);}
    | PARSEOP_PLD_ROTATION                  {$$ = TrCreateLeafNode (PARSEOP_PLD_ROTATION);}
    | PARSEOP_PLD_ORDER                     {$$ = TrCreateLeafNode (PARSEOP_PLD_ORDER);}
    | PARSEOP_PLD_RESERVED                  {$$ = TrCreateLeafNode (PARSEOP_PLD_RESERVED);}
    | PARSEOP_PLD_VERTICALOFFSET            {$$ = TrCreateLeafNode (PARSEOP_PLD_VERTICALOFFSET);}
    | PARSEOP_PLD_HORIZONTALOFFSET          {$$ = TrCreateLeafNode (PARSEOP_PLD_HORIZONTALOFFSET);}
    ;

RangeTypeKeyword
    : PARSEOP_RANGETYPE_ISAONLY             {$$ = TrCreateLeafNode (PARSEOP_RANGETYPE_ISAONLY);}
    | PARSEOP_RANGETYPE_NONISAONLY          {$$ = TrCreateLeafNode (PARSEOP_RANGETYPE_NONISAONLY);}
    | PARSEOP_RANGETYPE_ENTIRE              {$$ = TrCreateLeafNode (PARSEOP_RANGETYPE_ENTIRE);}
    ;

RegionSpaceKeyword
    : PARSEOP_REGIONSPACE_IO                {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_IO);}
    | PARSEOP_REGIONSPACE_MEM               {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_MEM);}
    | PARSEOP_REGIONSPACE_PCI               {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_PCI);}
    | PARSEOP_REGIONSPACE_EC                {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_EC);}
    | PARSEOP_REGIONSPACE_SMBUS             {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_SMBUS);}
    | PARSEOP_REGIONSPACE_CMOS              {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_CMOS);}
    | PARSEOP_REGIONSPACE_PCIBAR            {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_PCIBAR);}
    | PARSEOP_REGIONSPACE_IPMI              {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_IPMI);}
    | PARSEOP_REGIONSPACE_GPIO              {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_GPIO);}
    | PARSEOP_REGIONSPACE_GSBUS             {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_GSBUS);}
    | PARSEOP_REGIONSPACE_PCC               {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_PCC);}
    | PARSEOP_REGIONSPACE_FFIXEDHW          {$$ = TrCreateLeafNode (PARSEOP_REGIONSPACE_FFIXEDHW);}
    ;

ResourceTypeKeyword
    : PARSEOP_RESOURCETYPE_CONSUMER         {$$ = TrCreateLeafNode (PARSEOP_RESOURCETYPE_CONSUMER);}
    | PARSEOP_RESOURCETYPE_PRODUCER         {$$ = TrCreateLeafNode (PARSEOP_RESOURCETYPE_PRODUCER);}
    ;

SerializeRuleKeyword
    : PARSEOP_SERIALIZERULE_SERIAL          {$$ = TrCreateLeafNode (PARSEOP_SERIALIZERULE_SERIAL);}
    | PARSEOP_SERIALIZERULE_NOTSERIAL       {$$ = TrCreateLeafNode (PARSEOP_SERIALIZERULE_NOTSERIAL);}
    ;

ShareTypeKeyword
    : PARSEOP_SHARETYPE_SHARED              {$$ = TrCreateLeafNode (PARSEOP_SHARETYPE_SHARED);}
    | PARSEOP_SHARETYPE_EXCLUSIVE           {$$ = TrCreateLeafNode (PARSEOP_SHARETYPE_EXCLUSIVE);}
    | PARSEOP_SHARETYPE_SHAREDWAKE          {$$ = TrCreateLeafNode (PARSEOP_SHARETYPE_SHAREDWAKE);}
    | PARSEOP_SHARETYPE_EXCLUSIVEWAKE       {$$ = TrCreateLeafNode (PARSEOP_SHARETYPE_EXCLUSIVEWAKE);}
   ;

SlaveModeKeyword
    : PARSEOP_SLAVEMODE_CONTROLLERINIT      {$$ = TrCreateLeafNode (PARSEOP_SLAVEMODE_CONTROLLERINIT);}
    | PARSEOP_SLAVEMODE_DEVICEINIT          {$$ = TrCreateLeafNode (PARSEOP_SLAVEMODE_DEVICEINIT);}
    ;

StopBitsKeyword
    : PARSEOP_STOPBITS_TWO                  {$$ = TrCreateLeafNode (PARSEOP_STOPBITS_TWO);}
    | PARSEOP_STOPBITS_ONEPLUSHALF          {$$ = TrCreateLeafNode (PARSEOP_STOPBITS_ONEPLUSHALF);}
    | PARSEOP_STOPBITS_ONE                  {$$ = TrCreateLeafNode (PARSEOP_STOPBITS_ONE);}
    | PARSEOP_STOPBITS_ZERO                 {$$ = TrCreateLeafNode (PARSEOP_STOPBITS_ZERO);}
    ;

TranslationKeyword
    : PARSEOP_TRANSLATIONTYPE_SPARSE        {$$ = TrCreateLeafNode (PARSEOP_TRANSLATIONTYPE_SPARSE);}
    | PARSEOP_TRANSLATIONTYPE_DENSE         {$$ = TrCreateLeafNode (PARSEOP_TRANSLATIONTYPE_DENSE);}
    ;

TypeKeyword
    : PARSEOP_TYPE_TRANSLATION              {$$ = TrCreateLeafNode (PARSEOP_TYPE_TRANSLATION);}
    | PARSEOP_TYPE_STATIC                   {$$ = TrCreateLeafNode (PARSEOP_TYPE_STATIC);}
    ;

UpdateRuleKeyword
    : PARSEOP_UPDATERULE_PRESERVE           {$$ = TrCreateLeafNode (PARSEOP_UPDATERULE_PRESERVE);}
    | PARSEOP_UPDATERULE_ONES               {$$ = TrCreateLeafNode (PARSEOP_UPDATERULE_ONES);}
    | PARSEOP_UPDATERULE_ZEROS              {$$ = TrCreateLeafNode (PARSEOP_UPDATERULE_ZEROS);}
    ;

WireModeKeyword
    : PARSEOP_WIREMODE_FOUR                 {$$ = TrCreateLeafNode (PARSEOP_WIREMODE_FOUR);}
    | PARSEOP_WIREMODE_THREE                {$$ = TrCreateLeafNode (PARSEOP_WIREMODE_THREE);}
    ;

XferSizeKeyword
    : PARSEOP_XFERSIZE_8                    {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_8,   0);}
    | PARSEOP_XFERSIZE_16                   {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_16,  1);}
    | PARSEOP_XFERSIZE_32                   {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_32,  2);}
    | PARSEOP_XFERSIZE_64                   {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_64,  3);}
    | PARSEOP_XFERSIZE_128                  {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_128, 4);}
    | PARSEOP_XFERSIZE_256                  {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_256, 5);}
    ;

XferTypeKeyword
    : PARSEOP_XFERTYPE_8                    {$$ = TrCreateLeafNode (PARSEOP_XFERTYPE_8);}
    | PARSEOP_XFERTYPE_8_16                 {$$ = TrCreateLeafNode (PARSEOP_XFERTYPE_8_16);}
    | PARSEOP_XFERTYPE_16                   {$$ = TrCreateLeafNode (PARSEOP_XFERTYPE_16);}
    ;


/*******************************************************************************
 *
 * ASL Resource Template Terms
 *
 ******************************************************************************/

/*
 * Note: Create two default nodes to allow conversion to a Buffer AML opcode
 * Also, insert the EndTag at the end of the template.
 */
ResourceTemplateTerm
    : PARSEOP_RESOURCETEMPLATE '(' ')'
        '{'
        ResourceMacroList '}'       {$$ = TrCreateNode (PARSEOP_RESOURCETEMPLATE,4,
                                          TrCreateLeafNode (PARSEOP_DEFAULT_ARG),
                                          TrCreateLeafNode (PARSEOP_DEFAULT_ARG),
                                          $5,
                                          TrCreateLeafNode (PARSEOP_ENDTAG));}
    ;

ResourceMacroList
    :                               {$$ = NULL;}
    | ResourceMacroList
        ResourceMacroTerm           {$$ = TrLinkPeerNode ($1,$2);}
    ;

ResourceMacroTerm
    : DMATerm                       {}
    | DWordIOTerm                   {}
    | DWordMemoryTerm               {}
    | DWordSpaceTerm                {}
    | EndDependentFnTerm            {}
    | ExtendedIOTerm                {}
    | ExtendedMemoryTerm            {}
    | ExtendedSpaceTerm             {}
    | FixedDmaTerm                  {}
    | FixedIOTerm                   {}
    | GpioIntTerm                   {}
    | GpioIoTerm                    {}
    | I2cSerialBusTerm              {}
    | I2cSerialBusTermV2            {}
    | InterruptTerm                 {}
    | IOTerm                        {}
    | IRQNoFlagsTerm                {}
    | IRQTerm                       {}
    | Memory24Term                  {}
    | Memory32FixedTerm             {}
    | Memory32Term                  {}
    | QWordIOTerm                   {}
    | QWordMemoryTerm               {}
    | QWordSpaceTerm                {}
    | RegisterTerm                  {}
    | SpiSerialBusTerm              {}
    | SpiSerialBusTermV2            {}
    | StartDependentFnNoPriTerm     {}
    | StartDependentFnTerm          {}
    | UartSerialBusTerm             {}
    | UartSerialBusTermV2           {}
    | VendorLongTerm                {}
    | VendorShortTerm               {}
    | WordBusNumberTerm             {}
    | WordIOTerm                    {}
    | WordSpaceTerm                 {}
    ;

DMATerm
    : PARSEOP_DMA '('               {$<n>$ = TrCreateLeafNode (PARSEOP_DMA);}
        DMATypeKeyword
        OptionalBusMasterKeyword
        ',' XferTypeKeyword
        OptionalNameString_Last
        ')' '{'
            ByteList '}'            {$$ = TrLinkChildren ($<n>3,5,$4,$5,$7,$8,$11);}
    | PARSEOP_DMA '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

DWordIOTerm
    : PARSEOP_DWORDIO '('           {$<n>$ = TrCreateLeafNode (PARSEOP_DWORDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,15,$4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22,$23);}
    | PARSEOP_DWORDIO '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

DWordMemoryTerm
    : PARSEOP_DWORDMEMORY '('       {$<n>$ = TrCreateLeafNode (PARSEOP_DWORDMEMORY);}
        OptionalResourceType_First
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        OptionalMemType
        ',' OptionalReadWriteKeyword
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalAddressRange
        OptionalType_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,16,$4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$20,$21,$22,$23,$24,$25);}
    | PARSEOP_DWORDMEMORY '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

DWordSpaceTerm
    : PARSEOP_DWORDSPACE '('        {$<n>$ = TrCreateLeafNode (PARSEOP_DWORDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,14,$4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23,$24);}
    | PARSEOP_DWORDSPACE '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

EndDependentFnTerm
    : PARSEOP_ENDDEPENDENTFN '('
        ')'                         {$$ = TrCreateLeafNode (PARSEOP_ENDDEPENDENTFN);}
    | PARSEOP_ENDDEPENDENTFN '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

ExtendedIOTerm
    : PARSEOP_EXTENDEDIO '('        {$<n>$ = TrCreateLeafNode (PARSEOP_EXTENDEDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalQWordConstExpr
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,14,$4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22);}
    | PARSEOP_EXTENDEDIO '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

ExtendedMemoryTerm
    : PARSEOP_EXTENDEDMEMORY '('    {$<n>$ = TrCreateLeafNode (PARSEOP_EXTENDEDMEMORY);}
        OptionalResourceType_First
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        OptionalMemType
        ',' OptionalReadWriteKeyword
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalQWordConstExpr
        OptionalNameString
        OptionalAddressRange
        OptionalType_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,15,$4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$20,$21,$22,$23,$24);}
    | PARSEOP_EXTENDEDMEMORY '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

ExtendedSpaceTerm
    : PARSEOP_EXTENDEDSPACE '('     {$<n>$ = TrCreateLeafNode (PARSEOP_EXTENDEDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalQWordConstExpr
        OptionalNameString_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,13,$4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23);}
    | PARSEOP_EXTENDEDSPACE '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

FixedDmaTerm
    : PARSEOP_FIXEDDMA '('          {$<n>$ = TrCreateLeafNode (PARSEOP_FIXEDDMA);}
        WordConstExpr               /* 04: DMA RequestLines */
        ',' WordConstExpr           /* 06: DMA Channels */
        OptionalXferSize            /* 07: DMA TransferSize */
        OptionalNameString          /* 08: DescriptorName */
        ')'                         {$$ = TrLinkChildren ($<n>3,4,$4,$6,$7,$8);}
    | PARSEOP_FIXEDDMA '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

FixedIOTerm
    : PARSEOP_FIXEDIO '('           {$<n>$ = TrCreateLeafNode (PARSEOP_FIXEDIO);}
        WordConstExpr
        ',' ByteConstExpr
        OptionalNameString_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,3,$4,$6,$7);}
    | PARSEOP_FIXEDIO '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

GpioIntTerm
    : PARSEOP_GPIO_INT '('          {$<n>$ = TrCreateLeafNode (PARSEOP_GPIO_INT);}
        InterruptTypeKeyword        /* 04: InterruptType */
        ',' InterruptLevel          /* 06: InterruptLevel */
        OptionalShareType           /* 07: SharedType */
        ',' PinConfigByte           /* 09: PinConfig */
        OptionalWordConstExpr       /* 10: DebounceTimeout */
        ',' StringData              /* 12: ResourceSource */
        OptionalByteConstExpr       /* 13: ResourceSourceIndex */
        OptionalResourceType        /* 14: ResourceType */
        OptionalNameString          /* 15: DescriptorName */
        OptionalBuffer_Last         /* 16: VendorData */
        ')' '{'
            DWordConstExpr '}'      {$$ = TrLinkChildren ($<n>3,11,$4,$6,$7,$9,$10,$12,$13,$14,$15,$16,$19);}
    | PARSEOP_GPIO_INT '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

GpioIoTerm
    : PARSEOP_GPIO_IO '('           {$<n>$ = TrCreateLeafNode (PARSEOP_GPIO_IO);}
        OptionalShareType_First     /* 04: SharedType */
        ',' PinConfigByte           /* 06: PinConfig */
        OptionalWordConstExpr       /* 07: DebounceTimeout */
        OptionalWordConstExpr       /* 08: DriveStrength */
        OptionalIoRestriction       /* 09: IoRestriction */
        ',' StringData              /* 11: ResourceSource */
        OptionalByteConstExpr       /* 12: ResourceSourceIndex */
        OptionalResourceType        /* 13: ResourceType */
        OptionalNameString          /* 14: DescriptorName */
        OptionalBuffer_Last         /* 15: VendorData */
        ')' '{'
            DWordList '}'           {$$ = TrLinkChildren ($<n>3,11,$4,$6,$7,$8,$9,$11,$12,$13,$14,$15,$18);}
    | PARSEOP_GPIO_IO '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

I2cSerialBusTerm
    : PARSEOP_I2C_SERIALBUS '('     {$<n>$ = TrCreateLeafNode (PARSEOP_I2C_SERIALBUS);}
        WordConstExpr               /* 04: SlaveAddress */
        OptionalSlaveMode           /* 05: SlaveMode */
        ',' DWordConstExpr          /* 07: ConnectionSpeed */
        OptionalAddressingMode      /* 08: AddressingMode */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalBuffer_Last         /* 14: VendorData */
        ')'                         {$$ = TrLinkChildren ($<n>3,10,$4,$5,$7,$8,$10,$11,$12,$13,
                                        TrCreateLeafNode (PARSEOP_DEFAULT_ARG),$14);}
    | PARSEOP_I2C_SERIALBUS '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

I2cSerialBusTermV2
    : PARSEOP_I2C_SERIALBUS_V2 '('  {$<n>$ = TrCreateLeafNode (PARSEOP_I2C_SERIALBUS_V2);}
        WordConstExpr               /* 04: SlaveAddress */
        OptionalSlaveMode           /* 05: SlaveMode */
        ',' DWordConstExpr          /* 07: ConnectionSpeed */
        OptionalAddressingMode      /* 08: AddressingMode */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalShareType           /* 14: Share */
        OptionalBuffer_Last         /* 15: VendorData */
        ')'                         {$$ = TrLinkChildren ($<n>3,10,$4,$5,$7,$8,$10,$11,$12,$13,
                                        $14,$15);}
    | PARSEOP_I2C_SERIALBUS_V2 '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

InterruptTerm
    : PARSEOP_INTERRUPT '('         {$<n>$ = TrCreateLeafNode (PARSEOP_INTERRUPT);}
        OptionalResourceType_First
        ',' InterruptTypeKeyword
        ',' InterruptLevel
        OptionalShareType
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        ')' '{'
            DWordList '}'           {$$ = TrLinkChildren ($<n>3,8,$4,$6,$8,$9,$10,$11,$12,$15);}
    | PARSEOP_INTERRUPT '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

IOTerm
    : PARSEOP_IO '('                {$<n>$ = TrCreateLeafNode (PARSEOP_IO);}
        IODecodeKeyword
        ',' WordConstExpr
        ',' WordConstExpr
        ',' ByteConstExpr
        ',' ByteConstExpr
        OptionalNameString_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,6,$4,$6,$8,$10,$12,$13);}
    | PARSEOP_IO '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

IRQNoFlagsTerm
    : PARSEOP_IRQNOFLAGS '('        {$<n>$ = TrCreateLeafNode (PARSEOP_IRQNOFLAGS);}
        OptionalNameString_First
        ')' '{'
            ByteList '}'            {$$ = TrLinkChildren ($<n>3,2,$4,$7);}
    | PARSEOP_IRQNOFLAGS '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

IRQTerm
    : PARSEOP_IRQ '('               {$<n>$ = TrCreateLeafNode (PARSEOP_IRQ);}
        InterruptTypeKeyword
        ',' InterruptLevel
        OptionalShareType
        OptionalNameString_Last
        ')' '{'
            ByteList '}'            {$$ = TrLinkChildren ($<n>3,5,$4,$6,$7,$8,$11);}
    | PARSEOP_IRQ '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

Memory24Term
    : PARSEOP_MEMORY24 '('          {$<n>$ = TrCreateLeafNode (PARSEOP_MEMORY24);}
        OptionalReadWriteKeyword
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalNameString_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,6,$4,$6,$8,$10,$12,$13);}
    | PARSEOP_MEMORY24 '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

Memory32FixedTerm
    : PARSEOP_MEMORY32FIXED '('     {$<n>$ = TrCreateLeafNode (PARSEOP_MEMORY32FIXED);}
        OptionalReadWriteKeyword
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalNameString_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,4,$4,$6,$8,$9);}
    | PARSEOP_MEMORY32FIXED '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

Memory32Term
    : PARSEOP_MEMORY32 '('          {$<n>$ = TrCreateLeafNode (PARSEOP_MEMORY32);}
        OptionalReadWriteKeyword
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalNameString_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,6,$4,$6,$8,$10,$12,$13);}
    | PARSEOP_MEMORY32 '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

QWordIOTerm
    : PARSEOP_QWORDIO '('           {$<n>$ = TrCreateLeafNode (PARSEOP_QWORDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,15,$4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22,$23);}
    | PARSEOP_QWORDIO '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

QWordMemoryTerm
    : PARSEOP_QWORDMEMORY '('       {$<n>$ = TrCreateLeafNode (PARSEOP_QWORDMEMORY);}
        OptionalResourceType_First
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        OptionalMemType
        ',' OptionalReadWriteKeyword
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalAddressRange
        OptionalType_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,16,$4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$20,$21,$22,$23,$24,$25);}
    | PARSEOP_QWORDMEMORY '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

QWordSpaceTerm
    : PARSEOP_QWORDSPACE '('        {$<n>$ = TrCreateLeafNode (PARSEOP_QWORDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,14,$4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23,$24);}
    | PARSEOP_QWORDSPACE '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

RegisterTerm
    : PARSEOP_REGISTER '('          {$<n>$ = TrCreateLeafNode (PARSEOP_REGISTER);}
        AddressSpaceKeyword
        ',' ByteConstExpr
        ',' ByteConstExpr
        ',' QWordConstExpr
        OptionalAccessSize
        OptionalNameString_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,6,$4,$6,$8,$10,$11,$12);}
    | PARSEOP_REGISTER '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

SpiSerialBusTerm
    : PARSEOP_SPI_SERIALBUS '('     {$<n>$ = TrCreateLeafNode (PARSEOP_SPI_SERIALBUS);}
        WordConstExpr               /* 04: DeviceSelection */
        OptionalDevicePolarity      /* 05: DevicePolarity */
        OptionalWireMode            /* 06: WireMode */
        ',' ByteConstExpr           /* 08: DataBitLength */
        OptionalSlaveMode           /* 09: SlaveMode */
        ',' DWordConstExpr          /* 11: ConnectionSpeed */
        ',' ClockPolarityKeyword    /* 13: ClockPolarity */
        ',' ClockPhaseKeyword       /* 15: ClockPhase */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalBuffer_Last         /* 21: VendorData */
        ')'                         {$$ = TrLinkChildren ($<n>3,14,$4,$5,$6,$8,$9,$11,$13,$15,$17,$18,$19,$20,
                                        TrCreateLeafNode (PARSEOP_DEFAULT_ARG),$21);}
    | PARSEOP_SPI_SERIALBUS '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

SpiSerialBusTermV2
    : PARSEOP_SPI_SERIALBUS_V2 '('  {$<n>$ = TrCreateLeafNode (PARSEOP_SPI_SERIALBUS_V2);}
        WordConstExpr               /* 04: DeviceSelection */
        OptionalDevicePolarity      /* 05: DevicePolarity */
        OptionalWireMode            /* 06: WireMode */
        ',' ByteConstExpr           /* 08: DataBitLength */
        OptionalSlaveMode           /* 09: SlaveMode */
        ',' DWordConstExpr          /* 11: ConnectionSpeed */
        ',' ClockPolarityKeyword    /* 13: ClockPolarity */
        ',' ClockPhaseKeyword       /* 15: ClockPhase */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalShareType           /* 21: Share */
        OptionalBuffer_Last         /* 22: VendorData */
        ')'                         {$$ = TrLinkChildren ($<n>3,14,$4,$5,$6,$8,$9,$11,$13,$15,$17,$18,$19,$20,
                                        $21,$22);}
    | PARSEOP_SPI_SERIALBUS_V2 '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

StartDependentFnNoPriTerm
    : PARSEOP_STARTDEPENDENTFN_NOPRI '('    {$<n>$ = TrCreateLeafNode (PARSEOP_STARTDEPENDENTFN_NOPRI);}
        ')' '{'
        ResourceMacroList '}'       {$$ = TrLinkChildren ($<n>3,1,$6);}
    | PARSEOP_STARTDEPENDENTFN_NOPRI '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

StartDependentFnTerm
    : PARSEOP_STARTDEPENDENTFN '('  {$<n>$ = TrCreateLeafNode (PARSEOP_STARTDEPENDENTFN);}
        ByteConstExpr
        ',' ByteConstExpr
        ')' '{'
        ResourceMacroList '}'       {$$ = TrLinkChildren ($<n>3,3,$4,$6,$9);}
    | PARSEOP_STARTDEPENDENTFN '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

UartSerialBusTerm
    : PARSEOP_UART_SERIALBUS '('    {$<n>$ = TrCreateLeafNode (PARSEOP_UART_SERIALBUS);}
        DWordConstExpr              /* 04: ConnectionSpeed */
        OptionalBitsPerByte         /* 05: BitsPerByte */
        OptionalStopBits            /* 06: StopBits */
        ',' ByteConstExpr           /* 08: LinesInUse */
        OptionalEndian              /* 09: Endianess */
        OptionalParityType          /* 10: Parity */
        OptionalFlowControl         /* 11: FlowControl */
        ',' WordConstExpr           /* 13: Rx BufferSize */
        ',' WordConstExpr           /* 15: Tx BufferSize */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalBuffer_Last         /* 21: VendorData */
        ')'                         {$$ = TrLinkChildren ($<n>3,15,$4,$5,$6,$8,$9,$10,$11,$13,$15,$17,$18,$19,$20,
                                        TrCreateLeafNode (PARSEOP_DEFAULT_ARG),$21);}
    | PARSEOP_UART_SERIALBUS '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

UartSerialBusTermV2
    : PARSEOP_UART_SERIALBUS_V2 '(' {$<n>$ = TrCreateLeafNode (PARSEOP_UART_SERIALBUS_V2);}
        DWordConstExpr              /* 04: ConnectionSpeed */
        OptionalBitsPerByte         /* 05: BitsPerByte */
        OptionalStopBits            /* 06: StopBits */
        ',' ByteConstExpr           /* 08: LinesInUse */
        OptionalEndian              /* 09: Endianess */
        OptionalParityType          /* 10: Parity */
        OptionalFlowControl         /* 11: FlowControl */
        ',' WordConstExpr           /* 13: Rx BufferSize */
        ',' WordConstExpr           /* 15: Tx BufferSize */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalShareType           /* 21: Share */
        OptionalBuffer_Last         /* 22: VendorData */
        ')'                         {$$ = TrLinkChildren ($<n>3,15,$4,$5,$6,$8,$9,$10,$11,$13,$15,$17,$18,$19,$20,
                                        $21,$22);}
    | PARSEOP_UART_SERIALBUS_V2 '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

VendorLongTerm
    : PARSEOP_VENDORLONG '('        {$<n>$ = TrCreateLeafNode (PARSEOP_VENDORLONG);}
        OptionalNameString_First
        ')' '{'
            ByteList '}'            {$$ = TrLinkChildren ($<n>3,2,$4,$7);}
    | PARSEOP_VENDORLONG '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

VendorShortTerm
    : PARSEOP_VENDORSHORT '('       {$<n>$ = TrCreateLeafNode (PARSEOP_VENDORSHORT);}
        OptionalNameString_First
        ')' '{'
            ByteList '}'            {$$ = TrLinkChildren ($<n>3,2,$4,$7);}
    | PARSEOP_VENDORSHORT '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

WordBusNumberTerm
    : PARSEOP_WORDBUSNUMBER '('     {$<n>$ = TrCreateLeafNode (PARSEOP_WORDBUSNUMBER);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,12,$4,$5,$6,$7,$9,$11,$13,$15,$17,$18,$19,$20);}
    | PARSEOP_WORDBUSNUMBER '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

WordIOTerm
    : PARSEOP_WORDIO '('            {$<n>$ = TrCreateLeafNode (PARSEOP_WORDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,15,$4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22,$23);}
    | PARSEOP_WORDIO '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;

WordSpaceTerm
    : PARSEOP_WORDSPACE '('         {$<n>$ = TrCreateLeafNode (PARSEOP_WORDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        ')'                         {$$ = TrLinkChildren ($<n>3,14,$4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23,$24);}
    | PARSEOP_WORDSPACE '('
        error ')'                   {$$ = AslDoError(); yyclearin;}
    ;


/******* Object References ***********************************************/

/* Allow IO, DMA, IRQ Resource macro names to also be used as identifiers */

NameString
    : NameSeg                       {}
    | PARSEOP_NAMESTRING            {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) AslCompilerlval.s);}
    | PARSEOP_IO                    {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) "IO");}
    | PARSEOP_DMA                   {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) "DMA");}
    | PARSEOP_IRQ                   {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESTRING, (ACPI_NATIVE_INT) "IRQ");}
    ;

NameSeg
    : PARSEOP_NAMESEG               {$$ = TrCreateValuedLeafNode (PARSEOP_NAMESEG, (ACPI_NATIVE_INT) AslCompilerlval.s);}
    ;


/*******************************************************************************
 *
 * ASL Helper Terms
 *
 ******************************************************************************/

OptionalBusMasterKeyword
    : ','                                       {$$ = TrCreateLeafNode (PARSEOP_BUSMASTERTYPE_MASTER);}
    | ',' PARSEOP_BUSMASTERTYPE_MASTER          {$$ = TrCreateLeafNode (PARSEOP_BUSMASTERTYPE_MASTER);}
    | ',' PARSEOP_BUSMASTERTYPE_NOTMASTER       {$$ = TrCreateLeafNode (PARSEOP_BUSMASTERTYPE_NOTMASTER);}
    ;

OptionalAccessAttribTerm
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ByteConstExpr             {$$ = $2;}
    | ',' AccessAttribKeyword       {$$ = $2;}
    ;

OptionalAccessSize
    :                               {$$ = TrCreateValuedLeafNode (PARSEOP_BYTECONST, 0);}
    | ','                           {$$ = TrCreateValuedLeafNode (PARSEOP_BYTECONST, 0);}
    | ',' ByteConstExpr             {$$ = $2;}
    ;

OptionalAddressingMode
    : ','                           {$$ = NULL;}
    | ',' AddressingModeKeyword     {$$ = $2;}
    ;

OptionalAddressRange
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' AddressKeyword            {$$ = $2;}
    ;

OptionalBitsPerByte
    : ','                           {$$ = NULL;}
    | ',' BitsPerByteKeyword        {$$ = $2;}
    ;

OptionalBuffer_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' RawDataBufferTerm         {$$ = $2;}
    ;

OptionalByteConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ByteConstExpr             {$$ = $2;}
    ;

OptionalDecodeType
    : ','                           {$$ = NULL;}
    | ',' DecodeKeyword             {$$ = $2;}
    ;

OptionalDevicePolarity
    : ','                           {$$ = NULL;}
    | ',' DevicePolarityKeyword     {$$ = $2;}
    ;

OptionalDWordConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' DWordConstExpr            {$$ = $2;}
    ;

OptionalEndian
    : ','                           {$$ = NULL;}
    | ',' EndianKeyword             {$$ = $2;}
    ;

OptionalFlowControl
    : ','                           {$$ = NULL;}
    | ',' FlowControlKeyword        {$$ = $2;}
    ;

OptionalIoRestriction
    : ','                           {$$ = NULL;}
    | ',' IoRestrictionKeyword      {$$ = $2;}
    ;

OptionalListString
    :                               {$$ = TrCreateValuedLeafNode (PARSEOP_STRING_LITERAL, ACPI_TO_INTEGER (""));}   /* Placeholder is a NULL string */
    | ','                           {$$ = TrCreateValuedLeafNode (PARSEOP_STRING_LITERAL, ACPI_TO_INTEGER (""));}   /* Placeholder is a NULL string */
    | ',' TermArg                   {$$ = $2;}
    ;

OptionalMaxType
    : ','                           {$$ = NULL;}
    | ',' MaxKeyword                {$$ = $2;}
    ;

OptionalMemType
    : ','                           {$$ = NULL;}
    | ',' MemTypeKeyword            {$$ = $2;}
    ;

OptionalMinType
    : ','                           {$$ = NULL;}
    | ',' MinKeyword                {$$ = $2;}
    ;

OptionalNameString
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' NameString                {$$ = $2;}
    ;

OptionalNameString_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' NameString                {$$ = $2;}
    ;

OptionalNameString_First
    :                               {$$ = TrCreateLeafNode (PARSEOP_ZERO);}
    | NameString                    {$$ = $1;}
    ;

OptionalObjectTypeKeyword
    :                               {$$ = TrCreateLeafNode (PARSEOP_OBJECTTYPE_UNK);}
    | ',' ObjectTypeKeyword         {$$ = $2;}
    ;

OptionalParityType
    : ','                           {$$ = NULL;}
    | ',' ParityTypeKeyword         {$$ = $2;}
    ;

OptionalQWordConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' QWordConstExpr            {$$ = $2;}
    ;

OptionalRangeType
    : ','                           {$$ = NULL;}
    | ',' RangeTypeKeyword          {$$ = $2;}
    ;

OptionalReadWriteKeyword
    :                                   {$$ = TrCreateLeafNode (PARSEOP_READWRITETYPE_BOTH);}
    | PARSEOP_READWRITETYPE_BOTH        {$$ = TrCreateLeafNode (PARSEOP_READWRITETYPE_BOTH);}
    | PARSEOP_READWRITETYPE_READONLY    {$$ = TrCreateLeafNode (PARSEOP_READWRITETYPE_READONLY);}
    ;

OptionalResourceType_First
    :                               {$$ = TrCreateLeafNode (PARSEOP_RESOURCETYPE_CONSUMER);}
    | ResourceTypeKeyword           {$$ = $1;}
    ;

OptionalResourceType
    :                               {$$ = TrCreateLeafNode (PARSEOP_RESOURCETYPE_CONSUMER);}
    | ','                           {$$ = TrCreateLeafNode (PARSEOP_RESOURCETYPE_CONSUMER);}
    | ',' ResourceTypeKeyword       {$$ = $2;}
    ;

OptionalSlaveMode
    : ','                           {$$ = NULL;}
    | ',' SlaveModeKeyword          {$$ = $2;}
    ;

OptionalShareType
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ShareTypeKeyword          {$$ = $2;}
    ;

OptionalShareType_First
    :                               {$$ = NULL;}
    | ShareTypeKeyword              {$$ = $1;}
    ;

OptionalStopBits
    : ','                           {$$ = NULL;}
    | ',' StopBitsKeyword           {$$ = $2;}
    ;

OptionalStringData
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' StringData                {$$ = $2;}
    ;

OptionalTranslationType_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TranslationKeyword        {$$ = $2;}
    ;

OptionalType
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TypeKeyword               {$$ = $2;}
    ;

OptionalType_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TypeKeyword               {$$ = $2;}
    ;

OptionalWireMode
    : ','                           {$$ = NULL;}
    | ',' WireModeKeyword           {$$ = $2;}
    ;

OptionalWordConstExpr
    : ','                           {$$ = NULL;}
    | ',' WordConstExpr             {$$ = $2;}
    ;

OptionalXferSize
    :                               {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_32, 2);}
    | ','                           {$$ = TrCreateValuedLeafNode (PARSEOP_XFERSIZE_32, 2);}
    | ',' XferSizeKeyword           {$$ = $2;}
    ;
