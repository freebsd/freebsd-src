/** @file
  Flattened Device Tree Library.

  All structure data are in big-endian format.
  Functions are provided for converting data between
  little-endian and big-endian.
  For example:
  Pushing data to FDT blob needs to convert data to
  big-endian by CpuToFdt*().
  Retrieving data from FDT blob needs to convert data to
  little-endian by Fdt*ToCpu().
  Refer to FDT specification: https://www.devicetree.org/specifications/

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef FDT_LIB_H_
#define FDT_LIB_H_

/* Error codes: informative error codes */
#define FDT_ERR_NOTFOUND  1
/* FDT_ERR_NOTFOUND: The requested node or property does not exist */
#define FDT_ERR_EXISTS  2

/* FDT_ERR_EXISTS: Attempted to create a node or property which
 * already exists */
#define FDT_ERR_NOSPACE  3

/* FDT_ERR_NOSPACE: Operation needed to expand the device
 * tree, but its buffer did not have sufficient space to
 * contain the expanded tree. Use fdt_open_into() to move the
 * device tree to a buffer with more space. */

/* Error codes: codes for bad parameters */
#define FDT_ERR_BADOFFSET  4

/* FDT_ERR_BADOFFSET: Function was passed a structure block
 * offset which is out-of-bounds, or which points to an
 * unsuitable part of the structure for the operation. */
#define FDT_ERR_BADPATH  5

/* FDT_ERR_BADPATH: Function was passed a badly formatted path
 * (e.g. missing a leading / for a function which requires an
 * absolute path) */
#define FDT_ERR_BADPHANDLE  6

/* FDT_ERR_BADPHANDLE: Function was passed an invalid phandle.
 * This can be caused either by an invalid phandle property
 * length, or the phandle value was either 0 or -1, which are
 * not permitted. */
#define FDT_ERR_BADSTATE  7

/* FDT_ERR_BADSTATE: Function was passed an incomplete device
 * tree created by the sequential-write functions, which is
 * not sufficiently complete for the requested operation. */

/* Error codes: codes for bad device tree blobs */
#define FDT_ERR_TRUNCATED  8

/* FDT_ERR_TRUNCATED: FDT or a sub-block is improperly
 * terminated (overflows, goes outside allowed bounds, or
 * isn't properly terminated).  */
#define FDT_ERR_BADMAGIC  9

/* FDT_ERR_BADMAGIC: Given "device tree" appears not to be a
 * device tree at all - it is missing the flattened device
 * tree magic number. */
#define FDT_ERR_BADVERSION  10

/* FDT_ERR_BADVERSION: Given device tree has a version which
 * can't be handled by the requested operation.  For
 * read-write functions, this may mean that fdt_open_into() is
 * required to convert the tree to the expected version. */
#define FDT_ERR_BADSTRUCTURE  11

/* FDT_ERR_BADSTRUCTURE: Given device tree has a corrupt
 * structure block or other serious error (e.g. misnested
 * nodes, or subnodes preceding properties). */
#define FDT_ERR_BADLAYOUT  12

/* FDT_ERR_BADLAYOUT: For read-write functions, the given
 * device tree has it's sub-blocks in an order that the
 * function can't handle (memory reserve map, then structure,
 * then strings).  Use fdt_open_into() to reorganize the tree
 * into a form suitable for the read-write operations. */

/* "Can't happen" error indicating a bug in libfdt */
#define FDT_ERR_INTERNAL  13

/* FDT_ERR_INTERNAL: libfdt has failed an internal assertion.
 * Should never be returned, if it is, it indicates a bug in
 * libfdt itself. */

/* Errors in device tree content */
#define FDT_ERR_BADNCELLS  14

/* FDT_ERR_BADNCELLS: Device tree has a #address-cells, #size-cells
 * or similar property with a bad format or value */

#define FDT_ERR_BADVALUE  15

/* FDT_ERR_BADVALUE: Device tree has a property with an unexpected
 * value. For example: a property expected to contain a string list
 * is not NUL-terminated within the length of its value. */

#define FDT_ERR_BADOVERLAY  16

/* FDT_ERR_BADOVERLAY: The device tree overlay, while
 * correctly structured, cannot be applied due to some
 * unexpected or missing value, property or node. */

#define FDT_ERR_NOPHANDLES  17

/* FDT_ERR_NOPHANDLES: The device tree doesn't have any
 * phandle available anymore without causing an overflow */

#define FDT_ERR_BADFLAGS  18

/* FDT_ERR_BADFLAGS: The function was passed a flags field that
 * contains invalid flags or an invalid combination of flags. */

#define FDT_ERR_ALIGNMENT  19

/* FDT_ERR_ALIGNMENT: The device tree base address is not 8-byte
 * aligned. */

#define FDT_ERR_MAX  19

/**
  Flattened Device Tree definition

  The Devicetree Blob (DTB) format is a binary encoding with big-endian.
  When producing or consuming the blob data, CpuToFdt*() or Fdt*ToCpu()
  provided by this library may be called to convert data between
  big-endian and little-endian.
**/
typedef struct {
  UINT32    Magic;               /* magic word FDT_MAGIC */
  UINT32    TotalSize;           /* total size of DT block */
  UINT32    OffsetDtStruct;      /* offset to structure */
  UINT32    OffsetDtStrings;     /* offset to strings */
  UINT32    OffsetMemRsvmap;     /* offset to memory reserve map */
  UINT32    Version;             /* format version */
  UINT32    LastCompVersion;     /* last compatible version */

  /* version 2 fields below */
  UINT32    BootCpuidPhys;       /* Which physical CPU id we're
                                    booting on */
  /* version 3 fields below */
  UINT32    SizeDtStrings;       /* size of the strings block */

  /* version 17 fields below */
  UINT32    SizeDtStruct;        /* size of the structure block */
} FDT_HEADER;

typedef struct {
  UINT64    Address;
  UINT64    Size;
} FDT_RESERVE_ENTRY;

typedef struct {
  UINT32    Tag;
  CHAR8     Name[];
} FDT_NODE_HEADER;

typedef struct {
  UINT32    Tag;
  UINT32    Length;
  UINT32    NameOffset;
  CHAR8     Data[];
} FDT_PROPERTY;

#ifndef FDT_TAGSIZE
#define FDT_TAGSIZE  sizeof(UINT32)
#endif
#ifndef FDT_MAX_NCELLS
#define FDT_MAX_NCELLS  4
#endif

#define FdtGetHeader(Fdt, Field) \
  (Fdt32ToCpu (((const FDT_HEADER *)(Fdt))->Field))
#define FdtTotalSize(Fdt)  (FdtGetHeader ((Fdt), TotalSize))

#define FdtForEachSubnode(Node, Fdt, Parent) \
  for (Node = FdtFirstSubnode (Fdt, Parent); \
       Node >= 0;                            \
       Node = FdtNextSubnode (Fdt, Node))

/**
  Convert UINT16 data of the FDT blob to little-endian

  @param[in] Value            The value to the blob data.

  @return The value to be converted to little-endian.

**/
UINT16
EFIAPI
Fdt16ToCpu (
  IN UINT16  Value
  );

/**
  Convert UINT16 data to big-endian for aligned with the FDT blob

  @param[in] Value            The value to align with the FDT blob.

  @return The value to be converted to big-endian.

**/
UINT16
EFIAPI
CpuToFdt16 (
  IN UINT16  Value
  );

/**
  Convert UINT32 data of the FDT blob to little-endian

  @param[in] Value            The value to the blob data.

  @return The value to be converted to little-endian.

**/
UINT32
EFIAPI
Fdt32ToCpu (
  IN UINT32  Value
  );

/**
  Convert UINT32 data to big-endian for aligned with the FDT blob

  @param[in] Value            The value to align with the FDT blob.

  @return The value to be converted to big-endian.

**/
UINT32
EFIAPI
CpuToFdt32 (
  IN UINT32  Value
  );

/**
  Convert UINT64 data of the FDT blob to little-endian

  @param[in] Value            The value to the blob data.

  @return The value to be converted to little-endian.

**/
UINT64
EFIAPI
Fdt64ToCpu (
  IN UINT64  Value
  );

/**
  Convert UINT64 data to big-endian for aligned with the FDT blob

  @param[in] Value            The value to align with the FDT blob.

  @return The value to be converted to big-endian.

**/
UINT64
EFIAPI
CpuToFdt64 (
  IN UINT64  Value
  );

/**
  Verify the header of the Flattened Device Tree

  @param[in] Fdt            The pointer to FDT blob.

  @return Zero for successfully, otherwise failed.

**/
INT32
EFIAPI
FdtCheckHeader (
  IN CONST VOID  *Fdt
  );

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
  );

/**
  Pack FDT blob in place.

  @param[in][out]  Fdt            The pointer to FDT blob.

  @return Zero.
**/
INT32
EFIAPI
FdtPack (
  IN OUT VOID  *Fdt
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

/**
  Returns the number of memory reserve map entries.

  @param[in] Fdt             The pointer to FDT blob.

  @return The number of entries in the reserve map.

**/
INTN
EFIAPI
FdtGetNumberOfReserveMapEntries (
  IN CONST VOID  *Fdt
  );

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
  IN CONST VOID  *Fdt,
  IN INTN        Index,
  OUT UINT64     *Addr,
  OUT UINT64     *Size
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

/**
  Returns the name of a given node.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] NodeOffse      Offset of node to check.
  @param[in] Length         The pointer to an integer variable (will be overwritten) or NULL.

  @return The pointer to the node's name.

**/
CONST CHAR8 *
EFIAPI
FdtGetName (
  IN VOID   *Fdt,
  IN INT32  NodeOffset,
  IN INT32  *Length
  );

/**
  FdtNodeDepth() finds the depth of a given node.  The root node
  has depth 0, its immediate subnodes depth 1 and so forth.

  @param[in] Fdt            The pointer to FDT blob.
  @param[in] NodeOffset     Offset of node to check.

  @return Depth of the node at NodeOffset.
**/
INT32
EFIAPI
FdtNodeDepth (
  IN CONST VOID  *Fdt,
  IN INT32       NodeOffset
  );

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
  );

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
  );

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
  );

/* Debug functions. */
CONST
CHAR8
*
FdtStrerror (
  IN INT32  ErrVal
  );

#endif /* FDT_LIB_H_ */
