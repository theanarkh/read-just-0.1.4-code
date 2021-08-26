#include "just.h"

std::map<std::string, just::builtin*> just::builtins;
std::map<std::string, just::register_plugin> just::modules;
uint32_t scriptId = 1;

ssize_t just::process_memory_usage() {
  char buf[1024];
  const char* s = NULL;
  ssize_t n = 0;
  unsigned long val = 0;
  int fd = 0;
  int i = 0;
  do {
    fd = open("/proc/thread-self/stat", O_RDONLY);
  } while (fd == -1 && errno == EINTR);
  if (fd == -1) return (ssize_t)errno;
  do
    n = read(fd, buf, sizeof(buf) - 1);
  while (n == -1 && errno == EINTR);
  close(fd);
  if (n == -1)
    return (ssize_t)errno;
  buf[n] = '\0';
  s = strchr(buf, ' ');
  if (s == NULL)
    goto err;
  s += 1;
  if (*s != '(')
    goto err;
  s = strchr(s, ')');
  if (s == NULL)
    goto err;
  for (i = 1; i <= 22; i++) {
    s = strchr(s + 1, ' ');
    if (s == NULL)
      goto err;
  }
  errno = 0;
  val = strtoul(s, NULL, 10);
  if (errno != 0)
    goto err;
  return val * (unsigned long)getpagesize();
err:
  return 0;
}
// 注册JS模块
void just::builtins_add (const char* name, const char* source, 
  unsigned int size) {
  struct builtin* b = new builtin();
  b->size = size;
  b->source = source;
  builtins[name] = b;
}
// 设置对象的属性，属性为函数
void just::SET_METHOD(Isolate *isolate, Local<ObjectTemplate> 
  recv, const char *name, FunctionCallback callback) {
  recv->Set(String::NewFromUtf8(isolate, name, 
    NewStringType::kNormal).ToLocalChecked(), 
    FunctionTemplate::New(isolate, callback));
}
// 设置对象的属性，属性为对象
void just::SET_MODULE(Isolate *isolate, Local<ObjectTemplate> 
  recv, const char *name, Local<ObjectTemplate> module) {
  recv->Set(String::NewFromUtf8(isolate, name, 
    NewStringType::kNormal).ToLocalChecked(), 
    module);
}
// 设置对象属性，属性为一般值
void just::SET_VALUE(Isolate *isolate, Local<ObjectTemplate> 
  recv, const char *name, Local<Value> value) {
  recv->Set(String::NewFromUtf8(isolate, name, 
    NewStringType::kNormal).ToLocalChecked(), 
    value);
}

// 打印错误栈信息
void just::PrintStackTrace(Isolate* isolate, const TryCatch& try_catch) {
  HandleScope handleScope(isolate);
  Local<Value> exception = try_catch.Exception();
  Local<Message> message = try_catch.Message();
  Local<StackTrace> stack = message->GetStackTrace();
  String::Utf8Value ex(isolate, exception);
  Local<Value> scriptName = message->GetScriptResourceName();
  String::Utf8Value scriptname(isolate, scriptName);
  Local<Context> context = isolate->GetCurrentContext();
  int linenum = message->GetLineNumber(context).FromJust();
  fprintf(stderr, "%s in %s on line %i\n", *ex, *scriptname, linenum);
  if (stack.IsEmpty()) return;
  for (int i = 0; i < stack->GetFrameCount(); i++) {
    Local<StackFrame> stack_frame = stack->GetFrame(isolate, i);
    Local<String> functionName = stack_frame->GetFunctionName();
    Local<String> scriptName = stack_frame->GetScriptName();
    String::Utf8Value fn_name_s(isolate, functionName);
    String::Utf8Value script_name(isolate, scriptName);
    const int line_number = stack_frame->GetLineNumber();
    const int column = stack_frame->GetColumn();
    if (stack_frame->IsEval()) {
      if (stack_frame->GetScriptId() == Message::kNoScriptIdInfo) {
        fprintf(stderr, "    at [eval]:%i:%i\n", line_number, column);
      } else {
        fprintf(stderr, "    at [eval] (%s:%i:%i)\n", *script_name,
          line_number, column);
      }
      break;
    }
    if (fn_name_s.length() == 0) {
      fprintf(stderr, "    at %s:%i:%i\n", *script_name, line_number, column);
    } else {
      fprintf(stderr, "    at %s (%s:%i:%i)\n", *fn_name_s, *script_name,
        line_number, column);
    }
  }
  fflush(stderr);
}

v8::MaybeLocal<v8::Module> just::OnModuleInstantiate(Local<Context> context, 
  Local<String> specifier, Local<Module> referrer) {
  return MaybeLocal<Module>();
}

void just::PromiseRejectCallback(PromiseRejectMessage data) {
  if (data.GetEvent() == v8::kPromiseRejectAfterResolved ||
      data.GetEvent() == v8::kPromiseResolveAfterResolved) {
    // Ignore reject/resolve after resolved.
    return;
  }
  Local<Promise> promise = data.GetPromise();
  Isolate* isolate = promise->GetIsolate();
  if (data.GetEvent() == v8::kPromiseHandlerAddedAfterReject) {
    return;
  }
  Local<Value> exception = data.GetValue();
  v8::Local<Message> message;
  // Assume that all objects are stack-traces.
  if (exception->IsObject()) {
    message = v8::Exception::CreateMessage(isolate, exception);
  }
  if (!exception->IsNativeError() &&
      (message.IsEmpty() || message->GetStackTrace().IsEmpty())) {
    // If there is no real Error object, manually create a stack trace.
    exception = v8::Exception::Error(
        v8::String::NewFromUtf8Literal(isolate, "Unhandled Promise."));
    message = Exception::CreateMessage(isolate, exception);
  }
  Local<Context> context = isolate->GetCurrentContext();
  TryCatch try_catch(isolate);
  Local<Object> globalInstance = context->Global();
  Local<Value> func = globalInstance->Get(context, 
    String::NewFromUtf8Literal(isolate, "onUnhandledRejection", 
      NewStringType::kNormal)).ToLocalChecked();
  if (func.IsEmpty()) {
    return;
  }
  Local<Function> onUnhandledRejection = Local<Function>::Cast(func);
  if (try_catch.HasCaught()) {
    fprintf(stderr, "PromiseRejectCallback: Cast\n");
    return;
  }
  Local<Value> argv[1] = { exception };
  MaybeLocal<Value> result = onUnhandledRejection->Call(context, 
    globalInstance, 1, argv);
  if (result.IsEmpty() && try_catch.HasCaught()) {
    fprintf(stderr, "PromiseRejectCallback: Call\n");
  }
}

void just::FreeMemory(void* buf, size_t length, void* data) {
  free(buf);
}

int just::CreateIsolate(int argc, char** argv, 
  const char* main_src, unsigned int main_len, 
  const char* js, unsigned int js_len, struct iovec* buf, int fd) {
  Isolate::CreateParams create_params;
  int statusCode = 0;
  // 分配ArrayBuffer的内存分配器
  create_params.array_buffer_allocator = 
    ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate *isolate = Isolate::New(create_params);
  {
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    // TODO: make this a config option
    isolate->SetCaptureStackTraceForUncaughtExceptions(true, 1000, 
      StackTrace::kDetailed);
    // 新建一个对象为全局对象
    Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
    // 新建一个对象为核心对象，也是个全局对象
    Local<ObjectTemplate> just = ObjectTemplate::New(isolate);
    // 设置一些属性到just对象
    just::Init(isolate, just);
    // 设置全局属性just
    global->Set(String::NewFromUtf8Literal(isolate, "just", 
      NewStringType::kNormal), just);
    // 新建上下文，并且以global为全局对象
    Local<Context> context = Context::New(isolate, NULL, global);
    Context::Scope context_scope(context);
    // TODO: make this a config option
    // 不允许使用eval和new Function
    context->AllowCodeGenerationFromStrings(false);
    isolate->SetPromiseRejectCallback(PromiseRejectCallback);
    // 处理命令行参数，保存到数组中
    Local<Array> arguments = Array::New(isolate);
    for (int i = 0; i < argc; i++) {
      arguments->Set(context, i, String::NewFromUtf8(isolate, argv[i], 
        NewStringType::kNormal, strlen(argv[i])).ToLocalChecked()).Check();
    }
    Local<Object> globalInstance = context->Global();
    // 设置全局属性global指向全局对象
    globalInstance->Set(context, String::NewFromUtf8Literal(isolate, 
      "global", 
      NewStringType::kNormal), globalInstance).Check();
    // 获取just属性对应的值
    Local<Value> obj = globalInstance->Get(context, 
      String::NewFromUtf8Literal(
        isolate, "just", 
        NewStringType::kNormal)).ToLocalChecked();
    Local<Object> justInstance = Local<Object>::Cast(obj);
    if (buf != NULL) {
      std::unique_ptr<BackingStore> backing = SharedArrayBuffer::NewBackingStore(
          buf->iov_base, buf->iov_len, [](void*, size_t, void*){}, nullptr);
      Local<SharedArrayBuffer> ab = SharedArrayBuffer::New(isolate, std::move(backing));
      justInstance->Set(context, String::NewFromUtf8Literal(isolate, 
        "buffer", NewStringType::kNormal), ab).Check();
    }
    if (fd != 0) {
      justInstance->Set(context, String::NewFromUtf8Literal(isolate, "fd", 
        NewStringType::kNormal), 
        Integer::New(isolate, fd)).Check();
    }
    justInstance->Set(context, String::NewFromUtf8Literal(isolate, "args", 
      NewStringType::kNormal), arguments).Check();
    if (js_len > 0) {
      justInstance->Set(context, String::NewFromUtf8Literal(isolate, 
        "workerSource", NewStringType::kNormal), 
        String::NewFromUtf8(isolate, js, NewStringType::kNormal, 
        js_len).ToLocalChecked()).Check();
    }
    TryCatch try_catch(isolate);

    ScriptOrigin baseorigin(
      String::NewFromUtf8Literal(isolate, "just.js", 
      NewStringType::kInternalized), // resource name
      Integer::New(isolate, 0), // line offset
      Integer::New(isolate, 0),  // column offset
      False(isolate), // is shared cross-origin
      Integer::New(isolate, scriptId++),  // script id
      Local<Value>(), // source map url
      False(isolate), // is opaque
      False(isolate), // is wasm
      False(isolate)  // is module
    );
    Local<Script> script;
    Local<String> base;
    base = String::NewFromUtf8(isolate, main_src, NewStringType::kNormal, 
      main_len).ToLocalChecked();
    ScriptCompiler::Source basescript(base, baseorigin);
    if (!ScriptCompiler::Compile(context, &basescript).ToLocal(&script)) {
      PrintStackTrace(isolate, try_catch);
      return 1;
    }
    // 编译执行just.js，just.js是核心的jS代码
    MaybeLocal<Value> maybe_result = script->Run(context);
    Local<Value> result;
    if (!maybe_result.ToLocal(&result)) {
      just::PrintStackTrace(isolate, try_catch);
      return 2;
    }

    Local<Value> func = globalInstance->Get(context, 
      String::NewFromUtf8Literal(isolate, "onExit", 
        NewStringType::kNormal)).ToLocalChecked();
    if (func->IsFunction()) {
      Local<Function> onExit = Local<Function>::Cast(func);
      Local<Value> argv[1] = {Integer::New(isolate, 0)};
      MaybeLocal<Value> result = onExit->Call(context, globalInstance, 0, argv);
      if (!result.IsEmpty()) {
        statusCode = result.ToLocalChecked()->Uint32Value(context).ToChecked();
      }
      if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
        just::PrintStackTrace(isolate, try_catch);
        return 2;
      }
      statusCode = result.ToLocalChecked()->Uint32Value(context).ToChecked();
    }
  }
  isolate->ContextDisposedNotification();
  isolate->LowMemoryNotification();
  isolate->ClearKeptObjects();
  bool stop = false;
  while(!stop) {
    stop = isolate->IdleNotificationDeadline(1);  
  }
  isolate->Dispose();
  delete create_params.array_buffer_allocator;
  isolate = nullptr;
  return statusCode;
}

int just::CreateIsolate(int argc, char** argv, const char* main_src, 
  unsigned int main_len) {
  return CreateIsolate(argc, argv, main_src, main_len, NULL, 0, NULL, 0);
}
// 打印信息
void just::Print(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  if (args[0].IsEmpty()) return;
  String::Utf8Value str(args.GetIsolate(), args[0]);
  int endline = 1;
  if (args.Length() > 1) {
    endline = static_cast<int>(args[1]->BooleanValue(isolate));
  }
  const char *cstr = *str;
  if (endline == 1) {
    fprintf(stdout, "%s\n", cstr);
  } else {
    fprintf(stdout, "%s", cstr);
  }
}
// 打印错误信息
void just::Error(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  if (args[0].IsEmpty()) return;
  String::Utf8Value str(args.GetIsolate(), args[0]);
  int endline = 1;
  if (args.Length() > 1) {
    endline = static_cast<int>(args[1]->BooleanValue(isolate));
  }
  const char *cstr = *str;
  if (endline == 1) {
    fprintf(stderr, "%s\n", cstr);
  } else {
    fprintf(stderr, "%s", cstr);
  }
}
// 加载C++模块
void just::Load(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  // C++模块导出的信息
  Local<ObjectTemplate> exports = ObjectTemplate::New(isolate);
  // 加载某个模块
  if (args[0]->IsString()) {
    String::Utf8Value name(isolate, args[0]);
    auto iter = just::modules.find(*name);
    if (iter == just::modules.end()) {
      return;
    } else {
      register_plugin _init = (*iter->second);
      auto _register = reinterpret_cast<InitializerCallback>(_init());
      // 执行C++模块提供的注册函数，见C++模块，导出的属性在exports对象中
      _register(isolate, exports);
    }
  } else {
    // 传入的是注册函数的虚拟地址
    Local<BigInt> address64 = Local<BigInt>::Cast(args[0]);
    void* ptr = reinterpret_cast<void*>(address64->Uint64Value());
    register_plugin _init = reinterpret_cast<register_plugin>(ptr);
    auto _register = reinterpret_cast<InitializerCallback>(_init());
    _register(isolate, exports);
  }
  // 返回导出的信息
  args.GetReturnValue().Set(exports->NewInstance(context).ToLocalChecked());
}
// 获取内置JS模块的源码
void just::Builtin(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value name(isolate, args[0]);
  // 获取源码信息
  just::builtin* b = builtins[*name];
  if (b == nullptr) {
    args.GetReturnValue().Set(Null(isolate));
    return;
  }
  if (args.Length() == 1) {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, b->source, 
      NewStringType::kNormal, b->size).ToLocalChecked());
    return;
  }
  // 分配存储源码字符串的内存
  std::unique_ptr<BackingStore> backing = SharedArrayBuffer::NewBackingStore(
      (void*)b->source, b->size, [](void*, size_t, void*){}, nullptr);
  Local<SharedArrayBuffer> ab = SharedArrayBuffer::New(isolate, std::move(backing));

  //Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, (void*)b->source, b->size, v8::ArrayBufferCreationMode::kExternalized);
  args.GetReturnValue().Set(ab);
}
// 阻塞式睡眠
void just::Sleep(const FunctionCallbackInfo<Value> &args) {
  sleep(Local<Integer>::Cast(args[0])->Value());
}

void just::MemoryUsage(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  ssize_t rss = just::process_memory_usage();
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);
  Local<BigUint64Array> array;
  Local<ArrayBuffer> ab;
  // 获取保存数据的内存
  if (args.Length() > 0) {
    array = args[0].As<BigUint64Array>();
    ab = array->Buffer();
  } else {
    ab = ArrayBuffer::New(isolate, 16 * 8);
    array = BigUint64Array::New(ab, 0, 16);
  }
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  uint64_t *fields = static_cast<uint64_t *>(backing->Data());
  // 进程内存信息
  fields[0] = rss;
  // 进程堆内存信息
  fields[1] = v8_heap_stats.total_heap_size();
  fields[2] = v8_heap_stats.used_heap_size();
  fields[3] = v8_heap_stats.external_memory();
  fields[4] = v8_heap_stats.does_zap_garbage();
  fields[5] = v8_heap_stats.heap_size_limit();
  fields[6] = v8_heap_stats.malloced_memory();
  fields[7] = v8_heap_stats.number_of_detached_contexts();
  fields[8] = v8_heap_stats.number_of_native_contexts();
  fields[9] = v8_heap_stats.peak_malloced_memory();
  fields[10] = v8_heap_stats.total_available_size();
  fields[11] = v8_heap_stats.total_heap_size_executable();
  fields[12] = v8_heap_stats.total_physical_size();
  // 堆外内存的大小
  fields[13] = isolate->AdjustAmountOfExternalAllocatedMemory(0);
  args.GetReturnValue().Set(array);
}
// 退出进程
void just::Exit(const FunctionCallbackInfo<Value>& args) {
  exit(Local<Integer>::Cast(args[0])->Value());
}
// 获取进程id
void just::PID(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), getpid()));
}
// 修改进程工作目录
void just::Chdir(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value path(isolate, args[0]);
  args.GetReturnValue().Set(Integer::New(isolate, chdir(*path)));
}

void just::Init(Isolate* isolate, Local<ObjectTemplate> target) {
  // 新建一个对象
  Local<ObjectTemplate> version = ObjectTemplate::New(isolate);
  // 设置对象的属性
  SET_VALUE(isolate, version, "just", String::NewFromUtf8Literal(isolate, 
    JUST_VERSION));
  SET_VALUE(isolate, version, "v8", String::NewFromUtf8(isolate, 
    v8::V8::GetVersion()).ToLocalChecked());
  // 新建一个对象
  Local<ObjectTemplate> kernel = ObjectTemplate::New(isolate);
  utsname kernel_rec;
  // 获取系统信息
  int rc = uname(&kernel_rec);
  // 设置对象属性
  if (rc == 0) {
    kernel->Set(String::NewFromUtf8Literal(isolate, "os", 
      NewStringType::kNormal), String::NewFromUtf8(isolate, 
      kernel_rec.sysname).ToLocalChecked());
    kernel->Set(String::NewFromUtf8Literal(isolate, "release", 
      NewStringType::kNormal), String::NewFromUtf8(isolate, 
      kernel_rec.release).ToLocalChecked());
    kernel->Set(String::NewFromUtf8Literal(isolate, "version", 
      NewStringType::kNormal), String::NewFromUtf8(isolate, 
      kernel_rec.version).ToLocalChecked());
  }
  version->Set(String::NewFromUtf8Literal(isolate, "kernel", 
    NewStringType::kNormal), kernel);
  SET_METHOD(isolate, target, "print", Print);
  SET_METHOD(isolate, target, "error", Error);
  SET_METHOD(isolate, target, "load", Load);
  SET_METHOD(isolate, target, "exit", Exit);
  SET_METHOD(isolate, target, "pid", PID);
  SET_METHOD(isolate, target, "chdir", Chdir);
  SET_METHOD(isolate, target, "sleep", Sleep);
  SET_METHOD(isolate, target, "builtin", Builtin);
  SET_METHOD(isolate, target, "memoryUsage", MemoryUsage);
  SET_MODULE(isolate, target, "version", version);
}
