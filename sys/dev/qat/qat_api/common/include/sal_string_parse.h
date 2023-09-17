/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file sal_string_parse.h
 *
 * @defgroup SalStringParse
 *
 * @ingroup SalStringParse
 *
 * @description
 *     This file contains string parsing functions
 *
 *****************************************************************************/

#ifndef SAL_STRING_PARSE_H
#define SAL_STRING_PARSE_H

/* Maximum size of the strings used by SAL */
#define SAL_CFG_MAX_VAL_LEN_IN_BYTES 64

/**
*******************************************************************************
 * @ingroup SalStringParse
 *      Builds a string and store it in result
 *
 * @description
 *      The result string will be the concatenation of string1, instanceNumber
 *      and string2. The size of result has to be SAL_CFG_MAX_VAL_LEN_IN_BYTES.
 *      We can't check this in this function, this is the user responsibility
 *
 * @param[in]  string1          First string to concatenate
 * @param[in]  instanceNumber   Instance number
 * @param[in]  string2          Second string to concatenate
 * @param[out] result           Resulting string of concatenation
 *
 * @retval CPA_STATUS_SUCCESS   Function executed successfully
 * @retval CPA_STATUS_FAIL      Function failed
 *
 *****************************************************************************/
CpaStatus Sal_StringParsing(char *string1,
			    Cpa32U instanceNumber,
			    char *string2,
			    char *result);

/**
*******************************************************************************
 * @ingroup SalStringParse
 *      Convert a string to an unsigned long
 *
 * @description
 *      Parses the string cp in the specified base, and returned it as an
 *      unsigned long value.
 *
 * @param[in]  cp       String to be converted
 * @param[in]  endp     Pointer to the end of the string. This parameter
 *                      can also be NULL and will not be used in this case
 * @param[in]  cfgBase  Base to convert the string
 *
 * @retval  The string converted to an unsigned long
 *
 *****************************************************************************/
Cpa64U Sal_Strtoul(const char *cp, char **endp, unsigned int cfgBase);

#endif /* SAL_STRING_PARSE_H */
