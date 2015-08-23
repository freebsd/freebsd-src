#ifndef FILE_HANDLING_TEST_H
#define FILE_HANDLING_TEST_H

#include "g_sntptest.h"

#include <fstream>
#include <string>

using std::ifstream;
using std::string;
using std::ios;

class fileHandlingTest : public sntptest {
protected:
	enum DirectoryType {
		INPUT_DIR = 0,
		OUTPUT_DIR = 1
	};

	std::string CreatePath(const char* filename, DirectoryType argument) {
		std::string path;

		if (m_params.size() >= argument + 1) {
			path = m_params[argument];
		}

		if (path[path.size()-1] != DIR_SEP && !path.empty()) {
			path.append(1, DIR_SEP);
		}
		path.append(filename);

		return path;
	}

	int GetFileSize(ifstream& file) {
		int initial = file.tellg();

		file.seekg(0, ios::end);
		int length = file.tellg();
		file.seekg(initial);

		return length;
	}

	void CompareFileContent(ifstream& expected, ifstream& actual) {
		int currentLine = 1;
		while (actual.good() && expected.good()) {
			string actualLine, expectedLine;
			getline(actual, actualLine);
			getline(expected, expectedLine);
			
			EXPECT_EQ(expectedLine, actualLine) << "Comparision failed on line " << currentLine;
			currentLine++;
		}
	}

	void ClearFile(const std::string& filename) {
		std::ofstream clear(filename.c_str(), ios::trunc);
		ASSERT_TRUE(clear.good());
		clear.close();
	}
};

#endif // FILE_HANDLING_TEST_H
