cmake_minimum_required(VERSION 3.9)

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(is_multi_config)
    set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING
        "Semicolon separated list of supported configuration types")
    mark_as_advanced(CMAKE_CONFIGURATION_TYPES)
elseif(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_C_FLAGS)
    message(WARNING "No CMAKE_BUILD_TYPE is selected")
endif()

project(nvi2 C)

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckStructHasMember)
include(CheckCSourceCompiles)

mark_as_advanced(CMAKE_INSTALL_PREFIX)

option(USE_WIDECHAR "Enable wide character support" ON)
option(USE_ICONV "Enable iconv support" ON)

add_compile_options(-fcolor-diagnostics)
add_compile_options($<$<CONFIG:Debug>:-Wall>)
add_compile_options($<$<CONFIG:Debug>:-Wno-parentheses>)
add_compile_options($<$<CONFIG:Debug>:-Wno-uninitialized>)
add_compile_options($<$<CONFIG:Debug>:-Wmissing-prototypes>)
add_compile_options($<$<CONFIG:Debug>:-Wsystem-headers>)
add_compile_options($<$<CONFIG:Release>:-Wuninitialized>)
add_compile_options($<$<CONFIG:Release>:-Wno-dangling-else>)
add_compile_options(-Wstack-protector -fstack-protector)
add_compile_options(-Wstrict-aliasing -fstrict-aliasing)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(MAIN_PROTOS
    cl/extern.h common/extern.h ex/extern.h vi/extern.h
    common/options_def.h ex/ex_def.h ex/version.h)

set(CL_SRCS
    cl/cl_funcs.c cl/cl_main.c cl/cl_read.c cl/cl_screen.c cl/cl_term.c)

set(COMMON_SRCS
    common/conv.c common/cut.c common/delete.c common/encoding.c common/exf.c
    common/key.c common/line.c common/log.c common/main.c common/mark.c
    common/msg.c common/options.c common/options_f.c common/put.c
    common/recover.c common/screen.c common/search.c common/seq.c
    common/util.c)

set(EX_SRCS
    ex/ex.c ex/ex_abbrev.c ex/ex_append.c ex/ex_args.c ex/ex_argv.c ex/ex_at.c
    ex/ex_bang.c ex/ex_cd.c ex/ex_cmd.c ex/ex_cscope.c ex/ex_delete.c
    ex/ex_display.c ex/ex_edit.c ex/ex_equal.c ex/ex_file.c ex/ex_filter.c
    ex/ex_global.c ex/ex_init.c ex/ex_join.c ex/ex_map.c ex/ex_mark.c
    ex/ex_mkexrc.c ex/ex_move.c ex/ex_open.c ex/ex_preserve.c ex/ex_print.c
    ex/ex_put.c ex/ex_quit.c ex/ex_read.c ex/ex_screen.c ex/ex_script.c
    ex/ex_set.c ex/ex_shell.c ex/ex_shift.c ex/ex_source.c ex/ex_stop.c
    ex/ex_subst.c ex/ex_tag.c ex/ex_txt.c ex/ex_undo.c ex/ex_usage.c
    ex/ex_util.c ex/ex_version.c ex/ex_visual.c ex/ex_write.c ex/ex_yank.c
    ex/ex_z.c)

set(VI_SRCS
    vi/getc.c vi/v_at.c vi/v_ch.c vi/v_cmd.c vi/v_delete.c vi/v_ex.c
    vi/v_increment.c vi/v_init.c vi/v_itxt.c vi/v_left.c vi/v_mark.c
    vi/v_match.c vi/v_paragraph.c vi/v_put.c vi/v_redraw.c vi/v_replace.c
    vi/v_right.c vi/v_screen.c vi/v_scroll.c vi/v_search.c vi/v_section.c
    vi/v_sentence.c vi/v_status.c vi/v_txt.c vi/v_ulcase.c vi/v_undo.c
    vi/v_util.c vi/v_word.c vi/v_xchar.c vi/v_yank.c vi/v_z.c vi/v_zexit.c
    vi/vi.c vi/vs_line.c vi/vs_msg.c vi/vs_refresh.c vi/vs_relative.c
    vi/vs_smap.c vi/vs_split.c)

set(REGEX_SRCS
    regex/regcomp.c regex/regerror.c regex/regexec.c regex/regfree.c)

# commands to generate the public headers
set(extract_protos sed -n 's/^ \\* PUBLIC: \\\(.*\\\)/\\1/p')
set(extract_version sed -n
    's/^.*version \\\([^\)]*\)\\\).*/\#define VI_VERSION \\\"\\1\\\"/p')

add_custom_command(OUTPUT cl/extern.h
                   COMMAND ${extract_protos} ${CL_SRCS} > cl/extern.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   DEPENDS ${CL_SRCS})
add_custom_command(OUTPUT common/extern.h
                   COMMAND ${extract_protos} ${COMMON_SRCS} > common/extern.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   DEPENDS ${COMMON_SRCS})
add_custom_command(OUTPUT ex/extern.h
                   COMMAND ${extract_protos} ${EX_SRCS} > ex/extern.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   DEPENDS ${EX_SRCS})
add_custom_command(OUTPUT vi/extern.h
                   COMMAND ${extract_protos} ${VI_SRCS} > vi/extern.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   DEPENDS ${VI_SRCS})
add_custom_command(OUTPUT common/options_def.h
                   COMMAND awk -f common/options.awk
                           common/options.c > common/options_def.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   DEPENDS common/options.c)
add_custom_command(OUTPUT ex/ex_def.h
                   COMMAND awk -f ex/ex.awk ex/ex_cmd.c > ex/ex_def.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   DEPENDS ex/ex_cmd.c)
add_custom_command(OUTPUT ex/version.h
                   COMMAND ${extract_version} README > ex/version.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   DEPENDS README)

add_executable(nvi)
target_sources(nvi PRIVATE ${MAIN_PROTOS} ${CL_SRCS} ${COMMON_SRCS}
                           ${EX_SRCS} ${VI_SRCS})
target_compile_definitions(nvi PRIVATE $<$<CONFIG:Debug>:DEBUG>
                                       $<$<CONFIG:Debug>:COMLOG>)

check_function_exists(openpty UTIL_IN_LIBC)
if(NOT UTIL_IN_LIBC)
    find_library(UTIL_LIBRARY util)
    target_link_libraries(nvi PRIVATE ${UTIL_LIBRARY})
endif()

check_function_exists(__b64_ntop RESOLV_IN_LIBC)
if(NOT RESOLV_IN_LIBC)
    find_library(RESOLV_LIBRARY resolv)
    target_link_libraries(nvi PRIVATE ${RESOLV_LIBRARY})
endif()

if(USE_WIDECHAR)
    find_library(CURSES_LIBRARY NAMES ncursesw cursesw curses HINTS /usr/lib)
    find_library(TERMINFO_LIBRARY NAMES tinfow terminfo HINTS /usr/lib)

    # link to the wchar_t awared BSD libregex.a
    add_library(regex STATIC)
    target_sources(regex PRIVATE ${REGEX_SRCS})
    target_include_directories(regex PUBLIC regex)
    target_compile_definitions(regex PUBLIC __REGEX_PRIVATE)
    target_link_libraries(nvi PRIVATE regex)
else()
    find_library(CURSES_LIBRARY NAMES ncurses curses HINTS /usr/lib)
    find_library(TERMINFO_LIBRARY NAMES tinfo terminfo HINTS /usr/lib)
    target_compile_options(nvi PRIVATE -Wno-pointer-sign)
endif()

target_link_libraries(nvi PRIVATE ${CURSES_LIBRARY} ${TERMINFO_LIBRARY})

if(USE_ICONV)
    check_function_exists(iconv ICONV_IN_LIBC)
    if(NOT ICONV_IN_LIBC)
        find_path(ICONV_INCLUDE_DIR iconv.h)
        find_library(ICONV_LIBRARY iconv)
    endif()

    # detect the prototype of iconv(3)
    set(CMAKE_C_FLAGS_BACKUP "${CMAKE_C_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
    set(CMAKE_REQUIRED_INCLUDES "${ICONV_INCLUDE_DIR}")
    set(CMAKE_REQUIRED_LIBRARIES "${ICONV_LIBRARY}")
    check_c_source_compiles("
    #include <iconv.h>
    int main() {
        iconv_t conv = 0;
        char* in = 0;
        size_t ilen = 0;
        char* out = 0;
        size_t olen = 0;
        iconv(conv, &in, &ilen, &out, &olen);
        return 0;
    }
    " ICONV_TRADITIONAL)
    set(CMAKE_REQUIRED_INCLUDES)
    set(CMAKE_REQUIRED_LIBRARIES)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_BACKUP}")

    target_include_directories(nvi PRIVATE ${ICONV_INCLUDE_DIR})
    target_link_libraries(nvi PRIVATE ${ICONV_LIBRARY})
endif()

check_function_exists(getprogname GETPROGNAME_IN_LIBC)
check_function_exists(strlcpy STRLCPY_IN_LIBC)
if(NOT GETPROGNAME_IN_LIBC OR NOT STRLCPY_IN_LIBC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBBSD libbsd-overlay)
    add_definitions(${LIBBSD_CFLAGS})
    target_link_libraries(nvi PRIVATE ${LIBBSD_LIBRARIES})
endif()

check_function_exists(dbopen DBOPEN_IN_LIBC)
if(NOT DBOPEN_IN_LIBC)
    target_link_libraries(nvi PRIVATE db1)
endif()

check_include_files(libutil.h HAVE_LIBUTIL_H)
check_include_files(ncurses.h HAVE_NCURSES_H)
check_include_files(ncursesw/ncurses.h HAVE_NCURSESW_NCURSES_H)
check_include_files(pty.h HAVE_PTY_H)
check_include_files(term.h HAVE_TERM_H)
check_struct_has_member("struct dirent" d_namlen dirent.h HAVE_DIRENT_D_NAMLEN LANGUAGE C)

configure_file(files/config.h.in config.h)

set(vi_cv_path_preserve /var/tmp/vi.recover/)
if(APPLE)
    set(vi_cv_path_msgcat /usr/local/share/vi/catalog/)
else()
    set(vi_cv_path_msgcat /usr/share/vi/catalog/)
endif()

configure_file(files/pathnames.h.in pathnames.h)
configure_file(files/recover.in recover @ONLY)
