//===- IndexProvider.cpp - Maps information to translation units -*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  Maps information to TranslationUnits.
//
//===----------------------------------------------------------------------===//

#include "clang/Index/IndexProvider.h"
#include "clang/Index/Entity.h"
using namespace clang;
using namespace idx;

// Out-of-line to give the virtual table a home.
IndexProvider::~IndexProvider() { }
