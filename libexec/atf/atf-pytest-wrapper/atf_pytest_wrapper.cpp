// vim: ts=2 sw=2 et

#include <format>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <stdlib.h>
#include <unistd.h>

class Handler {
  private:
    const std::string kPytestName = "pytest";
    const std::string kCleanupSuffix = ":cleanup";
    const std::string kPythonPathEnv = "PYTHONPATH";
    const std::string kAtfVar = "_ATF_VAR_";
  public:
    // Test listing requested
    bool flag_list = false;
    // Output debug data (will break listing)
    bool flag_debug = false;
    // Cleanup for the test requested
    bool flag_cleanup = false;
    // Test source directory (provided by ATF)
    std::string src_dir;
    // Path to write test status to (provided by ATF)
    std::string dst_file;
    // Path to add to PYTHONPATH (provided by the schebang args)
    std::string python_path;
    // Path to the script (provided by the schebang wrapper)
    std::string script_path;
    // Name of the test to run (provided by ATF)
    std::string test_name;
    // kv pairs (provided by ATF)
    std::map<std::string,std::string> kv_map;
    // our binary name
    std::string binary_name;

    static std::vector<std::string> ToVector(int argc, char **argv) {
      std::vector<std::string> ret;

      for (int i = 0; i < argc; i++) {
        ret.emplace_back(std::string(argv[i]));
      }
      return ret;
    }

    static void PrintVector(std::string prefix, const std::vector<std::string> &vec) {
      std::cerr << prefix << ": ";
      for (auto &val: vec) {
        std::cerr << "'" << val << "' ";
      }
      std::cerr << std::endl;
    }

    void Usage(std::string msg, bool exit_with_error) {
      std::cerr << binary_name << ": ERROR: " << msg << "." << std::endl;
      std::cerr << binary_name << ": See atf-test-program(1) for usage details." << std::endl;
      exit(exit_with_error != 0);
    }

    // Parse args received from the OS. There can be multiple valid options:
    // * with schebang args (#!/binary -P/path):
    // atf_wrap '-P /path' /path/to/script -l
    // * without schebang args
    // atf_wrap /path/to/script -l
    // Running test:
    // atf_wrap '-P /path' /path/to/script -r /path1 -s /path2 -vk1=v1 testname
    void Parse(int argc, char **argv) {
      if (flag_debug) {
        PrintVector("IN", ToVector(argc, argv));
      }
      // getopt() skips the first argument (as it is typically binary name)
      // it is possible to have either '-P\s*/path' followed by the script name
      // or just the script name. Parse kernel-provided arg manually and adjust
      // array to make getopt work

      binary_name = std::string(argv[0]);
      argc--; argv++;
      // parse -P\s*path from the kernel.
      if (argc > 0 && !strncmp(argv[0], "-P", 2)) {
        char *path = &argv[0][2];
        while (*path == ' ')
          path++;
        python_path = std::string(path);
        argc--; argv++;
      }

      // The next argument is a script name. Copy and keep argc/argv the same
      // Show usage for empty args
      if (argc == 0) {
        Usage("Must provide a test case name", true);
      }
      script_path = std::string(argv[0]);

      int c;
      while ((c = getopt(argc, argv, "lr:s:v:")) != -1) {
        switch (c) {
          case 'l':
            flag_list = true;
            break;
          case 's':
            src_dir = std::string(optarg);
            break;
          case 'r':
            dst_file = std::string(optarg);
            break;
          case 'v':
            {
              std::string kv = std::string(optarg);
              size_t splitter = kv.find("=");
              if (splitter == std::string::npos) {
                Usage("Unknown variable: " + kv, true);
              }
              kv_map[kv.substr(0, splitter)] = kv.substr(splitter + 1);
            }
            break;
          default:
            Usage("Unknown option -" + std::string(1, static_cast<char>(c)), true);
        }
      }
      argc -= optind;
      argv += optind;

      if (flag_list) {
        return;
      }
      // There should be just one argument with the test name
      if (argc != 1) {
        Usage("Must provide a test case name", true);
      }
      test_name = std::string(argv[0]);
      if (test_name.size() > kCleanupSuffix.size() &&
          std::equal(kCleanupSuffix.rbegin(), kCleanupSuffix.rend(), test_name.rbegin())) {
        test_name = test_name.substr(0, test_name.size() - kCleanupSuffix.size());
        flag_cleanup = true;
      }
    }

    std::vector<std::string> BuildArgs() {
      std::vector<std::string> args = {"pytest", "-vv", "-p",
        "no:cacheprovider", "-s", "--atf"};

      args.push_back("--confcutdir=" + python_path);

      if (flag_list) {
        args.push_back("--co");
        args.push_back(script_path);
        return args;
      }
      if (flag_cleanup) {
        args.push_back("--atf-cleanup");
      }
      // workaround pytest parser bug:
      // https://github.com/pytest-dev/pytest/issues/3097
      // use '--arg=value' format instead of '--arg value' for all
      // path-like options
      if (!src_dir.empty()) {
        args.push_back("--atf-source-dir=" + src_dir);
      }
      if (!dst_file.empty()) {
        args.push_back("--atf-file=" + dst_file);
      }
      // Create nodeid from the test path &name
      args.push_back(script_path + "::" + test_name);
      return args;
    }

    void SetPythonPath() {
      if (!python_path.empty()) {
        char *env_path = getenv(kPythonPathEnv.c_str());
        if (env_path != nullptr) {
          python_path = python_path + ":" + std::string(env_path);
        }
        setenv(kPythonPathEnv.c_str(), python_path.c_str(), 1);
      }
    }

    void SetEnv() {
      SetPythonPath();

      // Pass ATF kv pairs as env variables to avoid dealing with
      // pytest parser
      for (auto [k, v]: kv_map) {
        setenv((kAtfVar + k).c_str(), v.c_str(), 1);
      }
    }

    bool Run(std::string binary, std::vector<std::string> args) {
      if (flag_debug) {
        PrintVector("OUT", args);
      }
      // allocate array with final NULL
      char **arr = new char*[args.size() + 1]();
      for (unsigned long i = 0; i < args.size(); i++) {
        // work around 'char *const *'
        arr[i] = strdup(args[i].c_str());
      }
      return execvp(binary.c_str(), arr) == 0;
    }

    void ReportError() {
      if (flag_list) {
        std::cout << "Content-Type: application/X-atf-tp; version=\"1\"";
        std::cout << std::endl << std::endl;
        std::cout << "ident: __test_cases_list_"<< kPytestName << "_binary_" <<
          "not_found__" << std::endl;
      } else {
        std::cout << "execvp(" << kPytestName << ") failed: " <<
          std::strerror(errno) << std::endl;
      }
    }

    int Process() {
      SetEnv();
      if (!Run(kPytestName, BuildArgs())) {
        ReportError();
      }
      return 0;
    }
};


int main(int argc, char **argv) {
  Handler handler;

  handler.Parse(argc, argv);
  return handler.Process();
}
