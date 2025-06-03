//===-- SBLanguages.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_API_SBLANGUAGE_H
#define LLDB_API_SBLANGUAGE_H

namespace lldb {
/// Used by \ref SBExpressionOptions.
/// These enumerations use the same language enumerations as the DWARF
/// specification for ease of use and consistency.
enum SBSourceLanguageName : uint16_t {
  /// ISO Ada.
  eLanguageNameAda = 0x0001,
  /// BLISS.
  eLanguageNameBLISS = 0x0002,
  /// C (K&R and ISO).
  eLanguageNameC = 0x0003,
  /// ISO C++.
  eLanguageNameC_plus_plus = 0x0004,
  /// ISO Cobol.
  eLanguageNameCobol = 0x0005,
  /// Crystal.
  eLanguageNameCrystal = 0x0006,
  /// D.
  eLanguageNameD = 0x0007,
  /// Dylan.
  eLanguageNameDylan = 0x0008,
  /// ISO Fortran.
  eLanguageNameFortran = 0x0009,
  /// Go.
  eLanguageNameGo = 0x000a,
  /// Haskell.
  eLanguageNameHaskell = 0x000b,
  /// Java.
  eLanguageNameJava = 0x000c,
  /// Julia.
  eLanguageNameJulia = 0x000d,
  /// Kotlin.
  eLanguageNameKotlin = 0x000e,
  /// Modula 2.
  eLanguageNameModula2 = 0x000f,
  /// Modula 3.
  eLanguageNameModula3 = 0x0010,
  /// Objective C.
  eLanguageNameObjC = 0x0011,
  /// Objective C++.
  eLanguageNameObjC_plus_plus = 0x0012,
  /// OCaml.
  eLanguageNameOCaml = 0x0013,
  /// OpenCL C.
  eLanguageNameOpenCL_C = 0x0014,
  /// ISO Pascal.
  eLanguageNamePascal = 0x0015,
  /// ANSI PL/I.
  eLanguageNamePLI = 0x0016,
  /// Python.
  eLanguageNamePython = 0x0017,
  /// RenderScript Kernel Language.
  eLanguageNameRenderScript = 0x0018,
  /// Rust.
  eLanguageNameRust = 0x0019,
  /// Swift.
  eLanguageNameSwift = 0x001a,
  /// Unified Parallel C (UPC).
  eLanguageNameUPC = 0x001b,
  /// Zig.
  eLanguageNameZig = 0x001c,
  /// Assembly.
  eLanguageNameAssembly = 0x001d,
  /// C#.
  eLanguageNameC_sharp = 0x001e,
  /// Mojo.
  eLanguageNameMojo = 0x001f,
  /// OpenGL Shading Language.
  eLanguageNameGLSL = 0x0020,
  /// OpenGL ES Shading Language.
  eLanguageNameGLSL_ES = 0x0021,
  /// High Level Shading Language.
  eLanguageNameHLSL = 0x0022,
  /// OpenCL C++.
  eLanguageNameOpenCL_CPP = 0x0023,
  /// C++ for OpenCL.
  eLanguageNameCPP_for_OpenCL = 0x0024,
  /// SYCL.
  eLanguageNameSYCL = 0x0025,
  /// Ruby.
  eLanguageNameRuby = 0x0026,
  /// Move.
  eLanguageNameMove = 0x0027,
  /// Hylo.
  eLanguageNameHylo = 0x0028,
};

} // namespace lldb

#endif
