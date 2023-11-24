/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2015-2020 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#ifndef TERMINAL_H
#define TERMINAL_H

#define TERMINAL_FG_BLACK	"\x1b[30m"
#define TERMINAL_FG_RED		"\x1b[31m"
#define TERMINAL_FG_GREEN	"\x1b[32m"
#define TERMINAL_FG_YELLOW	"\x1b[33m"
#define TERMINAL_FG_BLUE	"\x1b[34m"
#define TERMINAL_FG_MAGENTA	"\x1b[35m"
#define TERMINAL_FG_CYAN	"\x1b[36m"
#define TERMINAL_FG_WHITE	"\x1b[37m"
#define TERMINAL_FG_DEFAULT	"\x1b[39m"

#define TERMINAL_BG_BLACK	"\x1b[40m"
#define TERMINAL_BG_RED		"\x1b[41m"
#define TERMINAL_BG_GREEN	"\x1b[42m"
#define TERMINAL_BG_YELLOW	"\x1b[43m"
#define TERMINAL_BG_BLUE	"\x1b[44m"
#define TERMINAL_BG_MAGENTA	"\x1b[45m"
#define TERMINAL_BG_CYAN	"\x1b[46m"
#define TERMINAL_BG_WHITE	"\x1b[47m"
#define TERMINAL_BG_DEFAULT	"\x1b[49m"

#define TERMINAL_BOLD		"\x1b[1m"
#define TERMINAL_NO_BOLD	"\x1b[22m"
#define TERMINAL_UNDERLINE	"\x1b[4m"
#define TERMINAL_NO_UNDERLINE	"\x1b[24m"

#define TERMINAL_RESET		"\x1b[0m"

#define TERMINAL_SAVE_CURSOR	"\x1b[s"
#define TERMINAL_RESTORE_CURSOR	"\x1b[u"
#define TERMINAL_UP_CURSOR(l)	"\x1b[" #l "A"
#define TERMINAL_DOWN_CURSOR(l)	"\x1b[" #l "B"
#define TERMINAL_RIGHT_CURSOR(c) "\x1b[" #c "C"
#define TERMINAL_LEFT_CURSOR(c)	"\x1b[" #c "D"
#define TERMINAL_CLEAR_DOWN	"\x1b[0J"
#define TERMINAL_CLEAR_UP	"\x1b[1J"
#define TERMINAL_CLEAR_RIGHT	"\x1b[0K"
#define TERMINAL_CLEAR_LEFT	"\x1b[1K"
#define TERMINAL_CLEAR_LINE	"\x1b[2K"
#define TERMINAL_CLEAR_ALL	"\x1b[2J"

void terminal_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
