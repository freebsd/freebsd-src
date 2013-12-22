// RUN: rm -rf %t
// RUN: %clang_cc1 -Wauto-import -fmodules-cache-path=%t -fmodules -F %S/Inputs %s -verify

@import DependsOnModule.CXX; // expected-error{{module 'DependsOnModule.CXX' requires feature 'cplusplus'}}
@import DependsOnModule.NotCXX;
@import DependsOnModule.NotObjC; // expected-error{{module 'DependsOnModule.NotObjC' is incompatible with feature 'objc'}}
