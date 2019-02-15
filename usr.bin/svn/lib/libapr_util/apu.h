/* $FreeBSD$ */
/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * apu.h is generated from apu.h.in by configure -- do not edit apu.h
 */
/* @file apu.h
 * @brief APR-Utility main file
 */
/**
 * @defgroup APR_Util APR Utility Functions
 * @{
 */


#ifndef APU_H
#define APU_H

/**
 * APU_DECLARE_EXPORT is defined when building the APR-UTIL dynamic library,
 * so that all public symbols are exported.
 *
 * APU_DECLARE_STATIC is defined when including the APR-UTIL public headers,
 * to provide static linkage when the dynamic library may be unavailable.
 *
 * APU_DECLARE_STATIC and APU_DECLARE_EXPORT are left undefined when
 * including the APR-UTIL public headers, to import and link the symbols from 
 * the dynamic APR-UTIL library and assure appropriate indirection and calling
 * conventions at compile time.
 */

/**
 * The public APR-UTIL functions are declared with APU_DECLARE(), so they may
 * use the most appropriate calling convention.  Public APR functions with 
 * variable arguments must use APU_DECLARE_NONSTD().
 *
 * @deffunc APU_DECLARE(rettype) apr_func(args);
 */
#define APU_DECLARE(type)            type
/**
 * The public APR-UTIL functions using variable arguments are declared with 
 * APU_DECLARE_NONSTD(), as they must use the C language calling convention.
 *
 * @deffunc APU_DECLARE_NONSTD(rettype) apr_func(args, ...);
 */
#define APU_DECLARE_NONSTD(type)     type
/**
 * The public APR-UTIL variables are declared with APU_DECLARE_DATA.
 * This assures the appropriate indirection is invoked at compile time.
 *
 * @deffunc APU_DECLARE_DATA type apr_variable;
 * @tip APU_DECLARE_DATA extern type apr_variable; syntax is required for
 * declarations within headers to properly import the variable.
 */
#define APU_DECLARE_DATA
/*
 * we always have SDBM (it's in our codebase)
 */
#define APU_HAVE_SDBM   1
#define APU_HAVE_GDBM   0
#define APU_HAVE_NDBM   0
#define APU_HAVE_DB     0

#if APU_HAVE_DB
#define APU_HAVE_DB_VERSION    0
#endif /* APU_HAVE_DB */

#define APU_HAVE_PGSQL         0
#define APU_HAVE_MYSQL         0
#define APU_HAVE_SQLITE3       0
#define APU_HAVE_SQLITE2       0

#define APU_HAVE_APR_ICONV     0
#define APU_HAVE_ICONV         0
#define APR_HAS_XLATE          (APU_HAVE_APR_ICONV || APU_HAVE_ICONV)

#endif /* APU_H */
/** @} */
