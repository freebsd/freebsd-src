/*
 *  Generated header for gperf generated source Fri Apr 29 15:43:59 PDT 2011
 *  This file enumerates the list of names and declares the
 *  procedure for mapping string names to the enum value.
 */
#ifndef AUTOOPTS_VALUE_TYPE_H_GUARD
#define AUTOOPTS_VALUE_TYPE_H_GUARD 1

typedef enum {
    VTP_KWD_INVALID,
    VTP_KWD_STRING,
    VTP_KWD_INTEGER,
    VTP_KWD_BOOLEAN,
    VTP_KWD_BOOL,
    VTP_KWD_KEYWORD,
    VTP_KWD_SET,
    VTP_KWD_SET_MEMBERSHIP,
    VTP_KWD_NESTED,
    VTP_KWD_HIERARCHY,
    VTP_COUNT_KWD
} value_type_enum_t;

extern value_type_enum_t
find_value_type_id(char const * str, unsigned int len);
#endif /* AUTOOPTS_VALUE_TYPE_H_GUARD */
