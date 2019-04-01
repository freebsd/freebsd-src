#include "capsicum-test.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <map>
#include <vector>
#include <string>

bool verbose = false;
bool tmpdir_on_tmpfs = false;
bool force_mt = false;
bool force_nofork = false;
uid_t other_uid = 0;

namespace {
std::map<std::string, std::string> tmp_paths;
}

const char *TmpFile(const char *p) {
  std::string pathname(p);
  if (tmp_paths.find(pathname) == tmp_paths.end()) {
    std::string fullname = tmpdir + "/" + pathname;
    tmp_paths[pathname] = fullname;
  }
  return tmp_paths[pathname].c_str();
}

char ProcessState(int pid) {
#ifdef __linux__
  // Open the process status file.
  char s[1024];
  snprintf(s, sizeof(s), "/proc/%d/status", pid);
  FILE *f = fopen(s, "r");
  if (f == NULL) return '\0';

  // Read the file line by line looking for the state line.
  const char *prompt = "State:\t";
  while (!feof(f)) {
    fgets(s, sizeof(s), f);
    if (!strncmp(s, prompt, strlen(prompt))) {
      fclose(f);
      return s[strlen(prompt)];
    }
  }
  fclose(f);
  return '?';
#endif
#ifdef __FreeBSD__
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "ps -p %d -o state | grep -v STAT", pid);
  sig_t original = signal(SIGCHLD, SIG_IGN);
  FILE* cmd = popen(buffer, "r");
  usleep(50000);  // allow any pending SIGCHLD signals to arrive
  signal(SIGCHLD, original);
  int result = fgetc(cmd);
  fclose(cmd);
  // Map FreeBSD codes to Linux codes.
  switch (result) {
    case EOF:
      return '\0';
    case 'D': // disk wait
    case 'R': // runnable
    case 'S': // sleeping
    case 'T': // stopped
    case 'Z': // zombie
      return result;
    case 'W': // idle interrupt thread
      return 'S';
    case 'I': // idle
      return 'S';
    case 'L': // waiting to acquire lock
    default:
      return '?';
  }
#endif
}

typedef std::vector<std::string> TestList;
typedef std::map<std::string, TestList*> SkippedTestMap;
static SkippedTestMap skipped_tests;
void TestSkipped(const char *testcase, const char *test, const std::string& reason) {
  if (skipped_tests.find(reason) == skipped_tests.end()) {
    skipped_tests[reason] = new TestList;
  }
  std::string testname(testcase);
  testname += ".";
  testname += test;
  skipped_tests[reason]->push_back(testname);
}

void ShowSkippedTests(std::ostream& os) {
  for (SkippedTestMap::iterator skiplist = skipped_tests.begin();
       skiplist != skipped_tests.end(); ++skiplist) {
    os << "Following tests were skipped because: " << skiplist->first << std::endl;
    for (size_t ii = 0; ii < skiplist->second->size(); ++ii) {
      const std::string& testname((*skiplist->second)[ii]);
      os << "  " << testname << std::endl;
    }
  }
}
