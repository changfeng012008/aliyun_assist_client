// Copyright (c) 2017-2018 Alibaba Group Holding Limited.

#include <string>
#include "./packagemanager/packagemanager.h"
#include "optparse/OptionParser.h"
#include "utils/SubProcess.h"
#include "utils/Log.h"
#include "utils/AssistPath.h"
#include "utils/FileUtil.h"
#include "utils/CheckNet.h"
#include "../VersionInfo.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using optparse::OptionParser;

OptionParser& initParser() {
  static OptionParser parser = OptionParser().description("");

  parser.add_option("-v", "--version")
    .dest("version")
    .action("store_true")
    .help("show version and exit");

  parser.add_option("-l", "--list")
      .dest("list")
      .action("store_true")
      .help("list all packages");

  parser.add_option("-o", "--local")
      .dest("local")
      .action("store_true")
      .help("list all installed packages");

  parser.add_option("-a", "--latest")
      .dest("latest")
      .action("store_true")
      .help("list all packages which have new version");

  parser.add_option("-i", "--install")
      .dest("install")
      .action("store")
      .help("install package");

  parser.add_option("-u", "--uninstall")
      .dest("uninstall")
      .action("store")
      .help("uninstall package");

  parser.add_option("-d", "--update")
      .dest("update")
      .action("store")
      .help("update package");

  parser.add_option("-p", "--package")
      .dest("package")
      .action("store");

  parser.add_option("-e", "--package_version")
      .dest("package_version")
      .action("store");

  parser.add_option("-r", "--arch")
      .dest("arch")
      .action("store");

    return parser;
}

bool process_singleton() {
#ifdef _WIN32
  if (NULL == ::CreateMutex(NULL, FALSE, L"alyun_installer")) {
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
      return false;
    }
  }
#else

#endif
  return true;
}

int main(int argc, char *argv[]) {
  AssistPath path_service("");
  std::string log_path = path_service.GetLogPath();
  log_path += FileUtils::separator();
  log_path += "aliyun_installer.log";
  Log::Initialise(log_path);

  if ( !process_singleton() ) {
    Log::Error("exit by another installer process is running");
    return -1;
  }

  OptionParser& parser = initParser();
  optparse::Values options = parser.parse_args(argc, argv);
  alyun_assist_installer::PackageManager package_mgr;

  HostChooser host_choose;
  bool found = host_choose.Init(path_service.GetConfigPath());
  if (!found) {
    Log::Error("could not find a match region host");
    return -1;
  }

  if (options.is_set("version")) {
    printf("%s\n", FILE_VERSION_RESOURCE_STR);
    return 0;
  }

  if (options.is_set("list")) {
    std::string package_name = options.get("package");
    package_mgr.List(package_name);
    return 0;
  }

  if (options.is_set("local")) {
    std::string package_name = options.get("package");
    package_mgr.Local(package_name);
    return 0;
  }

  if (options.is_set("latest")) {
    std::string package_name = options.get("package");
    package_mgr.Latest(package_name);
    return 0;
  }

  if (options.is_set("install")) {
    std::string package_name = options.get("install");
    std::string package_version = options.get("package_version");
    std::string arch = options.get("arch");
    package_mgr.Install(package_name, package_version, arch);
    return 0;
  }

  if (options.is_set("uninstall")) {
    std::string package_name = options.get("uninstall");
    package_mgr.Uninstall(package_name);
    return 0;
  }

  if (options.is_set("update")) {
    std::string package_name = options.get("arch");
    package_mgr.Update(package_name);
    return 0;
  }

  parser.print_help();
}

