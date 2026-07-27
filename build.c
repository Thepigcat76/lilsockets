/* GURD Build Tool - <https://github.com/Thepigcat76/gurd> */

#include "gurd.h"

#define COMPILER "gcc"
#define STANDARD "c23"
#define DEBUG true

#define PROJECT_NAME "lilsockets"

#define OUT_NAME "build/" PROJECT_NAME

// The project directory containing header files
#define INCLUDE_DIR "./include/"
// The directory where header files should be moved to after installation
#define INSTALL_INCLUDE_DIR "/usr/include/"

#define LIB_PTHREAD "pthread"

static Cmd cmd = {0};

static void visit_entry(struct file_entry entry) {
  if (entry.file_ext == NULL || strcmp(entry.file_ext, "c") != 0)
    return;

  cmd_appendf(&cmd, "%s", entry.path);
}

static void lib_install(void);

int main(int argc, char **argv) {
  // Remove old build files
  remove_dir_recursive("build", false);

  if (arg_eq(argc, argv, 1, "install")) {
    lib_install();
    return 0;
  }

  // The compiler to use
  cmd_appendf(&cmd, COMPILER);
  // Flags
  cmd_appendf(&cmd, "-std=%s", STANDARD);
  if (DEBUG) {
    cmd_appendf(&cmd, "-g");
  }
  // Output location
  cmd_appendf(&cmd, "-o");
  cmd_appendf(&cmd, OUT_NAME);

  if (args_contains(argc, argv, "--server") != -1) {
    cmd_appendf(&cmd, "-DSERVER");
  }

  // Adding src files
  walk_dir("src", visit_entry);

  // Libraries
  cmd_appendf(&cmd, "-l%s", LIB_PTHREAD);

  // Make sure out path exists
  ensure_parent_dirs(OUT_NAME, 0755);

  // Run the command
  cmd_execute(&cmd);

  // Run program
  if (arg_eq(argc, argv, 1, "r")) {
    systemf("./" OUT_NAME);
  }
}

static void copy_lib_header_file(struct file_entry file) {
  char dest_file_buf[256];
  sprintf(dest_file_buf, INSTALL_INCLUDE_DIR "/%s", file.name);
  copy_file(file.path, dest_file_buf);
}

static void lib_install(void) {
  if (geteuid() != 0) {
    fprintf(stderr, "Please run the install step with sudo.\n");
    exit(EXIT_FAILURE);
  }

  make_dirs(INSTALL_INCLUDE_DIR, 0755);

  walk_dir(INCLUDE_DIR, copy_lib_header_file);
}
