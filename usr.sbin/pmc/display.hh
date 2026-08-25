/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Netflix, Inc.
 *
 * This software was developed by Ali Mashtizadeh under the sponsorship from
 * Netflix, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define COLOR256_GREEN		46
#define COLOR256_RED		196
#define COLOR256_YELLOW		226

void setup_screen();

/* Formatting */
std::string format_siprefix(uint64_t count);
std::string format_binprefix(uint64_t count);
std::string format_sample(uint64_t count, uint64_t total);
std::string format_percent(uint64_t count, uint64_t total);

/* Printing */
enum class siunit {
	seconds,
	// Special
	cycles,
	percent
};

void title(const std::string &msg);
void header(const std::string &msg);
void printval(const std::string &msg, uint64_t val, siunit ui);
void printval(const std::string &msg, float val, siunit ui);
void printbar(float percent);

/* Tables */
class field
{
public:
	field() { }
	field(const std::string &s);
	field(int64_t v);
	field(float v);
	field(int64_t count, int64_t total, bool percent = false);
	field(int64_t user_count, int64_t user_total,
	    int64_t kernel_count, int64_t kernel_total);
	virtual ~field() { }
	virtual bool operator>(const field &b) const;
	virtual bool operator<(const field &b) const;
	virtual std::string to_string();
	virtual size_t length();
private:
	enum class FIELD {
		STRING,
		INTEGER,
		FLOAT,
		SAMPLE,
		DSAMPLE,
		PERCENT
	};
	FIELD type;
	int64_t value[4];
	float fvalue;
	std::string svalue;
};

class table
{
public:
	table();
	~table();
	void addcolumn(const std::string &c, bool alignleft = false);
	void addrow(std::vector<field> r);
	void sort(int col, bool descending = true);
	void print();
private:
	int sortcol;
	bool sortdir;
	std::vector<std::string> cols;
	std::vector<bool> align;
	std::vector<std::vector<field>> rows;
};

