/** @file
  Flattened Device Tree Library.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <libfdt/libfdt/libfdt.h>
#include <Library/FdtLib.h>
#include <Uefi/UefiBaseType.h>

/**
  Convert UINT16 data of the FDT blob to little-endian

  @param[in] Value            The value to the blob data.

  @return The value to be converted to little-endian.

**/
UINT16
EFIAPI
Fdt16ToCpu (
  IN UINT16  Value
  )
{
  return fdt16_to_cpu (Value);
}

/**
  Convert UINT16 data to big-endian for aligned with the FDT blob

  @param[in] Value            The value to align with the FDT blob.

  @return The value to be converted to big-endian.

**/
UINT16
EFIAPI
CpuToFdt16 (
  IN UINT16  Value
  )
{
  return cpu_to_fdt16 (Value);
}

/**
  Convert UINT32 data of the FDT blob to little-endian

  @param[in] Value            The value to the blob data.

  @return The value to be converted to little-endian.

**/
UINT32
EFIAPI
Fdt32ToCpu (
  IN UINT32  Value
  )
{
  return fdt32_to_cpu (Value);
}

/**
  Convert UINT32 data to big-endian for aligned with the FDT blob

  @param[in] Value            The value to align with the FDT blob.

  @return The value to be converted to big-endian.

**/
UINT32
EFIAPI
CpuToFdt32 (
  IN UINT32  Value
  )
{
  return cpu_to_fdt32 (Value);
}

/**
  Convert UINT64 data of the FDT blob to little-endian

  @param[in] Value            The value to the blob data.

  @return The value to be converted to little-endian.

**/
UINT64
EFIAPI
Fdt64ToCpu (
  IN UINT64  Value
  )
{
  return fdt64_to_cpu (Value);
}

/**
  Convert UINT64 data to big-endian for aligned with the FDT blob

  @param[in] Value            The value to align with the FDT blob.

  @return The value to be converted to big-endian.

**/
UINT64
EFIAPI
CpuToFdt64 (
  IN UINT64  Value
  )
{
  return cpu_to_fdt64 (Value);
}

/**
  Verify the header of the Flattened Device Tree

  @param[in] Fdt            The pointer to FDT blob.

  @return Zero for successfully, otherwise failed.

**/
INT32
EFIAPI
FdtCheckHeader (
  IN CONST VOID  *Fdt
  )
{
  return fdt_check_header (Fdt);
}

/**
  Create a empty Flattened Device Tree.

  @param[in] Buffer         The pointer to allocate a pool for FDT blob.
  @param[in] BufferSize     The BufferSize to the pool size.

  @return Zero for successfully, otherwise failed.

**/
INT32
EFIAPI
FdtCreateEmptyTree (
  IN VOID    *Buffer,
  IN UINT32  BufferSize
  )
{
  return fdt_create_empty_tree (Buffer, (int)BufferSize);
}

/**
  Unpack FDT blob into new buffer

  @param[in]  Fdt            The pointer to FDT blob.
  @param[out] Buffer         Pointer to destination buffer.
  @param[in]  BufferSize     The size of destination buffer.

  @return Zero for successfully, otherwise failed.

 **/
INT32
EFIAPI
FdtOpenInto (
  IN  CONST VOID  *Fdt,
  OUT VOID        *Buffer,
  IN  INT32       BufferSize
  )
{
  return fdt_open_into (Fdt, Buffer, BufferSize);
}

/**
  Pack FDT blob in place.

  @param[in][out]  Fdt            The pointer to FDT blob.

  @return Zero.
**/
INT32
EFIAPI
FdtPack (
  IN OUT VOID  *Fdt
  )
{
  return fdt_pack (Fdt);
}

/**
  Returns a pointer to the node at a given offset.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] Offset         The offset to node.
  @param[in] Length         Maximum length of node.

  @return pointer to node.
**/
CONST VOID *
EFIAPI
FdtOffsetPointer (
  IN CONST VOID  *Fdt,
  IN INT32       Offset,
  IN UINT32      Length
  )
{
  return fdt_offset_ptr (Fdt, Offset, Length);
}

/**
  Returns a offset of next node from the given node.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] Offset         The offset to previous node.
  @param[in] Depth          The depth to the level of tree hierarchy.

  @return The offset to next node offset.

**/
INT32
EFIAPI
FdtNextNode (
  IN CONST VOID  *Fdt,
  IN INT32       Offset,
  IN INT32       *Depth
  )
{
  return fdt_next_node (Fdt, Offset, Depth);
}

/**
  Returns a offset of first node under the given node.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] Offset         The offset to previous node.

  @return The offset to next node offset.

**/
INT32
EFIAPI
FdtFirstSubnode (
  IN CONST VOID  *Fdt,
  IN INT32       Offset
  )
{
  return fdt_first_subnode (Fdt, Offset);
}

/**
  Returns a offset of next node from the given node.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] Offset         The offset to previous node.

  @return The offset to next node offset.

**/
INT32
EFIAPI
FdtNextSubnode (
  IN CONST VOID  *Fdt,
  IN INT32       Offset
  )
{
  return fdt_next_subnode (Fdt, Offset);
}

/**
  Returns the number of memory reserve map entries.

  @param[in] Fdt             The pointer to FDT blob.

  @return The number of entries in the reserve map.

**/
INTN
EFIAPI
FdtGetNumberOfReserveMapEntries (
  IN CONST VOID  *Fdt
  )
{
  return fdt_num_mem_rsv (Fdt);
}

/**
  Returns a memory reserve map entry.

  @param[in] *Fdt            The pointer to FDT blob.
  @param[in] Index           Index of reserve map entry.
  @param[out] Addr           Pointer to 64-bit variable to hold the start address
  @param[out] *Size          Pointer to 64-bit variable to hold size of reservation

  @return 0 on success, or negative error code.

**/
INTN
EFIAPI
FdtGetReserveMapEntry (
  IN CONST VOID             *Fdt,
  IN INTN                   Index,
  OUT EFI_PHYSICAL_ADDRESS  *Addr,
  OUT UINT64                *Size
  )
{
  return fdt_get_mem_rsv (Fdt, Index, Addr, Size);
}

/**
  Returns a offset of first node which includes the given name.

  @param[in] Fdt             The pointer to FDT blob.
  @param[in] ParentOffset    The offset to the node which start find under.
  @param[in] Name            The name to search the node with the name.
  @param[in] NameLength      The length of the name to check only.

  @return The offset to node offset with given node name.

**/
INT32
EFIAPI
FdtSubnodeOffsetNameLen (
  IN CONST VOID   *Fdt,
  IN INT32        ParentOffset,
  IN CONST CHAR8  *Name,
  IN INT32        NameLength
  )
{
  return fdt_subnode_offset_namelen (Fdt, ParentOffset, Name, NameLength);
}

/**
  Returns a offset of first node which matches the given name.

  @param[in] Fdt             The pointer to FDT blob.
  @param[in] ParentOffset    The offset to the node which start find under.
  @param[in] Name            The name to search the node with the name.

  @return The offset to node offset with given node name.

 **/
INT32
EFIAPI
FdtSubnodeOffset (
  IN CONST VOID   *Fdt,
  IN INT32        ParentOffset,
  IN CONST CHAR8  *Name
  )
{
  return fdt_subnode_offset (Fdt, ParentOffset, Name);
}

/**
  Find the parent of a given node.

  @param[in] Fdt             The pointer to FDT blob.
  @param[in] NodeOffset      The offset to the node to find the parent for.

  @return Structure block offset, or negative return value.
**/
INT32
EFIAPI
FdtParentOffset (
  IN CONST VOID  *Fdt,
  IN INT32       NodeOffset
  )
{
  return fdt_parent_offset (Fdt, NodeOffset);
}

/**
  Returns a offset of first node which includes the given property name and value.

  @param[in] Fdt             The pointer to FDT blob.
  @param[in] StartOffset     The offset to the starting node to find.
  @param[in] PropertyName    The property name to search the node including the named property.
  @param[in] PropertyValue   The property value (big-endian) to check the same property value.
  @param[in] PropertyLength  The length of the value in PropertValue.

  @return The offset to node offset with given property.

**/
INT32
EFIAPI
FdtNodeOffsetByPropertyValue (
  IN CONST VOID   *Fdt,
  IN INT32        StartOffset,
  IN CONST CHAR8  *PropertyName,
  IN CONST VOID   *PropertyValue,
  IN INT32        PropertyLength
  )
{
  return fdt_node_offset_by_prop_value (Fdt, StartOffset, PropertyName, PropertyValue, PropertyLength);
}

/**
  Returns a offset of first node which includes the given property name and value.

  @param[in] Fdt             The pointer to FDT blob.
  @param[in] Phandle         Phandle value to search for.

  @return The offset to node with matching Phandle value.
**/
INT32
EFIAPI
FdtNodeOffsetByPhandle (
  IN CONST VOID  *Fdt,
  IN UINT32      Phandle
  )
{
  return fdt_node_offset_by_phandle (Fdt, Phandle);
}

/**
  Look for a string in  a stringlist

  @param[in] StringList     Pointer to stringlist to search.
  @param[in] ListLength     Length of StringList.
  @param[in] String         Pointer to string to search for.

  @return 1 if found.
**/
INT32
EFIAPI
FdtStringListContains (
  IN CONST CHAR8  *StringList,
  IN INT32        ListLength,
  IN CONST CHAR8  *String
  )
{
  return fdt_stringlist_contains (StringList, ListLength, String);
}

/**
  Returns a property with the given name from the given node.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] NodeOffset     The offset to the given node.
  @param[in] Name           The name to the property which need be searched
  @param[in] Length         The length to the size of the property found.

  @return The property to the structure of the found property. Since the data
          come from FDT blob, it's encoding with big-endian.

**/
CONST FDT_PROPERTY *
EFIAPI
FdtGetProperty (
  IN CONST VOID   *Fdt,
  IN INT32        NodeOffset,
  IN CONST CHAR8  *Name,
  IN INT32        *Length
  )
{
  return (FDT_PROPERTY *)fdt_get_property (Fdt, NodeOffset, Name, Length);
}

/**
  Returns a pointer to a node mapped to an alias matching a substring.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] Name           The alias name string.
  @param[in] Length         The length to the size of the property found.

  @return A pointer to the expansion of the alias matching the substring,
          or NULL if alias not found.

**/
CONST CHAR8 *
EFIAPI
FdtGetAliasNameLen (
  IN CONST VOID   *Fdt,
  IN CONST CHAR8  *Name,
  IN INT32        Length
  )
{
  return fdt_get_alias_namelen (Fdt, Name, Length);
}

/**
  Returns a offset of first property in the given node.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] NodeOffset     The offset to the node which need be searched.

  @return The offset to first property offset in the given node.

**/
INT32
EFIAPI
FdtFirstPropertyOffset (
  IN CONST VOID  *Fdt,
  IN INT32       NodeOffset
  )
{
  return fdt_first_property_offset (Fdt, NodeOffset);
}

/**
  Returns a offset of next property from the given property.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] Offset         The offset to previous property.

  @return The offset to next property offset.

**/
INT32
EFIAPI
FdtNextPropertyOffset (
  IN CONST VOID  *Fdt,
  IN INT32       Offset
  )
{
  return fdt_next_property_offset (Fdt, Offset);
}

/**
  Returns a property from the given offset of the property.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] Offset         The offset to the given offset of the property.
  @param[in] Length         The length to the size of the property found.

  @return The property to the structure of the given property offset.

**/
CONST FDT_PROPERTY *
EFIAPI
FdtGetPropertyByOffset (
  IN CONST VOID  *Fdt,
  IN INT32       Offset,
  IN INT32       *Length
  )
{
  return (FDT_PROPERTY *)fdt_get_property_by_offset (Fdt, Offset, Length);
}

/**
  Returns a string by the given string offset.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] StrOffset      The offset to the location in the strings block of FDT.
  @param[in] Length         The length to the size of string which need be retrieved.

  @return The string to the given string offset.

**/
CONST CHAR8 *
EFIAPI
FdtGetString (
  IN CONST VOID  *Fdt,
  IN INT32       StrOffset,
  IN INT32       *Length        OPTIONAL
  )
{
  return fdt_get_string (Fdt, StrOffset, Length);
}

/**
  Add a new node to the FDT.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] ParentOffset   The offset to the node offset which want to add in.
  @param[in] Name           The name to name the node.

  @return  The offset to the new node.

**/
INT32
EFIAPI
FdtAddSubnode (
  IN VOID         *Fdt,
  IN INT32        ParentOffset,
  IN CONST CHAR8  *Name
  )
{
  return fdt_add_subnode (Fdt, ParentOffset, Name);
}

/**
  Add or modify a property in the given node.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] NodeOffset     The offset to the node offset which want to add in.
  @param[in] Name           The name to name the property.
  @param[in] Value          The value (big-endian) to the property value.
  @param[in] Length         The length to the size of the property.

  @return  Zero for successfully, otherwise failed.

**/
INT32
EFIAPI
FdtSetProperty (
  IN VOID         *Fdt,
  IN INT32        NodeOffset,
  IN CONST CHAR8  *Name,
  IN CONST VOID   *Value,
  IN UINT32       Length
  )
{
  return fdt_setprop (Fdt, NodeOffset, Name, Value, (int)Length);
}

/**
  Set a property to a 64-bit integer.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] NodeOffset     The offset to the node offset which want to add in.
  @param[in] Name           The name to name the property.
  @param[in] Value          The value (big-endian) to the property value.

  @return  Zero for successfully, otherwise failed.

 **/
INT32
EFIAPI
FdtSetPropU64 (
  IN VOID         *Fdt,
  IN INT32        NodeOffset,
  IN CONST CHAR8  *Name,
  IN UINT64       Value
  )
{
  UINT64  Tmp;

  Tmp = cpu_to_fdt64 (Value);

  return fdt_setprop (Fdt, NodeOffset, Name, &Tmp, sizeof (Tmp));
}

/**
  Append or create a property in the given node.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] NodeOffset     The offset to the node offset which want to add in.
  @param[in] Name           The name to name the property.
  @param[in] Value          The value (big-endian) to the property value.
  @param[in] Length         The length to the size of the property.

  @return  Zero for successfully, otherwise failed.

 **/
INT32
EFIAPI
FdtAppendProp (
  IN VOID         *Fdt,
  IN INT32        NodeOffset,
  IN CONST CHAR8  *Name,
  IN CONST VOID   *Value,
  IN UINT32       Length
  )
{
  return fdt_appendprop (Fdt, NodeOffset, Name, Value, (int)Length);
}

/**
  Delete a property.

  This function will delete data from the blob, and will therefore
  change the offsets of some existing nodes.

  @param[in][out] Fdt         Pointer to the device tree blob.
  @param[in]      NodeOffset  Offset of the node whose property to nop.
  @param[in]      Name        Name of the property to nop.

  @return  Zero for successfully, otherwise failed.

**/
INT32
FdtDelProp (
  IN OUT VOID         *Fdt,
  IN     INT32        NodeOffset,
  IN     CONST CHAR8  *Name
  )
{
  return fdt_delprop (Fdt, NodeOffset, Name);
}

/**
  Finds a tree node by substring

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] Path           Full path of the node to locate.
  @param[in] NameLength      The length of the name to check only.

  @return structure block offset of the node with the requested path (>=0), on success
**/
INT32
EFIAPI
FdtPathOffsetNameLen (
  IN CONST VOID   *Fdt,
  IN CONST CHAR8  *Path,
  IN INT32        NameLength
  )
{
  return fdt_path_offset_namelen (Fdt, Path, NameLength);
}

/**
  Finds a tree node by its full path.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] Path           Full path of the node to locate.

  @return structure block offset of the node with the requested path (>=0), on success
**/
INT32
EFIAPI
FdtPathOffset (
  IN CONST VOID   *Fdt,
  IN CONST CHAR8  *Path
  )
{
  return fdt_path_offset (Fdt, Path);
}

/**
  Returns the name of a given node.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] NodeOffset     Offset of node to check.
  @param[in] Length         The pointer to an integer variable (will be overwritten) or NULL.

  @return The pointer to the node's name.

**/
CONST CHAR8 *
EFIAPI
FdtGetName (
  IN VOID   *Fdt,
  IN INT32  NodeOffset,
  IN INT32  *Length
  )
{
  return fdt_get_name (Fdt, NodeOffset, Length);
}

/**
  FdtNodeDepth() finds the depth of a given node.  The root node
  has depth 0, its immediate subnodes depth 1 and so forth.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] NodeOffset     Offset of node to check.

  @returns Depth of the node at NodeOffset.
**/
INT32
EFIAPI
FdtNodeDepth (
  IN CONST VOID  *Fdt,
  IN INT32       NodeOffset
  )
{
  return fdt_node_depth (Fdt, NodeOffset);
}

/**
  Find nodes with a given 'compatible' value.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] StartOffset    Only find nodes after this offset.
  @param[in] Compatible     The string to match against.

  @retval The offset of the first node after StartOffset.
**/
INT32
EFIAPI
FdtNodeOffsetByCompatible (
  IN CONST VOID   *Fdt,
  IN INT32        StartOffset,
  IN CONST CHAR8  *Compatible
  )
{
  return fdt_node_offset_by_compatible (Fdt, StartOffset, Compatible);
}

/**
   Retrieve address size for a bus represented in the tree

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] NodeOffset     Offset of node to check.

  @return Number of cells in the bus address, or negative error.
**/
INT32
EFIAPI
FdtAddressCells (
  IN CONST VOID  *Fdt,
  IN INT32       NodeOffset
  )
{
  return fdt_address_cells (Fdt, NodeOffset);
}

/**
   Retrieve address range size for a bus represented in the tree

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] NodeOffset     Offset of node to check.

  @return Number of cells in the bus size, or negative error.
**/
INT32
EFIAPI
FdtSizeCells (
  IN CONST VOID  *Fdt,
  IN INT32       NodeOffset
  )
{
  return fdt_size_cells (Fdt, NodeOffset);
}

/* Debug functions. */
CONST
CHAR8
*
FdtStrerror (
  IN INT32  ErrVal
  )
{
  return fdt_strerror (ErrVal);
}
