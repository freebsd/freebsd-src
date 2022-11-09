/*
   LZ4 - Fast LZ compression algorithm
   Header File
   Copyright (C) 2011-2013, Yann Collet.
   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - LZ4 homepage : http://fastcompression.blogspot.com/p/lz4.html
   - LZ4 source repository : http://code.google.com/p/lz4/
*/
#pragma once

#if defined (__cplusplus)
extern "C" {
#endif


//**************************************
// Compiler Options
//**************************************
//Should go here if they are needed

//****************************
// Simple Functions
//****************************

int LZ4_decompress_safe (char* source, char* dest, int inputSize,
						int maxOutputSize);

/*
LZ4_decompress_safe() :
    maxOutputSize : 
     is the size of the destination buffer (which must be already allocated)
    return :
     the number of bytes decoded in the destination buffer
     (necessarily <= maxOutputSize)
     If the source stream is malformed or too large, the function will
     stop decoding and return a negative result.
     This function is protected against any kind of buffer overflow attemps 
     (never writes outside of output buffer, and never reads outside of 
     input buffer). It is therefore protected against malicious data packets
*/


//****************************
// Advanced Functions
//****************************

int LZ4_compress_limitedOutput(char* source, char* dest, int inputSize,
						int maxOutputSize);

/*
LZ4_compress_limitedOutput() :
    Compress 'inputSize' bytes from 'source' into an output buffer 'dest'
    of maximum size 'maxOutputSize'.
    If it cannot achieve it, compression will stop, and result of
    the function will be zero.
    This function never writes outside of provided output buffer.

    inputSize  : 
     Max supported value is ~1.9GB
    maxOutputSize : 
     is the size of the destination buffer (which must bealready allocated)
    return :
     the number of bytes written in buffer 'dest' or 0 if the compression fails
*/

#if defined (__cplusplus)
}
#endif
