// builtins.S汇编文件里定义
extern char _binary_lib_fs_js_start[];
extern char _binary_lib_fs_js_end[];
extern char _binary_lib_loop_js_start[];
extern char _binary_lib_loop_js_end[];
extern char _binary_lib_path_js_start[];
extern char _binary_lib_path_js_end[];
extern char _binary_lib_process_js_start[];
extern char _binary_lib_process_js_end[];
extern char _binary_lib_build_js_start[];
extern char _binary_lib_build_js_end[];
extern char _binary_lib_repl_js_start[];
extern char _binary_lib_repl_js_end[];
extern char _binary_lib_configure_js_start[];
extern char _binary_lib_configure_js_end[];
extern char _binary_lib_acorn_js_start[];
extern char _binary_lib_acorn_js_end[];
extern char _binary_just_cc_start[];
extern char _binary_just_cc_end[];
extern char _binary_Makefile_start[];
extern char _binary_Makefile_end[];
extern char _binary_main_cc_start[];
extern char _binary_main_cc_end[];
extern char _binary_just_h_start[];
extern char _binary_just_h_end[];
extern char _binary_just_js_start[];
extern char _binary_just_js_end[];
extern char _binary_lib_inspector_js_start[];
extern char _binary_lib_inspector_js_end[];
extern char _binary_lib_websocket_js_start[];
extern char _binary_lib_websocket_js_end[];
extern char _binary_config_js_start[];
extern char _binary_config_js_end[];
// 在modules文件夹里定义
extern "C" {
  extern void* _register_sys();
  extern void* _register_fs();
  extern void* _register_net();
  extern void* _register_vm();
  extern void* _register_epoll();
}
// 建立内置模块的映射关系
void register_builtins() {
  // 建立JS模块到模块代码到映射关系
  just::builtins_add("lib/fs.js", _binary_lib_fs_js_start, _binary_lib_fs_js_end - _binary_lib_fs_js_start);
  just::builtins_add("lib/loop.js", _binary_lib_loop_js_start, _binary_lib_loop_js_end - _binary_lib_loop_js_start);
  just::builtins_add("lib/path.js", _binary_lib_path_js_start, _binary_lib_path_js_end - _binary_lib_path_js_start);
  just::builtins_add("lib/process.js", _binary_lib_process_js_start, _binary_lib_process_js_end - _binary_lib_process_js_start);
  just::builtins_add("lib/build.js", _binary_lib_build_js_start, _binary_lib_build_js_end - _binary_lib_build_js_start);
  just::builtins_add("lib/repl.js", _binary_lib_repl_js_start, _binary_lib_repl_js_end - _binary_lib_repl_js_start);
  just::builtins_add("lib/configure.js", _binary_lib_configure_js_start, _binary_lib_configure_js_end - _binary_lib_configure_js_start);
  just::builtins_add("lib/acorn.js", _binary_lib_acorn_js_start, _binary_lib_acorn_js_end - _binary_lib_acorn_js_start);
  just::builtins_add("just.cc", _binary_just_cc_start, _binary_just_cc_end - _binary_just_cc_start);
  just::builtins_add("Makefile", _binary_Makefile_start, _binary_Makefile_end - _binary_Makefile_start);
  just::builtins_add("main.cc", _binary_main_cc_start, _binary_main_cc_end - _binary_main_cc_start);
  just::builtins_add("just.h", _binary_just_h_start, _binary_just_h_end - _binary_just_h_start);
  just::builtins_add("just.js", _binary_just_js_start, _binary_just_js_end - _binary_just_js_start);
  just::builtins_add("lib/inspector.js", _binary_lib_inspector_js_start, _binary_lib_inspector_js_end - _binary_lib_inspector_js_start);
  just::builtins_add("lib/websocket.js", _binary_lib_websocket_js_start, _binary_lib_websocket_js_end - _binary_lib_websocket_js_start);
  just::builtins_add("config.js", _binary_config_js_start, _binary_config_js_end - _binary_config_js_start);
  // 建立C++到注册函数到映射关系
  just::modules["sys"] = &_register_sys;
  just::modules["fs"] = &_register_fs;
  just::modules["net"] = &_register_net;
  just::modules["vm"] = &_register_vm;
  just::modules["epoll"] = &_register_epoll;
}
static unsigned int just_js_len = _binary_just_js_end - _binary_just_js_start;
static const char* just_js = _binary_just_js_start;
static unsigned int index_js_len = 0;
static const char* index_js = NULL;
static unsigned int _use_index = 0;
static const char* v8flags = "--stack-trace-limit=10 --use-strict --disallow-code-generation-from-strings";
static unsigned int _v8flags_from_commandline = 1;