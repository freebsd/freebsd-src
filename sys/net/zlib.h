/*	$Id: zlib.h,v 1.6 1997/05/22 06:44:40 paulus Exp $	*/

/*
 * This file is derived from zlib.h and zconf.h from the zlib-0.95
 * distribution by Jean-loup Gailly and Mark Adler, with some additions
 * by Paul Mackerras to aid in implementing Deflate compression and
 * decompression for PPP packets.
 */

/*
 *  ==FILEVERSION 970501==
 *
 * This marker is used by the Linux installation script to determine
 * whether an up-to-date version of this file is already installed.
 */

/* zlib.h -- interface of the 'zlib' general purpose compression library
  version 0.95, Aug 16th, 1995.

  Copyright (C) 1995 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  gzip@prep.ai.mit.edu    madler@alumni.caltech.edu
 */

#ifndef _ZLIB_H
#define _ZLIB_H

/* #include "zconf.h" */	/* included directly here */

/* zconf.h -- configuration of the zlib compression library
 * Copyright (C) 1995 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

/* From: zconf.h,v 1.12 1995/05/03 17:27:12 jloup Exp */

/*
     The library does not install any signal handler. It is recommended to
  add at least a handler for SIGSEGV when decompressing; the library checks
  the consistency of the input data whenever possible but may go nuts
  for some forms of corrupted input.
 */

/*
 * Compile with -DMAXSEG_64K if the alloc function cannot allocate more
 * than 64k bytes at a time (needed on systems with 16-bit int).
 * Compile with -DUNALIGNED_OK if it is OK to access shorts or ints
 * at addresses which are not a multiple of their size.
 * Under DOS, -DFAR=far or -DFAR=__far may be needed.
 */

#ifndef STDC
#  if defined(MSDOS) || defined(__STDC__) || defined(__cplusplus)
#    define STDC
#  endif
#endif

#ifdef	__MWERKS__ /* Metrowerks CodeWarrior declares fileno() in unix.h */
#  include <unix.h>
#endif

/* Maximum value for memLevel in deflateInit2 */
#ifndef MAX_MEM_LEVEL
#  ifdef MAXSEG_64K
#    define MAX_MEM_LEVEL 8
#  else
#    define MAX_MEM_LEVEL 9
#  endif
#endif

#ifndef FAR
#  define FAR
#endif

/* Maximum value for windowBits in deflateInit2 and inflateInit2 */
#ifndef MAX_WBITS
#  define MAX_WBITS   15 /* 32K LZ77 window */
#endif

/* The memory requirements for deflate are (in bytes):
            1 << (windowBits+2)   +  1 << (memLevel+9)
 that is: 128K for windowBits=15  +  128K for memLevel = 8  (default values)
 plus a few kilobytes for small objects. For example, if you want to reduce
 the default memory requirements from 256K to 128K, compile with
     make CFLAGS="-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7"
 Of course this will generally degrade compression (there's no free lunch).

   The memory requirements for inflate are (in bytes) 1 << windowBits
 that is, 32K for windowBits=15 (default value) plus a few kilobytes
 for small objects.
*/

                        /* Type declarations */

#ifndef OF /* function prototypes */
#  ifdef STDC
#    define OF(args)  args
#  else
#    define OF(args)  ()
#  endif
#endif

typedef unsigned char  Byte;  /* 8 bits */
typedef unsigned int   uInt;  /* 16 bits or more */
typedef unsigned long  uLong; /* 32 bits or more */

typedef Byte FAR Bytef;
typedef char FAR charf;
typedef int FAR intf;
typedef uInt FAR uIntf;
typedef uLong FAR uLongf;

#ifdef STDC
   typedef void FAR *voidpf;
   typedef void     *voidp;
#else
   typedef Byte FAR *voidpf;
   typedef Byte     *voidp;
#endif

/* end of original zconf.h */

#define ZLIB_VERSION "0.95P"

/* 
     The 'zlib' compression library provides in-memory compression and
  decompression functions, including integrity checks of the uncompressed
  data.  This version of the library supports only one compression method
  (deflation) but other algorithms may be added later and will have the same
  stream interface.

     For compression the application must provide the output buffer and
  may optionally provide the input buffer for optimization. For decompression,
  the application must provide the input buffer and may optionally provide
  the output buffer for optimization.

     Compression can be done in a single step if the buffers are large
  enough (for example if an input file is mmap'ed), or can be done by
  repeated calls of the compression function.  In the latter case, the
  application must provide more input and/or consume the output
  (providing more output space) before each call.
*/

typedef voidpf (*alloc_func) OF((voidpf opaque, uInt items, uInt size));
typedef void   (*free_func)  OF((voidpf opaque, voidpf address, uInt nbytes));

struct internal_state;

typedef struct z_stream_s {
    Bytef    *next_in;  /* next input byte */
    uInt     avail_in;  /* number of bytes available at next_in */
    uLong    total_in;  /* total nb of input bytes read so far */

    Bytef    *next_out; /* next output byte should be put there */
    uInt     avail_out; /* remaining free space at next_out */
    uLong    total_out; /* total nb of bytes output so far */

    char     *msg;      /* last error message, NULL if no error */
    struct internal_state FAR *state; /* not visible by applications */

    alloc_func zalloc;  /* used to allocate the internal state */
    alloc_func zalloc_init;  /* used to allocate the internal state during initialization */
    free_func  zfree;   /* used to free the internal state */
    voidp      opaque;  /* private data object passed to zalloc and zfree */

    Byte     data_type; /* best guess about the data type: ascii or binary */

} z_stream;

/*
   The application must update next_in and avail_in when avail_in has
   dropped to zero. It must update next_out and avail_out when avail_out
   has dropped to zero. The application must initialize zalloc, zfree and
   opaque before calling the init function. All other fields are set by the
   compression library and must not be updated by the application.

   The opaque value provided by the application will be passed as the first
   parameter for calls of zalloc and zfree. This can be useful for custom
   memory management. The compression library attaches no meaning to the
   opaque value.

   zalloc must return Z_NULL if there is not enough memory for the object.
   On 16-bit systems, the functions zalloc and zfree must be able to allocate
   exactly 65536 bytes, but will not be required to allocate more than this
   if the symbol MAXSEG_64K is defined (see zconf.h). WARNING: On MSDOS,
   pointers returned by zalloc for objects of exactly 65536 bytes *must*
   have their offset normalized to zero. The default allocation function
   provided by this library ensures this (see zutil.c). To reduce memory
   requirements and avoid any allocation of 64K objects, at the expense of
   compression ratio, compile the library with -DMAX_WBITS=14 (see zconf.h).

   The fields total_in and total_out can be used for statistics or
   progress reports. After compression, total_in holds the total size of
   the uncompressed data and may be saved for use in the decompressor
   (particularly if the decompressor wants to decompress everything in
   a single step).
*/

                        /* constants */

#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1
#define Z_FULL_FLUSH    2
#define Z_SYNC_FLUSH    3 /* experimental: partial_flush + byte align */
#define Z_FINISH        4
#define Z_PACKET_FLUSH	5
/* See deflate() below for the usage of these constants */

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
/* error codes for the compression/decompression functions */

#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)
/* compression levels */

#define Z_FILTERED            1
#define Z_HUFFMAN_ONLY        2
#define Z_DEFAULT_STRATEGY    0

#define Z_BINARY   0
#define Z_ASCII    1
#define Z_UNKNOWN  2
/* Used to set the data_type field */

#define Z_NULL  0  /* for initializing zalloc, zfree, opaque */

extern char *zlib_version;
/* The application can compare zlib_version and ZLIB_VERSION for consistency.
   If the first character differs, the library code actually used is
   not compatible with the zlib.h header file used by the application.
 */

                        /* basic functions */

extern int deflateInit OF((z_stream *strm, int level));
/* 
     Initializes the internal stream state for compression. The fields
   zalloc, zfree and opaque must be initialized before by the caller.
   If zalloc and zfree are set to Z_NULL, deflateInit updates them to
   use default allocation functions.

     The compression level must be Z_DEFAULT_COMPRESSION, or between 1 and 9:
   1 gives best speed, 9 gives best compression. Z_DEFAULT_COMPRESSION requests
   a default compromise between speed and compression (currently equivalent
   to level 6).

     deflateInit returns Z_OK if success, Z_MEM_ERROR if there was not
   enough memory, Z_STREAM_ERROR if level is not a valid compression level.
   msg is set to null if there is no error message.  deflateInit does not
   perform any compression: this will be done by deflate().
*/


extern int deflate OF((z_stream *strm, int flush));
/*
  Performs one or both of the following actions:

  - Compress more input starting at next_in and update next_in and avail_in
    accordingly. If not all input can be processed (because there is not
    enough room in the output buffer), next_in and avail_in are updated and
    processing will resume at this point for the next call of deflate().

  - Provide more output starting at next_out and update next_out and avail_out
    accordingly. This action is forced if the parameter flush is non zero.
    Forcing flush frequently degrades the compression ratio, so this parameter
    should be set only when necessary (in interactive applications).
    Some output may be provided even if flush is not set.

  Before the call of deflate(), the application should ensure that at least
  one of the actions is possible, by providing more input and/or consuming
  more output, and updating avail_in or avail_out accordingly; avail_out
  should never be zero before the call. The application can consume the
  compressed output when it wants, for example when the output buffer is full
  (avail_out == 0), or after each call of deflate().

    If the parameter flush is set to Z_PARTIAL_FLUSH, the current compression
  block is terminated and flushed to the output buffer so that the
  decompressor can get all input data available so far. For method 9, a future
  variant on method 8, the current block will be flushed but not terminated.
  If flush is set to Z_FULL_FLUSH, the compression block is terminated, a
  special marker is output and the compression dictionary is discarded; this
  is useful to allow the decompressor to synchronize if one compressed block
  has been damaged (see inflateSync below).  Flushing degrades compression and
  so should be used only when necessary.  Using Z_FULL_FLUSH too often can
  seriously degrade the compression. If deflate returns with avail_out == 0,
  this function must be called again with the same value of the flush
  parameter and more output space (updated avail_out), until the flush is
  complete (deflate returns with non-zero avail_out).

    If the parameter flush is set to Z_PACKET_FLUSH, the compression
  block is terminated, and a zero-length stored block is output,
  omitting the length bytes (the effect of this is that the 3-bit type
  code 000 for a stored block is output, and the output is then
  byte-aligned).  This is designed for use at the end of a PPP packet.
  In addition, if the current compression block contains all the data
  since the last Z_PACKET_FLUSH, it is never output as a stored block.
  If the current compression block output as a static or dynamic block
  would not be at least `minCompression' bytes smaller than the
  original data, then nothing is output for that block.  (The type
  code for the zero-length stored block is still output, resulting in
  a single zero byte being output for the whole packet.)
  `MinCompression' is a parameter to deflateInit2, or 0 if deflateInit
  is used.

    If the parameter flush is set to Z_FINISH, all pending input is processed,
  all pending output is flushed and deflate returns with Z_STREAM_END if there
  was enough output space; if deflate returns with Z_OK, this function must be
  called again with Z_FINISH and more output space (updated avail_out) but no
  more input data, until it returns with Z_STREAM_END or an error. After
  deflate has returned Z_STREAM_END, the only possible operations on the
  stream are deflateReset or deflateEnd.
  
    Z_FINISH can be used immediately after deflateInit if all the compression
  is to be done in a single step. In this case, avail_out must be at least
  0.1% larger than avail_in plus 12 bytes.  If deflate does not return
  Z_STREAM_END, then it must be called again as described above.

    deflate() may update data_type if it can make a good guess about
  the input data type (Z_ASCII or Z_BINARY). In doubt, the data is considered
  binary. This field is only for information purposes and does not affect
  the compression algorithm in any manner.

    deflate() returns Z_OK if some progress has been made (more input
  processed or more output produced), Z_STREAM_END if all input has been
  consumed and all output has been produced (only when flush is set to
  Z_FINISH), Z_STREAM_ERROR if the stream state was inconsistent (for example
  if next_in or next_out was NULL), Z_BUF_ERROR if no progress is possible.
*/


extern int deflateEnd OF((z_stream *strm));
/*
     All dynamically allocated data structures for this stream are freed.
   This function discards any unprocessed input and does not flush any
   pending output.

     deflateEnd returns Z_OK if success, Z_STREAM_ERROR if the
   stream state was inconsistent. In the error case, msg may be set
   but then points to a static string (which must not be deallocated).
*/


extern int inflateInit OF((z_stream *strm));
/* 
     Initializes the internal stream state for decompression. The fields
   zalloc and zfree must be initialized before by the caller.  If zalloc and
   zfree are set to Z_NULL, inflateInit updates them to use default allocation
   functions.

     inflateInit returns Z_OK if success, Z_MEM_ERROR if there was not
   enough memory.  msg is set to null if there is no error message.
   inflateInit does not perform any decompression: this will be done by
   inflate().
*/


#if defined(__FreeBSD__) && (defined(KERNEL) || defined(_KERNEL))
#define inflate	inflate_ppp	/* FreeBSD already has an inflate :-( */
#endif

extern int inflate OF((z_stream *strm, int flush));
/*
  Performs one or both of the following actions:

  - Decompress more input starting at next_in and update next_in and avail_in
    accordingly. If not all input can be processed (because there is not
    enough room in the output buffer), next_in is updated and processing
    will resume at this point for the next call of inflate().

  - Provide more output starting at next_out and update next_out and avail_out
    accordingly.  inflate() always provides as much output as possible
    (until there is no more input data or no more space in the output buffer).

  Before the call of inflate(), the application should ensure that at least
  one of the actions is possible, by providing more input and/or consuming
  more output, and updating the next_* and avail_* values accordingly.
  The application can consume the uncompressed output when it wants, for
  example when the output buffer is full (avail_out == 0), or after each
  call of inflate().

    If the parameter flush is set to Z_PARTIAL_FLUSH or Z_PACKET_FLUSH,
  inflate flushes as much output as possible to the output buffer. The
  flushing behavior of inflate is not specified for values of the flush
  parameter other than Z_PARTIAL_FLUSH, Z_PACKET_FLUSH or Z_FINISH, but the
  current implementation actually flushes as much output as possible
  anyway.  For Z_PACKET_FLUSH, inflate checks that once all the input data
  has been consumed, it is expecting to see the length field of a stored
  block; if not, it returns Z_DATA_ERROR.

    inflate() should normally be called until it returns Z_STREAM_END or an
  error. However if all decompression is to be performed in a single step
  (a single call of inflate), the parameter flush should be set to
  Z_FINISH. In this case all pending input is processed and all pending
  output is flushed; avail_out must be large enough to hold all the
  uncompressed data. (The size of the uncompressed data may have been saved
  by the compressor for this purpose.) The next operation on this stream must
  be inflateEnd to deallocate the decompression state. The use of Z_FINISH
  is never required, but can be used to inform inflate that a faster routine
  may be used for the single inflate() call.

    inflate() returns Z_OK if some progress has been made (more input
  processed or more output produced), Z_STREAM_END if the end of the
  compressed data has been reached and all uncompressed output has been
  produced, Z_DATA_ERROR if the input data was corrupted, Z_STREAM_ERROR if
  the stream structure was inconsistent (for example if next_in or next_out
  was NULL), Z_MEM_ERROR if there was not enough memory, Z_BUF_ERROR if no
  progress is possible or if there was not enough room in the output buffer
  when Z_FINISH is used. In the Z_DATA_ERROR case, the application may then
  call inflateSync to look for a good compression block.  */


extern int inflateEnd OF((z_stream *strm));
/*
     All dynamically allocated data structures for this stream are freed.
   This function discards any unprocessed input and does not flush any
   pending output.

     inflateEnd returns Z_OK if success, Z_STREAM_ERROR if the stream state
   was inconsistent. In the error case, msg may be set but then points to a
   static string (which must not be deallocated).
*/

                        /* advanced functions */

/*
    The following functions are needed only in some special applications.
*/

extern int deflateInit2 OF((z_stream *strm,
                            int  level,
                            int  method,
                            int  windowBits,
                            int  memLevel,
                            int  strategy,
			    int  minCompression));
/*   
     This is another version of deflateInit with more compression options. The
   fields next_in, zalloc and zfree must be initialized before by the caller.

     The method parameter is the compression method. It must be 8 in this
   version of the library. (Method 9 will allow a 64K history buffer and
   partial block flushes.)

     The windowBits parameter is the base two logarithm of the window size
   (the size of the history buffer).  It should be in the range 8..15 for this
   version of the library (the value 16 will be allowed for method 9). Larger
   values of this parameter result in better compression at the expense of
   memory usage. The default value is 15 if deflateInit is used instead.

    The memLevel parameter specifies how much memory should be allocated
   for the internal compression state. memLevel=1 uses minimum memory but
   is slow and reduces compression ratio; memLevel=9 uses maximum memory
   for optimal speed. The default value is 8. See zconf.h for total memory
   usage as a function of windowBits and memLevel.

     The strategy parameter is used to tune the compression algorithm. Use
   the value Z_DEFAULT_STRATEGY for normal data, Z_FILTERED for data
   produced by a filter (or predictor), or Z_HUFFMAN_ONLY to force Huffman
   encoding only (no string match).  Filtered data consists mostly of small
   values with a somewhat random distribution. In this case, the
   compression algorithm is tuned to compress them better. The strategy
   parameter only affects the compression ratio but not the correctness of
   the compressed output even if it is not set appropriately.

     The minCompression parameter specifies the minimum reduction in size
   required for a compressed block to be output when Z_PACKET_FLUSH is
   used (see the description of deflate above).

     If next_in is not null, the library will use this buffer to hold also
   some history information; the buffer must either hold the entire input
   data, or have at least 1<<(windowBits+1) bytes and be writable. If next_in
   is null, the library will allocate its own history buffer (and leave next_in
   null). next_out need not be provided here but must be provided by the
   application for the next call of deflate().

     If the history buffer is provided by the application, next_in must
   must never be changed by the application since the compressor maintains
   information inside this buffer from call to call; the application
   must provide more input only by increasing avail_in. next_in is always
   reset by the library in this case.

      deflateInit2 returns Z_OK if success, Z_MEM_ERROR if there was
   not enough memory, Z_STREAM_ERROR if a parameter is invalid (such as
   an invalid method). msg is set to null if there is no error message.
   deflateInit2 does not perform any compression: this will be done by
   deflate().
*/
                            
extern int deflateCopy OF((z_stream *dest,
                           z_stream *source));
/*
     Sets the destination stream as a complete copy of the source stream.  If
   the source stream is using an application-supplied history buffer, a new
   buffer is allocated for the destination stream.  The compressed output
   buffer is always application-supplied. It's the responsibility of the
   application to provide the correct values of next_out and avail_out for the
   next call of deflate.

     This function is useful when several compression strategies will be
   tried, for example when there are several ways of pre-processing the input
   data with a filter. The streams that will be discarded should then be freed
   by calling deflateEnd.  Note that deflateCopy duplicates the internal
   compression state which can be quite large, so this strategy is slow and
   can consume lots of memory.

      deflateCopy returns Z_OK if success, Z_MEM_ERROR if there was not
   enough memory, Z_STREAM_ERROR if the source stream state was inconsistent
   (such as zalloc being NULL). msg is left unchanged in both source and
   destination.
*/

extern int deflateReset OF((z_stream *strm));
/*
     This function is equivalent to deflateEnd followed by deflateInit,
   but does not free and reallocate all the internal compression state.
   The stream will keep the same compression level and any other attributes
   that may have been set by deflateInit2.

      deflateReset returns Z_OK if success, or Z_STREAM_ERROR if the source
   stream state was inconsistent (such as zalloc or state being NULL).
*/

extern int inflateInit2 OF((z_stream *strm,
                            int  windowBits));
/*   
     This is another version of inflateInit with more compression options. The
   fields next_out, zalloc and zfree must be initialized before by the caller.

     The windowBits parameter is the base two logarithm of the maximum window
   size (the size of the history buffer).  It should be in the range 8..15 for
   this version of the library (the value 16 will be allowed soon). The
   default value is 15 if inflateInit is used instead. If a compressed stream
   with a larger window size is given as input, inflate() will return with
   the error code Z_DATA_ERROR instead of trying to allocate a larger window.

     If next_out is not null, the library will use this buffer for the history
   buffer; the buffer must either be large enough to hold the entire output
   data, or have at least 1<<windowBits bytes.  If next_out is null, the
   library will allocate its own buffer (and leave next_out null). next_in
   need not be provided here but must be provided by the application for the
   next call of inflate().

     If the history buffer is provided by the application, next_out must
   never be changed by the application since the decompressor maintains
   history information inside this buffer from call to call; the application
   can only reset next_out to the beginning of the history buffer when
   avail_out is zero and all output has been consumed.

      inflateInit2 returns Z_OK if success, Z_MEM_ERROR if there was
   not enough memory, Z_STREAM_ERROR if a parameter is invalid (such as
   windowBits < 8). msg is set to null if there is no error message.
   inflateInit2 does not perform any decompression: this will be done by
   inflate().
*/

extern int inflateSync OF((z_stream *strm));
/* 
    Skips invalid compressed data until the special marker (see deflate()
  above) can be found, or until all available input is skipped. No output
  is provided.

    inflateSync returns Z_OK if the special marker has been found, Z_BUF_ERROR
  if no more input was provided, Z_DATA_ERROR if no marker has been found,
  or Z_STREAM_ERROR if the stream structure was inconsistent. In the success
  case, the application may save the current current value of total_in which
  indicates where valid compressed data was found. In the error case, the
  application may repeatedly call inflateSync, providing more input each time,
  until success or end of the input data.
*/

extern int inflateReset OF((z_stream *strm));
/*
     This function is equivalent to inflateEnd followed by inflateInit,
   but does not free and reallocate all the internal decompression state.
   The stream will keep attributes that may have been set by inflateInit2.

      inflateReset returns Z_OK if success, or Z_STREAM_ERROR if the source
   stream state was inconsistent (such as zalloc or state being NULL).
*/

extern int inflateIncomp OF((z_stream *strm));
/*
     This function adds the data at next_in (avail_in bytes) to the output
   history without performing any output.  There must be no pending output,
   and the decompressor must be expecting to see the start of a block.
   Calling this function is equivalent to decompressing a stored block
   containing the data at next_in (except that the data is not output).
*/

                        /* checksum functions */

/*
     This function is not related to compression but is exported
   anyway because it might be useful in applications using the
   compression library.
*/

extern uLong adler32 OF((uLong adler, Bytef *buf, uInt len));

/*
     Update a running Adler-32 checksum with the bytes buf[0..len-1] and
   return the updated checksum. If buf is NULL, this function returns
   the required initial value for the checksum.
   An Adler-32 checksum is almost as reliable as a CRC32 but can be computed
   much faster. Usage example:

     uLong adler = adler32(0L, Z_NULL, 0);

     while (read_buffer(buffer, length) != EOF) {
       adler = adler32(adler, buffer, length);
     }
     if (adler != original_adler) error();
*/

#ifndef _Z_UTIL_H
    struct internal_state {int dummy;}; /* hack for buggy compilers */
#endif

#endif /* _ZLIB_H */
