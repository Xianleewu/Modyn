#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include "modyn.h"

static void modyn_autoload_from_dir(const char *dir) {
  if (!dir || !*dir) return;
  (void)modyn_load_device_drivers_from_directory(dir);
  (void)modyn_load_model_loaders_from_directory(dir);
  (void)modyn_load_mempools_from_directory(dir);
}

static void modyn_autoload_from_env_or_default(void) {
  const char *env = getenv("MODYN_PLUGIN_DIR");
  if (env && *env) {
    // 支持以 ':' 分隔的多个目录
    char buf[1024];
    strncpy(buf, env, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
    char *save = NULL; char *tok = strtok_r(buf, ":", &save);
    while (tok) { modyn_autoload_from_dir(tok); tok = strtok_r(NULL, ":", &save); }
    return;
  }
  // 默认目录：工作目录下的 ./plugins
  modyn_autoload_from_dir("./plugins");
  // 以及可执行所在目录及其 plugins 子目录（例如 ./build 或 ./build/plugins）
  char exe_path[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
  if (n > 0) {
    exe_path[n] = '\0';
    // 取目录部分
    char dir_buf[PATH_MAX];
    strncpy(dir_buf, exe_path, sizeof(dir_buf)-1); dir_buf[sizeof(dir_buf)-1] = '\0';
    char *slash = strrchr(dir_buf, '/');
    if (slash) {
      *slash = '\0';
      // 先加载可执行所在目录
      modyn_autoload_from_dir(dir_buf);
      // 再加载其 plugins 子目录
      char sub[PATH_MAX];
      if (snprintf(sub, sizeof(sub), "%s/%s", dir_buf, "plugins") < (int)sizeof(sub)) {
        modyn_autoload_from_dir(sub);
      } else {
        // 路径过长，跳过plugins子目录
        fprintf(stderr, "Warning: Plugin path too long, skipping: %s/plugins\n", dir_buf);
      }
    }
  }
}

// 统一构造器：进程启动时自动加载插件
static void __attribute__((constructor)) modyn_autoload_constructor(void) {
  modyn_autoload_from_env_or_default();
}


