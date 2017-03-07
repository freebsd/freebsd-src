/** @file
  Declaration of internal functions in BaseLib.

  Copyright (c) 2006 - 2017, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __BASE_LIB_INTERNALS__
#define __BASE_LIB_INTERNALS__

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

//
// Math functions
//

/**
  Shifts a 64-bit integer left between 0 and 63 bits. The low bits
  are filled with zeros. The shifted value is returned.

  This function shifts the 64-bit value Operand to the left by Count bits. The
  low Count bits are set to zero. The shifted value is returned.

  @param  Operand The 64-bit operand to shift left.
  @param  Count   The number of bits to shift left.

  @return Operand << Count

**/
UINT64
EFIAPI
InternalMathLShiftU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  );

/**
  Shifts a 64-bit integer right between 0 and 63 bits. The high bits
  are filled with zeros. The shifted value is returned.

  This function shifts the 64-bit value Operand to the right by Count bits. The
  high Count bits are set to zero. The shifted value is returned.

  @param  Operand The 64-bit operand to shift right.
  @param  Count   The number of bits to shift right.

  @return Operand >> Count

**/
UINT64
EFIAPI
InternalMathRShiftU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  );

/**
  Shifts a 64-bit integer right between 0 and 63 bits. The high bits
  are filled with original integer's bit 63. The shifted value is returned.

  This function shifts the 64-bit value Operand to the right by Count bits. The
  high Count bits are set to bit 63 of Operand.  The shifted value is returned.

  @param  Operand The 64-bit operand to shift right.
  @param  Count   The number of bits to shift right.

  @return Operand arithmetically shifted right by Count

**/
UINT64
EFIAPI
InternalMathARShiftU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  );

/**
  Rotates a 64-bit integer left between 0 and 63 bits, filling
  the low bits with the high bits that were rotated.

  This function rotates the 64-bit value Operand to the left by Count bits. The
  low Count bits are filled with the high Count bits of Operand. The rotated
  value is returned.

  @param  Operand The 64-bit operand to rotate left.
  @param  Count   The number of bits to rotate left.

  @return Operand <<< Count

**/
UINT64
EFIAPI
InternalMathLRotU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  );

/**
  Rotates a 64-bit integer right between 0 and 63 bits, filling
  the high bits with the high low bits that were rotated.

  This function rotates the 64-bit value Operand to the right by Count bits.
  The high Count bits are filled with the low Count bits of Operand. The rotated
  value is returned.

  @param  Operand The 64-bit operand to rotate right.
  @param  Count   The number of bits to rotate right.

  @return Operand >>> Count

**/
UINT64
EFIAPI
InternalMathRRotU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  );

/**
  Switches the endianess of a 64-bit integer.

  This function swaps the bytes in a 64-bit unsigned value to switch the value
  from little endian to big endian or vice versa. The byte swapped value is
  returned.

  @param  Operand A 64-bit unsigned value.

  @return The byte swapped Operand.

**/
UINT64
EFIAPI
InternalMathSwapBytes64 (
  IN      UINT64                    Operand
  );

/**
  Multiplies a 64-bit unsigned integer by a 32-bit unsigned integer
  and generates a 64-bit unsigned result.

  This function multiplies the 64-bit unsigned value Multiplicand by the 32-bit
  unsigned value Multiplier and generates a 64-bit unsigned result. This 64-
  bit unsigned result is returned.

  @param  Multiplicand  A 64-bit unsigned value.
  @param  Multiplier    A 32-bit unsigned value.

  @return Multiplicand * Multiplier

**/
UINT64
EFIAPI
InternalMathMultU64x32 (
  IN      UINT64                    Multiplicand,
  IN      UINT32                    Multiplier
  );

/**
  Multiplies a 64-bit unsigned integer by a 64-bit unsigned integer
  and generates a 64-bit unsigned result.

  This function multiples the 64-bit unsigned value Multiplicand by the 64-bit
  unsigned value Multiplier and generates a 64-bit unsigned result. This 64-
  bit unsigned result is returned.

  @param  Multiplicand  A 64-bit unsigned value.
  @param  Multiplier    A 64-bit unsigned value.

  @return Multiplicand * Multiplier

**/
UINT64
EFIAPI
InternalMathMultU64x64 (
  IN      UINT64                    Multiplicand,
  IN      UINT64                    Multiplier
  );

/**
  Divides a 64-bit unsigned integer by a 32-bit unsigned integer and
  generates a 64-bit unsigned result.

  This function divides the 64-bit unsigned value Dividend by the 32-bit
  unsigned value Divisor and generates a 64-bit unsigned quotient. This
  function returns the 64-bit unsigned quotient.

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 32-bit unsigned value.

  @return Dividend / Divisor

**/
UINT64
EFIAPI
InternalMathDivU64x32 (
  IN      UINT64                    Dividend,
  IN      UINT32                    Divisor
  );

/**
  Divides a 64-bit unsigned integer by a 32-bit unsigned integer and
  generates a 32-bit unsigned remainder.

  This function divides the 64-bit unsigned value Dividend by the 32-bit
  unsigned value Divisor and generates a 32-bit remainder. This function
  returns the 32-bit unsigned remainder.

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 32-bit unsigned value.

  @return Dividend % Divisor

**/
UINT32
EFIAPI
InternalMathModU64x32 (
  IN      UINT64                    Dividend,
  IN      UINT32                    Divisor
  );

/**
  Divides a 64-bit unsigned integer by a 32-bit unsigned integer and
  generates a 64-bit unsigned result and an optional 32-bit unsigned remainder.

  This function divides the 64-bit unsigned value Dividend by the 32-bit
  unsigned value Divisor and generates a 64-bit unsigned quotient. If Remainder
  is not NULL, then the 32-bit unsigned remainder is returned in Remainder.
  This function returns the 64-bit unsigned quotient.

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 32-bit unsigned value.
  @param  Remainder A pointer to a 32-bit unsigned value. This parameter is
                    optional and may be NULL.

  @return Dividend / Divisor

**/
UINT64
EFIAPI
InternalMathDivRemU64x32 (
  IN      UINT64                    Dividend,
  IN      UINT32                    Divisor,
  OUT     UINT32                    *Remainder OPTIONAL
  );

/**
  Divides a 64-bit unsigned integer by a 64-bit unsigned integer and
  generates a 64-bit unsigned result and an optional 64-bit unsigned remainder.

  This function divides the 64-bit unsigned value Dividend by the 64-bit
  unsigned value Divisor and generates a 64-bit unsigned quotient. If Remainder
  is not NULL, then the 64-bit unsigned remainder is returned in Remainder.
  This function returns the 64-bit unsigned quotient.

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 64-bit unsigned value.
  @param  Remainder A pointer to a 64-bit unsigned value. This parameter is
                    optional and may be NULL.

  @return Dividend / Divisor

**/
UINT64
EFIAPI
InternalMathDivRemU64x64 (
  IN      UINT64                    Dividend,
  IN      UINT64                    Divisor,
  OUT     UINT64                    *Remainder OPTIONAL
  );

/**
  Divides a 64-bit signed integer by a 64-bit signed integer and
  generates a 64-bit signed result and an optional 64-bit signed remainder.

  This function divides the 64-bit signed value Dividend by the 64-bit
  signed value Divisor and generates a 64-bit signed quotient. If Remainder
  is not NULL, then the 64-bit signed remainder is returned in Remainder.
  This function returns the 64-bit signed quotient.

  @param  Dividend  A 64-bit signed value.
  @param  Divisor   A 64-bit signed value.
  @param  Remainder A pointer to a 64-bit signed value. This parameter is
                    optional and may be NULL.

  @return Dividend / Divisor

**/
INT64
EFIAPI
InternalMathDivRemS64x64 (
  IN      INT64                     Dividend,
  IN      INT64                     Divisor,
  OUT     INT64                     *Remainder  OPTIONAL
  );

/**
  Transfers control to a function starting with a new stack.

  Transfers control to the function specified by EntryPoint using the
  new stack specified by NewStack and passing in the parameters specified
  by Context1 and Context2.  Context1 and Context2 are optional and may
  be NULL.  The function EntryPoint must never return.
  Marker will be ignored on IA-32, x64, and EBC.
  IPF CPUs expect one additional parameter of type VOID * that specifies
  the new backing store pointer.

  If EntryPoint is NULL, then ASSERT().
  If NewStack is NULL, then ASSERT().

  @param  EntryPoint  A pointer to function to call with the new stack.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function.
  @param  Marker      VA_LIST marker for the variable argument list.

**/
VOID
EFIAPI
InternalSwitchStack (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1,   OPTIONAL
  IN      VOID                      *Context2,   OPTIONAL
  IN      VOID                      *NewStack,
  IN      VA_LIST                   Marker
  );


/**
  Worker function that locates the Node in the List.

  By searching the List, finds the location of the Node in List. At the same time,
  verifies the validity of this list.

  If List is NULL, then ASSERT().
  If List->ForwardLink is NULL, then ASSERT().
  If List->backLink is NULL, then ASSERT().
  If Node is NULL, then ASSERT();
  If PcdMaximumLinkedListLength is not zero, and prior to insertion the number
  of nodes in ListHead, including the ListHead node, is greater than or
  equal to PcdMaximumLinkedListLength, then ASSERT().

  @param  List  A pointer to a node in a linked list.
  @param  Node  A pointer to one nod.

  @retval TRUE   Node is in List.
  @retval FALSE  Node isn't in List, or List is invalid.

**/
BOOLEAN
EFIAPI
IsNodeInList (
  IN      CONST LIST_ENTRY      *List,
  IN      CONST LIST_ENTRY      *Node
  );

/**
  Worker function that returns a bit field from Operand.

  Returns the bitfield specified by the StartBit and the EndBit from Operand.

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
  @param  EndBit    The ordinal of the most significant bit in the bit field.

  @return The bit field read.

**/
UINTN
EFIAPI
BitFieldReadUint (
  IN      UINTN                     Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit
  );


/**
  Worker function that reads a bit field from Operand, performs a bitwise OR,
  and returns the result.

  Performs a bitwise OR between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new value is returned.

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
  @param  OrData    The value to OR with the read value from the value

  @return The new value.

**/
UINTN
EFIAPI
BitFieldOrUint (
  IN      UINTN                     Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINTN                     OrData
  );


/**
  Worker function that reads a bit field from Operand, performs a bitwise AND,
  and returns the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new value is returned.

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
  @param  AndData    The value to And with the read value from the value

  @return The new value.

**/
UINTN
EFIAPI
BitFieldAndUint (
  IN      UINTN                     Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINTN                     AndData
  );


/**
  Worker function that checks ASSERT condition for JumpBuffer

  Checks ASSERT condition for JumpBuffer.

  If JumpBuffer is NULL, then ASSERT().
  For IPF CPUs, if JumpBuffer is not aligned on a 16-byte boundary, then ASSERT().

  @param  JumpBuffer    A pointer to CPU context buffer.

**/
VOID
EFIAPI
InternalAssertJumpBuffer (
  IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
  );


/**
  Restores the CPU context that was saved with SetJump().

  Restores the CPU context from the buffer specified by JumpBuffer.
  This function never returns to the caller.
  Instead is resumes execution based on the state of JumpBuffer.

  @param  JumpBuffer    A pointer to CPU context buffer.
  @param  Value         The value to return when the SetJump() context is restored.

**/
VOID
EFIAPI
InternalLongJump (
  IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer,
  IN      UINTN                     Value
  );


/**
  Check if a Unicode character is a decimal character.

  This internal function checks if a Unicode character is a
  decimal character. The valid decimal character is from
  L'0' to L'9'.

  @param  Char  The character to check against.

  @retval TRUE  If the Char is a decmial character.
  @retval FALSE If the Char is not a decmial character.

**/
BOOLEAN
EFIAPI
InternalIsDecimalDigitCharacter (
  IN      CHAR16                    Char
  );


/**
  Convert a Unicode character to upper case only if
  it maps to a valid small-case ASCII character.

  This internal function only deal with Unicode character
  which maps to a valid small-case ASCII character, i.e.
  L'a' to L'z'. For other Unicode character, the input character
  is returned directly.

  @param  Char  The character to convert.

  @retval LowerCharacter   If the Char is with range L'a' to L'z'.
  @retval Unchanged        Otherwise.

**/
CHAR16
EFIAPI
InternalCharToUpper (
  IN      CHAR16                    Char
  );


/**
  Convert a Unicode character to numerical value.

  This internal function only deal with Unicode character
  which maps to a valid hexadecimal ASII character, i.e.
  L'0' to L'9', L'a' to L'f' or L'A' to L'F'. For other
  Unicode character, the value returned does not make sense.

  @param  Char  The character to convert.

  @return The numerical value converted.

**/
UINTN
EFIAPI
InternalHexCharToUintn (
  IN      CHAR16                    Char
  );


/**
  Check if a Unicode character is a hexadecimal character.

  This internal function checks if a Unicode character is a
  decimal character.  The valid hexadecimal character is
  L'0' to L'9', L'a' to L'f', or L'A' to L'F'.


  @param  Char  The character to check against.

  @retval TRUE  If the Char is a hexadecmial character.
  @retval FALSE If the Char is not a hexadecmial character.

**/
BOOLEAN
EFIAPI
InternalIsHexaDecimalDigitCharacter (
  IN      CHAR16                    Char
  );


/**
  Check if a ASCII character is a decimal character.

  This internal function checks if a Unicode character is a
  decimal character. The valid decimal character is from
  '0' to '9'.

  @param  Char  The character to check against.

  @retval TRUE  If the Char is a decmial character.
  @retval FALSE If the Char is not a decmial character.

**/
BOOLEAN
EFIAPI
InternalAsciiIsDecimalDigitCharacter (
  IN      CHAR8                     Char
  );


/**
  Converts a lowercase Ascii character to upper one.

  If Chr is lowercase Ascii character, then converts it to upper one.

  If Value >= 0xA0, then ASSERT().
  If (Value & 0x0F) >= 0x0A, then ASSERT().

  @param  Chr   one Ascii character

  @return The uppercase value of Ascii character

**/
CHAR8
EFIAPI
InternalBaseLibAsciiToUpper (
  IN      CHAR8                     Chr
  );


/**
  Check if a ASCII character is a hexadecimal character.

  This internal function checks if a ASCII character is a
  decimal character.  The valid hexadecimal character is
  L'0' to L'9', L'a' to L'f', or L'A' to L'F'.


  @param  Char  The character to check against.

  @retval TRUE  If the Char is a hexadecmial character.
  @retval FALSE If the Char is not a hexadecmial character.

**/
BOOLEAN
EFIAPI
InternalAsciiIsHexaDecimalDigitCharacter (
  IN      CHAR8                    Char
  );


/**
  Convert a ASCII character to numerical value.

  This internal function only deal with Unicode character
  which maps to a valid hexadecimal ASII character, i.e.
  '0' to '9', 'a' to 'f' or 'A' to 'F'. For other
  ASCII character, the value returned does not make sense.

  @param  Char  The character to convert.

  @return The numerical value converted.

**/
UINTN
EFIAPI
InternalAsciiHexCharToUintn (
  IN      CHAR8                    Char
  );


//
// Ia32 and x64 specific functions
//
#if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64)

/**
  Reads the current Global Descriptor Table Register(GDTR) descriptor.

  Reads and returns the current GDTR descriptor and returns it in Gdtr. This
  function is only available on IA-32 and x64.

  @param  Gdtr  The pointer to a GDTR descriptor.

**/
VOID
EFIAPI
InternalX86ReadGdtr (
  OUT     IA32_DESCRIPTOR           *Gdtr
  );

/**
  Writes the current Global Descriptor Table Register (GDTR) descriptor.

  Writes and the current GDTR descriptor specified by Gdtr. This function is
  only available on IA-32 and x64.

  @param  Gdtr  The pointer to a GDTR descriptor.

**/
VOID
EFIAPI
InternalX86WriteGdtr (
  IN      CONST IA32_DESCRIPTOR     *Gdtr
  );

/**
  Reads the current Interrupt Descriptor Table Register(GDTR) descriptor.

  Reads and returns the current IDTR descriptor and returns it in Idtr. This
  function is only available on IA-32 and x64.

  @param  Idtr  The pointer to an IDTR descriptor.

**/
VOID
EFIAPI
InternalX86ReadIdtr (
  OUT     IA32_DESCRIPTOR           *Idtr
  );

/**
  Writes the current Interrupt Descriptor Table Register(GDTR) descriptor.

  Writes the current IDTR descriptor and returns it in Idtr. This function is
  only available on IA-32 and x64.

  @param  Idtr  The pointer to an IDTR descriptor.

**/
VOID
EFIAPI
InternalX86WriteIdtr (
  IN      CONST IA32_DESCRIPTOR     *Idtr
  );

/**
  Save the current floating point/SSE/SSE2 context to a buffer.

  Saves the current floating point/SSE/SSE2 state to the buffer specified by
  Buffer. Buffer must be aligned on a 16-byte boundary. This function is only
  available on IA-32 and x64.

  @param  Buffer  The pointer to a buffer to save the floating point/SSE/SSE2 context.

**/
VOID
EFIAPI
InternalX86FxSave (
  OUT     IA32_FX_BUFFER            *Buffer
  );

/**
  Restores the current floating point/SSE/SSE2 context from a buffer.

  Restores the current floating point/SSE/SSE2 state from the buffer specified
  by Buffer. Buffer must be aligned on a 16-byte boundary. This function is
  only available on IA-32 and x64.

  @param  Buffer  The pointer to a buffer to save the floating point/SSE/SSE2 context.

**/
VOID
EFIAPI
InternalX86FxRestore (
  IN      CONST IA32_FX_BUFFER      *Buffer
  );

/**
  Enables the 32-bit paging mode on the CPU.

  Enables the 32-bit paging mode on the CPU. CR0, CR3, CR4, and the page tables
  must be properly initialized prior to calling this service. This function
  assumes the current execution mode is 32-bit protected mode. This function is
  only available on IA-32. After the 32-bit paging mode is enabled, control is
  transferred to the function specified by EntryPoint using the new stack
  specified by NewStack and passing in the parameters specified by Context1 and
  Context2. Context1 and Context2 are optional and may be NULL. The function
  EntryPoint must never return.

  There are a number of constraints that must be followed before calling this
  function:
  1)  Interrupts must be disabled.
  2)  The caller must be in 32-bit protected mode with flat descriptors. This
      means all descriptors must have a base of 0 and a limit of 4GB.
  3)  CR0 and CR4 must be compatible with 32-bit protected mode with flat
      descriptors.
  4)  CR3 must point to valid page tables that will be used once the transition
      is complete, and those page tables must guarantee that the pages for this
      function and the stack are identity mapped.

  @param  EntryPoint  A pointer to function to call with the new stack after
                      paging is enabled.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function as the first parameter after paging is enabled.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function as the second parameter after paging is enabled.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function after paging is enabled.

**/
VOID
EFIAPI
InternalX86EnablePaging32 (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1,  OPTIONAL
  IN      VOID                      *Context2,  OPTIONAL
  IN      VOID                      *NewStack
  );

/**
  Disables the 32-bit paging mode on the CPU.

  Disables the 32-bit paging mode on the CPU and returns to 32-bit protected
  mode. This function assumes the current execution mode is 32-paged protected
  mode. This function is only available on IA-32. After the 32-bit paging mode
  is disabled, control is transferred to the function specified by EntryPoint
  using the new stack specified by NewStack and passing in the parameters
  specified by Context1 and Context2. Context1 and Context2 are optional and
  may be NULL. The function EntryPoint must never return.

  There are a number of constraints that must be followed before calling this
  function:
  1)  Interrupts must be disabled.
  2)  The caller must be in 32-bit paged mode.
  3)  CR0, CR3, and CR4 must be compatible with 32-bit paged mode.
  4)  CR3 must point to valid page tables that guarantee that the pages for
      this function and the stack are identity mapped.

  @param  EntryPoint  A pointer to function to call with the new stack after
                      paging is disabled.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function as the first parameter after paging is disabled.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function as the second parameter after paging is
                      disabled.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function after paging is disabled.

**/
VOID
EFIAPI
InternalX86DisablePaging32 (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1,  OPTIONAL
  IN      VOID                      *Context2,  OPTIONAL
  IN      VOID                      *NewStack
  );

/**
  Enables the 64-bit paging mode on the CPU.

  Enables the 64-bit paging mode on the CPU. CR0, CR3, CR4, and the page tables
  must be properly initialized prior to calling this service. This function
  assumes the current execution mode is 32-bit protected mode with flat
  descriptors. This function is only available on IA-32. After the 64-bit
  paging mode is enabled, control is transferred to the function specified by
  EntryPoint using the new stack specified by NewStack and passing in the
  parameters specified by Context1 and Context2. Context1 and Context2 are
  optional and may be 0. The function EntryPoint must never return.

  @param  Cs          The 16-bit selector to load in the CS before EntryPoint
                      is called. The descriptor in the GDT that this selector
                      references must be setup for long mode.
  @param  EntryPoint  The 64-bit virtual address of the function to call with
                      the new stack after paging is enabled.
  @param  Context1    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the first parameter after
                      paging is enabled.
  @param  Context2    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the second parameter after
                      paging is enabled.
  @param  NewStack    The 64-bit virtual address of the new stack to use for
                      the EntryPoint function after paging is enabled.

**/
VOID
EFIAPI
InternalX86EnablePaging64 (
  IN      UINT16                    Cs,
  IN      UINT64                    EntryPoint,
  IN      UINT64                    Context1,  OPTIONAL
  IN      UINT64                    Context2,  OPTIONAL
  IN      UINT64                    NewStack
  );

/**
  Disables the 64-bit paging mode on the CPU.

  Disables the 64-bit paging mode on the CPU and returns to 32-bit protected
  mode. This function assumes the current execution mode is 64-paging mode.
  This function is only available on x64. After the 64-bit paging mode is
  disabled, control is transferred to the function specified by EntryPoint
  using the new stack specified by NewStack and passing in the parameters
  specified by Context1 and Context2. Context1 and Context2 are optional and
  may be 0. The function EntryPoint must never return.

  @param  Cs          The 16-bit selector to load in the CS before EntryPoint
                      is called. The descriptor in the GDT that this selector
                      references must be setup for 32-bit protected mode.
  @param  EntryPoint  The 64-bit virtual address of the function to call with
                      the new stack after paging is disabled.
  @param  Context1    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the first parameter after
                      paging is disabled.
  @param  Context2    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the second parameter after
                      paging is disabled.
  @param  NewStack    The 64-bit virtual address of the new stack to use for
                      the EntryPoint function after paging is disabled.

**/
VOID
EFIAPI
InternalX86DisablePaging64 (
  IN      UINT16                    Cs,
  IN      UINT32                    EntryPoint,
  IN      UINT32                    Context1,  OPTIONAL
  IN      UINT32                    Context2,  OPTIONAL
  IN      UINT32                    NewStack
  );

/**
  Generates a 16-bit random number through RDRAND instruction.

  @param[out]  Rand     Buffer pointer to store the random result.

  @retval TRUE          RDRAND call was successful.
  @retval FALSE         Failed attempts to call RDRAND.

 **/
BOOLEAN
EFIAPI
InternalX86RdRand16 (
  OUT     UINT16                    *Rand
  );

/**
  Generates a 32-bit random number through RDRAND instruction.

  @param[out]  Rand     Buffer pointer to store the random result.

  @retval TRUE          RDRAND call was successful.
  @retval FALSE         Failed attempts to call RDRAND.

**/
BOOLEAN
EFIAPI
InternalX86RdRand32 (
  OUT     UINT32                    *Rand
  );

/**
  Generates a 64-bit random number through RDRAND instruction.


  @param[out]  Rand     Buffer pointer to store the random result.

  @retval TRUE          RDRAND call was successful.
  @retval FALSE         Failed attempts to call RDRAND.

**/
BOOLEAN
EFIAPI
InternalX86RdRand64  (
  OUT     UINT64                    *Rand
  );


#elif defined (MDE_CPU_IPF)
//
//
// IPF specific functions
//

/**
  Reads control register DCR.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_DCR.

  @return The 64-bit control register DCR.

**/
UINT64
EFIAPI
AsmReadControlRegisterDcr (
  VOID
  );


/**
  Reads control register ITM.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_ITM.

  @return The 64-bit control register ITM.

**/
UINT64
EFIAPI
AsmReadControlRegisterItm (
  VOID
  );


/**
  Reads control register IVA.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IVA.

  @return The 64-bit control register IVA.

**/
UINT64
EFIAPI
AsmReadControlRegisterIva (
  VOID
  );


/**
  Reads control register PTA.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_PTA.

  @return The 64-bit control register PTA.

**/
UINT64
EFIAPI
AsmReadControlRegisterPta (
  VOID
  );


/**
  Reads control register IPSR.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IPSR.

  @return The 64-bit control register IPSR.

**/
UINT64
EFIAPI
AsmReadControlRegisterIpsr (
  VOID
  );


/**
  Reads control register ISR.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_ISR.

  @return The 64-bit control register ISR.

**/
UINT64
EFIAPI
AsmReadControlRegisterIsr (
  VOID
  );


/**
  Reads control register IIP.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IIP.

  @return The 64-bit control register IIP.

**/
UINT64
EFIAPI
AsmReadControlRegisterIip (
  VOID
  );


/**
  Reads control register IFA.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IFA.

  @return The 64-bit control register IFA.

**/
UINT64
EFIAPI
AsmReadControlRegisterIfa (
  VOID
  );


/**
  Reads control register ITIR.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_ITIR.

  @return The 64-bit control register ITIR.

**/
UINT64
EFIAPI
AsmReadControlRegisterItir (
  VOID
  );


/**
  Reads control register IIPA.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IIPA.

  @return The 64-bit control register IIPA.

**/
UINT64
EFIAPI
AsmReadControlRegisterIipa (
  VOID
  );


/**
  Reads control register IFS.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IFS.

  @return The 64-bit control register IFS.

**/
UINT64
EFIAPI
AsmReadControlRegisterIfs (
  VOID
  );


/**
  Reads control register IIM.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IIM.

  @return The 64-bit control register IIM.

**/
UINT64
EFIAPI
AsmReadControlRegisterIim (
  VOID
  );


/**
  Reads control register IHA.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IHA.

  @return The 64-bit control register IHA.

**/
UINT64
EFIAPI
AsmReadControlRegisterIha (
  VOID
  );


/**
  Reads control register LID.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_LID.

  @return The 64-bit control register LID.

**/
UINT64
EFIAPI
AsmReadControlRegisterLid (
  VOID
  );


/**
  Reads control register IVR.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IVR.

  @return The 64-bit control register IVR.

**/
UINT64
EFIAPI
AsmReadControlRegisterIvr (
  VOID
  );


/**
  Reads control register TPR.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_TPR.

  @return The 64-bit control register TPR.

**/
UINT64
EFIAPI
AsmReadControlRegisterTpr (
  VOID
  );


/**
  Reads control register EOI.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_EOI.

  @return The 64-bit control register EOI.

**/
UINT64
EFIAPI
AsmReadControlRegisterEoi (
  VOID
  );


/**
  Reads control register IRR0.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IRR0.

  @return The 64-bit control register IRR0.

**/
UINT64
EFIAPI
AsmReadControlRegisterIrr0 (
  VOID
  );


/**
  Reads control register IRR1.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IRR1.

  @return The 64-bit control register IRR1.

**/
UINT64
EFIAPI
AsmReadControlRegisterIrr1 (
  VOID
  );


/**
  Reads control register IRR2.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IRR2.

  @return The 64-bit control register IRR2.

**/
UINT64
EFIAPI
AsmReadControlRegisterIrr2 (
  VOID
  );


/**
  Reads control register IRR3.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_IRR3.

  @return The 64-bit control register IRR3.

**/
UINT64
EFIAPI
AsmReadControlRegisterIrr3 (
  VOID
  );


/**
  Reads control register ITV.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_ITV.

  @return The 64-bit control register ITV.

**/
UINT64
EFIAPI
AsmReadControlRegisterItv (
  VOID
  );


/**
  Reads control register PMV.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_PMV.

  @return The 64-bit control register PMV.

**/
UINT64
EFIAPI
AsmReadControlRegisterPmv (
  VOID
  );


/**
  Reads control register CMCV.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_CMCV.

  @return The 64-bit control register CMCV.

**/
UINT64
EFIAPI
AsmReadControlRegisterCmcv (
  VOID
  );


/**
  Reads control register LRR0.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_LRR0.

  @return The 64-bit control register LRR0.

**/
UINT64
EFIAPI
AsmReadControlRegisterLrr0 (
  VOID
  );


/**
  Reads control register LRR1.

  This is a worker function for AsmReadControlRegister()
  when its parameter Index is IPF_CONTROL_REGISTER_LRR1.

  @return The 64-bit control register LRR1.

**/
UINT64
EFIAPI
AsmReadControlRegisterLrr1 (
  VOID
  );


/**
  Reads application register K0.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_K0.

  @return The 64-bit application register K0.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterK0 (
  VOID
  );



/**
  Reads application register K1.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_K1.

  @return The 64-bit application register K1.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterK1 (
  VOID
  );


/**
  Reads application register K2.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_K2.

  @return The 64-bit application register K2.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterK2 (
  VOID
  );


/**
  Reads application register K3.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_K3.

  @return The 64-bit application register K3.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterK3 (
  VOID
  );


/**
  Reads application register K4.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_K4.

  @return The 64-bit application register K4.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterK4 (
  VOID
  );


/**
  Reads application register K5.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_K5.

  @return The 64-bit application register K5.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterK5 (
  VOID
  );


/**
  Reads application register K6.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_K6.

  @return The 64-bit application register K6.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterK6 (
  VOID
  );


/**
  Reads application register K7.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_K7.

  @return The 64-bit application register K7.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterK7 (
  VOID
  );


/**
  Reads application register RSC.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_RSC.

  @return The 64-bit application register RSC.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterRsc (
  VOID
  );


/**
  Reads application register BSP.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_BSP.

  @return The 64-bit application register BSP.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterBsp (
  VOID
  );


/**
  Reads application register BSPSTORE.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_BSPSTORE.

  @return The 64-bit application register BSPSTORE.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterBspstore (
  VOID
  );


/**
  Reads application register RNAT.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_RNAT.

  @return The 64-bit application register RNAT.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterRnat (
  VOID
  );


/**
  Reads application register FCR.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_FCR.

  @return The 64-bit application register FCR.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterFcr (
  VOID
  );


/**
  Reads application register EFLAG.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_EFLAG.

  @return The 64-bit application register EFLAG.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterEflag (
  VOID
  );


/**
  Reads application register CSD.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_CSD.

  @return The 64-bit application register CSD.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterCsd (
  VOID
  );


/**
  Reads application register SSD.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_SSD.

  @return The 64-bit application register SSD.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterSsd (
  VOID
  );


/**
  Reads application register CFLG.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_CFLG.

  @return The 64-bit application register CFLG.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterCflg (
  VOID
  );


/**
  Reads application register FSR.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_FSR.

  @return The 64-bit application register FSR.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterFsr (
  VOID
  );


/**
  Reads application register FIR.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_FIR.

  @return The 64-bit application register FIR.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterFir (
  VOID
  );


/**
  Reads application register FDR.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_FDR.

  @return The 64-bit application register FDR.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterFdr (
  VOID
  );


/**
  Reads application register CCV.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_CCV.

  @return The 64-bit application register CCV.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterCcv (
  VOID
  );


/**
  Reads application register UNAT.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_UNAT.

  @return The 64-bit application register UNAT.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterUnat (
  VOID
  );


/**
  Reads application register FPSR.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_FPSR.

  @return The 64-bit application register FPSR.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterFpsr (
  VOID
  );


/**
  Reads application register ITC.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_ITC.

  @return The 64-bit application register ITC.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterItc (
  VOID
  );


/**
  Reads application register PFS.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_PFS.

  @return The 64-bit application register PFS.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterPfs (
  VOID
  );


/**
  Reads application register LC.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_LC.

  @return The 64-bit application register LC.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterLc (
  VOID
  );


/**
  Reads application register EC.

  This is a worker function for AsmReadApplicationRegister()
  when its parameter Index is IPF_APPLICATION_REGISTER_EC.

  @return The 64-bit application register EC.

**/
UINT64
EFIAPI
AsmReadApplicationRegisterEc (
  VOID
  );



/**
  Transfers control to a function starting with a new stack.

  Transfers control to the function specified by EntryPoint using the new stack
  specified by NewStack and passing in the parameters specified by Context1 and
  Context2. Context1 and Context2 are optional and may be NULL. The function
  EntryPoint must never return.

  If EntryPoint is NULL, then ASSERT().
  If NewStack is NULL, then ASSERT().

  @param  EntryPoint  A pointer to function to call with the new stack.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function.
  @param  NewBsp      A pointer to the new memory location for RSE backing
                      store.

**/
VOID
EFIAPI
AsmSwitchStackAndBackingStore (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1,  OPTIONAL
  IN      VOID                      *Context2,  OPTIONAL
  IN      VOID                      *NewStack,
  IN      VOID                      *NewBsp
  );

/**
  Internal worker function to invalidate a range of instruction cache lines
  in the cache coherency domain of the calling CPU.

  Internal worker function to invalidate the instruction cache lines specified
  by Address and Length. If Address is not aligned on a cache line boundary,
  then entire instruction cache line containing Address is invalidated. If
  Address + Length is not aligned on a cache line boundary, then the entire
  instruction cache line containing Address + Length -1 is invalidated. This
  function may choose to invalidate the entire instruction cache if that is more
  efficient than invalidating the specified range. If Length is 0, the no instruction
  cache lines are invalidated. Address is returned.
  This function is only available on IPF.

  @param  Address The base address of the instruction cache lines to
                  invalidate. If the CPU is in a physical addressing mode, then
                  Address is a physical address. If the CPU is in a virtual
                  addressing mode, then Address is a virtual address.

  @param  Length  The number of bytes to invalidate from the instruction cache.

  @return Address

**/
VOID *
EFIAPI
InternalFlushCacheRange (
  IN      VOID                      *Address,
  IN      UINTN                     Length
  );

#else

#endif

#endif
