#include "sys.h"

uint64_t just::sys::hrtime() {
  struct timespec t;
  clock_t clock_id = CLOCK_MONOTONIC;
  if (clock_gettime(clock_id, &t)) return 0;
  return t.tv_sec * (uint64_t) 1e9 + t.tv_nsec;
}

void just::sys::WaitPID(const FunctionCallbackInfo<Value> &args) {
  Local<ArrayBuffer> ab = args[0].As<Int32Array>()->Buffer();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  int *fields = static_cast<int *>(backing->Data());
  int pid = -1;
  if (args.Length() > 1) pid = Local<Integer>::Cast(args[1])->Value();
  int flags = WNOHANG;
  if (args.Length() > 2) flags = Local<Integer>::Cast(args[2])->Value();
  fields[1] = waitpid(pid, &fields[0], flags);
  fields[0] = WEXITSTATUS(fields[0]); 
  args.GetReturnValue().Set(args[0]);
}

void just::sys::Exec(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  String::Utf8Value filePath(isolate, args[0]);
  Local<Array> arguments = args[1].As<Array>();
  int len = arguments->Length();
  char** argv = (char**)calloc(len + 2, sizeof(char*));
  argv[0] = (char*)calloc(1, filePath.length());
  memcpy(argv[0], *filePath, filePath.length());
  Local<Context> context = isolate->GetCurrentContext();
  int written = 0;
  for (int i = 0; i < len; i++) {
    Local<String> val = 
      arguments->Get(context, i).ToLocalChecked().As<v8::String>();
    argv[i + 1] = (char*)calloc(1, val->Length() + 1);
    val->WriteUtf8(isolate, argv[i + 1], val->Length() + 1, &written, 
      v8::String::HINT_MANY_WRITES_EXPECTED);
  }
  argv[len + 1] = NULL;
  args.GetReturnValue().Set(Integer::New(isolate, execvp(*filePath, argv)));
}

void just::sys::Setenv(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value name(isolate, args[0]);
  String::Utf8Value value(isolate, args[1]);
  args.GetReturnValue().Set(Integer::New(isolate, setenv(*name, *value, 1)));
}

void just::sys::Getenv(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value name(isolate, args[0]);
  char* value = getenv(*name);
  if (value == NULL) return;
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, 
    value, v8::NewStringType::kNormal, 
    strnlen(value, 1024)).ToLocalChecked());
}

void just::sys::Unsetenv(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value name(isolate, args[0]);
  args.GetReturnValue().Set(Integer::New(isolate, unsetenv(*name)));
}

void just::sys::Spawn(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  String::Utf8Value filePath(isolate, args[0]);
  String::Utf8Value cwd(isolate, args[1]);
  Local<Array> arguments = args[2].As<Array>();
  int fds[6];
  fds[0] = Local<Integer>::Cast(args[3])->Value(); // STDIN READ
  fds[1] = Local<Integer>::Cast(args[4])->Value(); // STDOUT WRITE
  fds[2] = Local<Integer>::Cast(args[5])->Value(); // STDERR WRITE
  int len = arguments->Length();
  char** argv = (char**)calloc(len + 2, sizeof(char*));
  int written = 0;
  argv[0] = (char*)calloc(1, filePath.length());
  memcpy(argv[0], *filePath, filePath.length());
  for (int i = 0; i < len; i++) {
    Local<String> val = 
      arguments->Get(context, i).ToLocalChecked().As<v8::String>();
    argv[i + 1] = (char*)calloc(1, val->Length() + 1);
    val->WriteUtf8(isolate, argv[i + 1], val->Length() + 1, &written, 
      v8::String::HINT_MANY_WRITES_EXPECTED);
  }
  argv[len + 1] = NULL;
  pid_t pid = fork();
  if (pid == -1) {
    for (int i = 0; i < len; i++) free(argv[i]);
    free(argv);
    args.GetReturnValue().Set(Integer::New(isolate, pid));
    return;
  }
  if (pid == 0) {
    int r = chdir(*cwd);
    if (r < 0) exit(127);
    // copy the pipes from the parent process into stdio fds
    dup2(fds[0], STDIN_FILENO);
    dup2(fds[1], STDOUT_FILENO);
    dup2(fds[2], STDERR_FILENO);
    // todo: use execve and pass environment
    execvp(*filePath, argv);
    for (int i = 0; i < len; i++) free(argv[i]);
    free(argv);
    exit(127);
    return;
  }
  args.GetReturnValue().Set(Integer::New(isolate, pid));
  for (int i = 0; i < len; i++) free(argv[i]);
  free(argv);
}

void just::sys::HRTime(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(BigInt::New(args.GetIsolate(), just::sys::hrtime()));
}

void just::sys::RunMicroTasks(const FunctionCallbackInfo<Value> &args) {
  args.GetIsolate()->PerformMicrotaskCheckpoint();
}

void just::sys::EnqueueMicrotask(const FunctionCallbackInfo<Value>& args) {
  args.GetIsolate()->EnqueueMicrotask(args[0].As<Function>());
}

void just::sys::Exit(const FunctionCallbackInfo<Value>& args) {
  exit(Local<Integer>::Cast(args[0])->Value());
}

void just::sys::Fork(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), fork()));
}

void just::sys::Kill(const FunctionCallbackInfo<Value>& args) {
  int pid = Local<Integer>::Cast(args[0])->Value();
  int signum = Local<Integer>::Cast(args[1])->Value();
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), kill(pid, signum)));
}

void just::sys::Getchar(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), getchar()));
}

void just::sys::Putchar(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
    putchar(Local<Integer>::Cast(args[0])->Value())));
}

void just::sys::CPUUsage(const FunctionCallbackInfo<Value> &args) {
  struct tms stat;
  clock_t c = times(&stat);
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), c));
  if (c == -1) return;
  Local<Uint32Array> b32 = args[0].As<Uint32Array>();
  Local<ArrayBuffer> ab = b32->Buffer();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  uint32_t *fields = static_cast<uint32_t *>(backing->Data());
  fields[0] = stat.tms_utime;
  fields[1] = stat.tms_stime;
  fields[2] = stat.tms_cutime;
  fields[3] = stat.tms_cstime;
}

void just::sys::GetrUsage(const FunctionCallbackInfo<Value> &args) {
  struct rusage usage;
  int type = RUSAGE_SELF;
  if (args.Length() > 0) type = Local<Integer>::Cast(args[1])->Value();
  getrusage(type, &usage);
  Local<Float64Array> array = args[0].As<Float64Array>();
  Local<ArrayBuffer> ab = array->Buffer();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  double *fields = static_cast<double *>(backing->Data());
  fields[0] = (JUST_MICROS_PER_SEC * usage.ru_utime.tv_sec) 
    + usage.ru_utime.tv_usec;
  fields[1] = (JUST_MICROS_PER_SEC * usage.ru_stime.tv_sec) 
    + usage.ru_stime.tv_usec;
  fields[2] = usage.ru_maxrss;
  fields[3] = usage.ru_ixrss;
  fields[4] = usage.ru_idrss;
  fields[5] = usage.ru_isrss;
  fields[6] = usage.ru_minflt;
  fields[7] = usage.ru_majflt;
  fields[8] = usage.ru_nswap;
  fields[9] = usage.ru_inblock;
  fields[10] = usage.ru_oublock;
  fields[11] = usage.ru_msgsnd;
  fields[12] = usage.ru_msgrcv;
  fields[13] = usage.ru_nsignals;
  fields[14] = usage.ru_nvcsw;
  fields[15] = usage.ru_nivcsw;
}

void just::sys::PID(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), getpid()));
}

void just::sys::TID(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
    syscall(SYS_gettid)));
}

void just::sys::GetPgrp(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), getpgrp()));
}

void just::sys::SetPgid(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
    setpgid(Local<Integer>::Cast(args[0])->Value(), 
    Local<Integer>::Cast(args[1])->Value())));
}

void just::sys::PPID(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), getppid()));
}

void just::sys::GetSid(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
    getsid(Local<Integer>::Cast(args[0])->Value())));
}

void just::sys::SetSid(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), setsid()));
}

void just::sys::GetUid(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), getuid()));
}

void just::sys::SetUid(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
    setuid(Local<Integer>::Cast(args[0])->Value())));
}

void just::sys::GetGid(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), getgid()));
}

void just::sys::SetGid(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
    setgid(Local<Integer>::Cast(args[0])->Value())));
}

void just::sys::Errno(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), errno));
}

void just::sys::StrError(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(String::NewFromUtf8(args.GetIsolate(), 
    strerror(Local<Integer>::Cast(args[0])->Value())).ToLocalChecked());
}

void just::sys::Sleep(const FunctionCallbackInfo<Value> &args) {
  sleep(Local<Integer>::Cast(args[0])->Value());
}

void just::sys::USleep(const FunctionCallbackInfo<Value> &args) {
  usleep(Local<Integer>::Cast(args[0])->Value());
}

void just::sys::NanoSleep(const FunctionCallbackInfo<Value> &args) {
  struct timespec sleepfor;
  sleepfor.tv_sec = Local<Integer>::Cast(args[0])->Value();
  sleepfor.tv_nsec = Local<Integer>::Cast(args[1])->Value();
  nanosleep(&sleepfor, NULL);
}

void just::sys::SharedMemoryUsage(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  v8::SharedMemoryStatistics sm_stats;
  v8::V8::GetSharedMemoryStatistics(&sm_stats);
  Local<Object> o = Object::New(isolate);
  o->Set(context, String::NewFromUtf8Literal(isolate, "readOnlySpaceSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, sm_stats.read_only_space_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "readOnlySpaceUsedSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, sm_stats.read_only_space_used_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, 
    "readOnlySpacePhysicalSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, sm_stats.read_only_space_physical_size())).Check();
  args.GetReturnValue().Set(o);
}

void just::sys::HeapObjectStatistics(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  v8::HeapObjectStatistics obj_stats;
  size_t num_types = isolate->NumberOfTrackedHeapObjectTypes();
  Local<Object> res = Object::New(isolate);
  for (size_t i = 0; i < num_types; i++) {
    bool ok = isolate->GetHeapObjectStatisticsAtLastGC(&obj_stats, i);
    if (ok) {
      Local<Context> context = isolate->GetCurrentContext();
      Local<Object> o = Object::New(isolate);
      o->Set(context, String::NewFromUtf8Literal(isolate, "subType", 
        NewStringType::kInternalized), 
        String::NewFromUtf8(isolate, obj_stats.object_sub_type(), 
        NewStringType::kInternalized).ToLocalChecked()).Check();
      o->Set(context, String::NewFromUtf8Literal(isolate, "count", 
        NewStringType::kInternalized), 
        Integer::New(isolate, obj_stats.object_count())).Check();
      o->Set(context, String::NewFromUtf8Literal(isolate, "size", 
        NewStringType::kInternalized), 
        Integer::New(isolate, obj_stats.object_size())).Check();
      res->Set(context, 
        String::NewFromUtf8(isolate, obj_stats.object_type(), 
        NewStringType::kInternalized).ToLocalChecked(),
        o
      ).Check();
    }
  }
  args.GetReturnValue().Set(res);
}

void just::sys::HeapCodeStatistics(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  v8::HeapCodeStatistics code_stats;
  isolate->GetHeapCodeAndMetadataStatistics(&code_stats);
  Local<Object> o = Object::New(isolate);
  o->Set(context, String::NewFromUtf8Literal(isolate, "codeAndMetadataSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, code_stats.code_and_metadata_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "bytecodeAndMetadataSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, code_stats.bytecode_and_metadata_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "externalScriptSourceSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, code_stats.external_script_source_size())).Check();
  args.GetReturnValue().Set(o);
}

void just::sys::HeapSpaceUsage(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  HeapSpaceStatistics s;
  size_t number_of_heap_spaces = isolate->NumberOfHeapSpaces();
  Local<Array> spaces = args[0].As<Array>();
  Local<Object> o = Object::New(isolate);
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);
  Local<Object> heaps = Object::New(isolate);
  o->Set(context, String::NewFromUtf8Literal(isolate, "totalHeapSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.total_heap_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "totalPhysicalSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.total_physical_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "usedHeapSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.used_heap_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "totalAvailableSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.total_available_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "heapSizeLimit", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.heap_size_limit())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "totalHeapSizeExecutable", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.total_heap_size_executable())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "totalGlobalHandlesSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.total_global_handles_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "usedGlobalHandlesSize", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.used_global_handles_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "mallocedMemory", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.malloced_memory())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "externalMemory", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.external_memory())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "peakMallocedMemory", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.peak_malloced_memory())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "nativeContexts", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.number_of_native_contexts())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "detachedContexts", 
    NewStringType::kInternalized), 
    Integer::New(isolate, v8_heap_stats.number_of_detached_contexts())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "heapSpaces", 
    NewStringType::kInternalized), heaps).Check();
  for (size_t i = 0; i < number_of_heap_spaces; i++) {
    isolate->GetHeapSpaceStatistics(&s, i);
    Local<Float64Array> array = spaces->Get(context, i)
      .ToLocalChecked().As<Float64Array>();
    Local<ArrayBuffer> ab = array->Buffer();
    std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
    double *fields = static_cast<double *>(backing->Data());
    fields[0] = s.physical_space_size();
    fields[1] = s.space_available_size();
    fields[2] = s.space_size();
    fields[3] = s.space_used_size();
    heaps->Set(context, String::NewFromUtf8(isolate, s.space_name(), 
      NewStringType::kInternalized).ToLocalChecked(), array).Check();
  }
  args.GetReturnValue().Set(o);
}

void just::sys::Memcpy(const FunctionCallbackInfo<Value> &args) {
  std::shared_ptr<BackingStore> bdest = 
    args[0].As<ArrayBuffer>()->GetBackingStore();
  char *dest = static_cast<char *>(bdest->Data());
  std::shared_ptr<BackingStore> bsource = 
    args[1].As<ArrayBuffer>()->GetBackingStore();
  char *source = static_cast<char *>(bsource->Data());
  int slen = bsource->ByteLength();
  int argc = args.Length();
  int off = 0;
  if (argc > 2) off = Local<Integer>::Cast(args[2])->Value();
  int len = slen;
  if (argc > 3) len = Local<Integer>::Cast(args[3])->Value();
  if (len == 0) return;
  int off2 = 0;
  if (argc > 4) off2 = Local<Integer>::Cast(args[4])->Value();
  dest = dest + off;
  source = source + off2;
  memcpy(dest, source, len);
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), len));
}

void just::sys::Utf8Length(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  args.GetReturnValue().Set(Integer::New(isolate, 
    args[0].As<String>()->Utf8Length(isolate)));
}

// todo: aligned_alloc
void just::sys::Calloc(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  size_t count = Local<Integer>::Cast(args[0])->Value();
  size_t size = 0;
  void* chunk;
  if (args[1]->IsString()) {
    Local<String> str = args[1].As<String>();
    size = str->Utf8Length(isolate);
    chunk = calloc(count, size);
    if (chunk == NULL) return;
    int written;
    char* next = (char*)chunk;
    for (uint32_t i = 0; i < count; i++) {
      str->WriteUtf8(isolate, next, size, &written, 
        String::HINT_MANY_WRITES_EXPECTED | String::NO_NULL_TERMINATION);
      next += written;
    }
  } else {
    if (args[1]->IsBigInt()) {
      size = Local<BigInt>::Cast(args[1])->Uint64Value();
    } else {
      size = Local<Integer>::Cast(args[1])->Value();
    }
    chunk = calloc(count, size);
    if (chunk == NULL) return;
  }
  bool shared = false;
  if (args.Length() > 2) shared = args[2]->BooleanValue(isolate);
  std::unique_ptr<BackingStore> backing;
  if (shared) {
    backing = SharedArrayBuffer::NewBackingStore(chunk, count * size, 
          just::FreeMemory, nullptr);
    Local<SharedArrayBuffer> ab =
        SharedArrayBuffer::New(isolate, std::move(backing));
    args.GetReturnValue().Set(ab);
  } else {
    backing = ArrayBuffer::NewBackingStore(chunk, count * size, 
        just::FreeMemory, nullptr);
    Local<ArrayBuffer> ab =
        ArrayBuffer::New(isolate, std::move(backing));
    args.GetReturnValue().Set(ab);
  }
}

void just::sys::ReadString(const FunctionCallbackInfo<Value> &args) {
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  char *data = static_cast<char *>(backing->Data());
  int argc = args.Length();
  int len = 0;
  if (argc > 1) {
    len = Local<Integer>::Cast(args[1])->Value();
  } else {
    len = backing->ByteLength();
  }
  int off = 0;
  if (argc > 2) off = Local<Integer>::Cast(args[2])->Value();
  char* source = data + off;
  args.GetReturnValue().Set(String::NewFromUtf8(args.GetIsolate(), source, 
    NewStringType::kNormal, len).ToLocalChecked());
}

void just::sys::GetAddress(const FunctionCallbackInfo<Value> &args) {
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  char *data = static_cast<char *>(backing->Data());
  args.GetReturnValue().Set(BigInt::New(args.GetIsolate(), (uint64_t)data));
}

void just::sys::WriteString(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  Local<String> str = args[1].As<String>();
  int off = 0;
  if (args.Length() > 2) off = Local<Integer>::Cast(args[2])->Value();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  char *data = static_cast<char *>(backing->Data());
  char* source = data + off;
  int len = str->Utf8Length(isolate);
  int nchars = 0;
  int written = str->WriteUtf8(isolate, source, len, &nchars, 
    v8::String::HINT_MANY_WRITES_EXPECTED | v8::String::NO_NULL_TERMINATION);
  args.GetReturnValue().Set(Integer::New(isolate, written));
}

void just::sys::WriteCString(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  uint8_t* data = static_cast<uint8_t*>(backing->Data());
  Local<String> str = args[1].As<String>();
  int len = str->Length();
  int off = 0;
  if (args.Length() > 2) off = Local<Integer>::Cast(args[2])->Value();
  args.GetReturnValue().Set(Integer::New(isolate, str->WriteOneByte(isolate, 
    (uint8_t*)data, off, len, v8::String::HINT_MANY_WRITES_EXPECTED)));
}

void just::sys::Fcntl(const FunctionCallbackInfo<Value> &args) {
  int fd = Local<Integer>::Cast(args[0])->Value();
  int flags = Local<Integer>::Cast(args[1])->Value();
  if (args.Length() > 2) {
    int val = Local<Integer>::Cast(args[2])->Value();
    args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
      fcntl(fd, flags, val)));
    return;
  }
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), fcntl(fd, flags)));
}

void just::sys::Cwd(const FunctionCallbackInfo<Value> &args) {
  char cwd[PATH_MAX];
  args.GetReturnValue().Set(String::NewFromUtf8(args.GetIsolate(), 
    getcwd(cwd, PATH_MAX), NewStringType::kNormal).ToLocalChecked());
}

void just::sys::Env(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  int size = 0;
  while (environ[size]) size++;
  Local<Array> envarr = Array::New(isolate);
  for (int i = 0; i < size; ++i) {
    const char *var = environ[i];
    envarr->Set(context, i, String::NewFromUtf8(isolate, var, 
      NewStringType::kNormal, strlen(var)).ToLocalChecked()).Check();
  }
  args.GetReturnValue().Set(envarr);
}

void just::sys::Timer(const FunctionCallbackInfo<Value> &args) {
  int t1 = Local<Integer>::Cast(args[0])->Value();
  int t2 = Local<Integer>::Cast(args[1])->Value();
  int argc = args.Length();
  clockid_t cid = CLOCK_MONOTONIC;
  if (argc > 2) {
    cid = (clockid_t)Local<Integer>::Cast(args[2])->Value();
  }
  int flags = TFD_NONBLOCK | TFD_CLOEXEC;
  if (argc > 3) flags = Local<Integer>::Cast(args[3])->Value();
  int fd = timerfd_create(cid, flags);
  if (fd == -1) {
    args.GetReturnValue().Set(Integer::New(args.GetIsolate(), fd));
    return;
  }
  struct itimerspec ts;
  ts.it_interval.tv_sec = t1 / 1000;
	ts.it_interval.tv_nsec = (t1 % 1000) * 1000000;
	ts.it_value.tv_sec = t2 / 1000;
	ts.it_value.tv_nsec = (t2 % 1000) * 1000000;  
  int r = timerfd_settime(fd, 0, &ts, NULL);
  if (r == -1) {
    args.GetReturnValue().Set(Integer::New(args.GetIsolate(), r));
    return;
  }
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), fd));
}

void just::sys::AvailablePages(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
    sysconf(_SC_AVPHYS_PAGES)));
}

void just::sys::WritePointer(const FunctionCallbackInfo<Value> &args) {
  Local<ArrayBuffer> abdest = args[0].As<ArrayBuffer>();
  uint8_t* dest = static_cast<uint8_t*>(abdest->GetBackingStore()->Data());
  Local<ArrayBuffer> absrc = args[1].As<ArrayBuffer>();
  uint8_t* src = static_cast<uint8_t*>(absrc->GetBackingStore()->Data());
  int off = Local<Integer>::Cast(args[1])->Value();
  uint8_t* ptr = dest + off;
  *reinterpret_cast<void **>(ptr) = src;
}

void just::sys::ReadMemory(const FunctionCallbackInfo<Value> &args) {
  Local<BigInt> start64 = Local<BigInt>::Cast(args[0]);
  Local<BigInt> end64 = Local<BigInt>::Cast(args[1]);
  const uint64_t size = end64->Uint64Value() - start64->Uint64Value();
  void* start = reinterpret_cast<void*>(start64->Uint64Value());
  std::unique_ptr<BackingStore> backing = ArrayBuffer::NewBackingStore(
      start, size, [](void*, size_t, void*){}, nullptr);
  args.GetReturnValue().Set(ArrayBuffer::New(args.GetIsolate(), std::move(backing)));
}

void just::sys::ShmOpen(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value name(isolate, args[0]);
  int argc = args.Length();
  int flags = O_RDWR | O_CREAT;
  int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  if (argc > 1) flags = Local<Integer>::Cast(args[1])->Value();
  if (argc > 2) mode = Local<Integer>::Cast(args[2])->Value();
  args.GetReturnValue().Set(Integer::New(isolate, 
    shm_open(*name, flags, mode)));
}

void just::sys::ShmUnlink(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value name(isolate, args[0]);
  args.GetReturnValue().Set(Integer::New(isolate, shm_unlink(*name)));
}

void just::sys::MMap(const FunctionCallbackInfo<Value> &args) {
  // TODO: if private, then don't use sharedarraybuffer
  int fd = Local<Integer>::Cast(args[0])->Value();
  int len = Local<Integer>::Cast(args[1])->Value();
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_SHARED;
  size_t offset = 0;
  int argc = args.Length();
  if (argc > 2) prot = Local<Integer>::Cast(args[2])->Value();
  if (argc > 3) flags = Local<Integer>::Cast(args[3])->Value();
  if (argc > 4) offset = Local<Integer>::Cast(args[4])->Value();
  void* data = mmap(0, len, prot, flags, fd, offset);
  if (data == MAP_FAILED) return;
  std::unique_ptr<BackingStore> backing = SharedArrayBuffer::NewBackingStore(
      data, len, [](void*, size_t, void*){}, nullptr);
  args.GetReturnValue().Set(SharedArrayBuffer::New(args.GetIsolate(), 
    std::move(backing)));
}

void just::sys::MUnmap(const FunctionCallbackInfo<Value> &args) {
  Local<SharedArrayBuffer> ab = args[0].As<SharedArrayBuffer>();
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
    munmap(ab->GetBackingStore()->Data(), 
    Local<Integer>::Cast(args[1])->Value())));
}

void just::sys::Ioctl(const FunctionCallbackInfo<Value> &args) {
  int fd = Local<Integer>::Cast(args[0])->Value();
  int flags = Local<Integer>::Cast(args[1])->Value();
  if (args.Length() > 2) {
    if (args[2]->IsArrayBuffer()) {
      Local<ArrayBuffer> buf = args[2].As<ArrayBuffer>();
      args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
        ioctl(fd, flags, buf->GetBackingStore()->Data())));
      return;
    }
    if (args[2]->IsNumber()) {
      args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
        ioctl(fd, flags, Local<Integer>::Cast(args[2])->Value())));
      return;
    }
  }
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), ioctl(fd, flags)));
}

void just::sys::Reboot(const FunctionCallbackInfo<Value> &args) {
  int flags = RB_POWER_OFF;
  if (args.Length() > 0) flags = Local<Integer>::Cast(args[0])->Value();
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), reboot(flags)));
}

#ifndef STATIC
void just::sys::DLOpen(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  int mode = RTLD_LAZY;
  void* handle;
  int argc = args.Length();
  if (argc > 1) mode = Local<Integer>::Cast(args[1])->Value();
  if (argc > 0) {
    String::Utf8Value path(isolate, args[0]);
    handle = dlopen(*path, mode);
  } else {
    handle = dlopen(NULL, mode);
  }
  if (handle == NULL) {
    args.GetReturnValue().Set(v8::Null(isolate));
    return;
  }
  args.GetReturnValue().Set(BigInt::New(isolate, (uint64_t)handle));
}

void just::sys::DLSym(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<BigInt> address64 = Local<BigInt>::Cast(args[0]);
  String::Utf8Value name(isolate, args[1]);
  void* handle = reinterpret_cast<void*>(address64->Uint64Value());
  void* ptr = dlsym(handle, *name);
  if (ptr == NULL) {
    args.GetReturnValue().Set(v8::Null(isolate));
    return;
  }
  args.GetReturnValue().Set(BigInt::New(isolate, (uint64_t)ptr));
}

void just::sys::DLError(const FunctionCallbackInfo<Value> &args) {
  char* err = dlerror();
  if (err == NULL) {
    args.GetReturnValue().Set(v8::Null(args.GetIsolate()));
    return;
  }
  args.GetReturnValue().Set(String::NewFromUtf8(args.GetIsolate(), err, 
    NewStringType::kNormal).ToLocalChecked());
}

void just::sys::DLClose(const FunctionCallbackInfo<Value> &args) {
  Local<BigInt> address64 = Local<BigInt>::Cast(args[0]);
  void* handle = reinterpret_cast<void*>(address64->Uint64Value());
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), dlclose(handle)));
}
#endif

void just::sys::FExecVE(const FunctionCallbackInfo<Value> &args) {
  Isolate* isolate = args.GetIsolate();
  int fd = Local<Integer>::Cast(args[0])->Value();
  Local<Array> arguments = args[1].As<Array>();
  int len = arguments->Length();
  char** argv = (char**)calloc(len + 1, sizeof(char*));
  Local<Context> context = isolate->GetCurrentContext();
  int written = 0;
  for (int i = 0; i < len; i++) {
    Local<String> val = 
      arguments->Get(context, i).ToLocalChecked().As<v8::String>();
    argv[i] = (char*)calloc(1, val->Length() + 1);
    val->WriteUtf8(isolate, argv[i], val->Length() + 1, &written, 
      v8::String::HINT_MANY_WRITES_EXPECTED);
  }
  argv[len] = NULL;
  Local<Array> env = args[2].As<Array>();
  len = env->Length();
  char** envv = (char**)calloc(len + 1, sizeof(char*));
  for (int i = 0; i < len; i++) {
    Local<String> val = 
      env->Get(context, i).ToLocalChecked().As<v8::String>();
    envv[i] = (char*)calloc(1, val->Length() + 1);
    val->WriteUtf8(isolate, envv[i], val->Length() + 1, &written, 
      v8::String::HINT_MANY_WRITES_EXPECTED);
  }
  envv[len] = NULL;
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
    fexecve(fd, argv, envv)));
}

void just::sys::SetTerminalFlags(const FunctionCallbackInfo<Value> &args) {
  int fd = Local<Integer>::Cast(args[0])->Value();
  int flags = Local<Integer>::Cast(args[1])->Value();
  int action = TCSAFLUSH;
  if (args.Length() > 2) action = Local<Integer>::Cast(args[2])->Value();
  struct termios orig_termios;
  tcgetattr(fd, &orig_termios);
  struct termios raw = orig_termios;
  raw.c_lflag = flags;
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), 
    tcsetattr(fd, action, &raw)));
}

void just::sys::GetTerminalFlags(const FunctionCallbackInfo<Value> &args) {
  struct termios orig;
  tcgetattr(Local<Integer>::Cast(args[0])->Value(), &orig);
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), orig.c_lflag));
}

void just::sys::MemFdCreate(const FunctionCallbackInfo<Value> &args) {
  Isolate* isolate = args.GetIsolate();
  v8::String::Utf8Value fname(isolate, args[0]);
  args.GetReturnValue().Set(Integer::New(isolate, memfd_create(*fname, 
    Local<Integer>::Cast(args[1])->Value())));
}

void just::sys::BufferInfo(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<Object> meta = args[1].As<Object>();
  bool isExternal = false;
  bool isDetachable = false;
  bool isShared = false;
  if (args[0]->IsArrayBuffer()) {
    Local<ArrayBuffer> buf = args[0].As<ArrayBuffer>();
    isExternal = buf->IsExternal();
    isDetachable = buf->IsDetachable();
  } else if (args[0]->IsSharedArrayBuffer()) {
    Local<SharedArrayBuffer> buf = args[0].As<SharedArrayBuffer>();
    isExternal = buf->IsExternal();
    isShared = true;
  }
  meta->Set(context, String::NewFromUtf8Literal(isolate, "isExternal", 
    NewStringType::kInternalized), 
    v8::Boolean::New(isolate, isExternal)).Check();
  meta->Set(context, String::NewFromUtf8Literal(isolate, "isDetachable", 
    NewStringType::kInternalized), 
    v8::Boolean::New(isolate, isDetachable)).Check();
  meta->Set(context, String::NewFromUtf8Literal(isolate, "isShared", 
    NewStringType::kInternalized), 
    v8::Boolean::New(isolate, isShared)).Check();
  args.GetReturnValue().Set(meta);
}

void just::sys::Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> sys = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, sys, "getuid", GetUid);
  SET_METHOD(isolate, sys, "setuid", SetUid);
  SET_METHOD(isolate, sys, "getgid", GetGid);
  SET_METHOD(isolate, sys, "setgid", SetGid);
  SET_METHOD(isolate, sys, "calloc", Calloc);
  SET_METHOD(isolate, sys, "setTerminalFlags", SetTerminalFlags);
  SET_METHOD(isolate, sys, "getTerminalFlags", GetTerminalFlags);
  SET_METHOD(isolate, sys, "shmopen", ShmOpen);
  SET_METHOD(isolate, sys, "shmunlink", ShmUnlink);
  SET_METHOD(isolate, sys, "fexecve", FExecVE);
  SET_METHOD(isolate, sys, "readString", ReadString);
  SET_METHOD(isolate, sys, "writeString", WriteString);
  SET_METHOD(isolate, sys, "writeCString", WriteCString);  
  SET_METHOD(isolate, sys, "getAddress", GetAddress);
  SET_METHOD(isolate, sys, "fcntl", Fcntl);
  SET_METHOD(isolate, sys, "memcpy", Memcpy);
  SET_METHOD(isolate, sys, "sleep", Sleep);
  SET_METHOD(isolate, sys, "utf8Length", Utf8Length);
  SET_METHOD(isolate, sys, "readMemory", ReadMemory);
  SET_METHOD(isolate, sys, "writePointer", WritePointer);
  SET_METHOD(isolate, sys, "bufferInfo", BufferInfo);
  SET_METHOD(isolate, sys, "timer", Timer);
  SET_METHOD(isolate, sys, "heapUsage", HeapSpaceUsage);
  SET_METHOD(isolate, sys, "sharedMemoryUsage", SharedMemoryUsage);
  SET_METHOD(isolate, sys, "heapObjectStatistics", HeapObjectStatistics);
  SET_METHOD(isolate, sys, "heapCodeStatistics", HeapCodeStatistics);
  SET_METHOD(isolate, sys, "memfdCreate", MemFdCreate);
  SET_METHOD(isolate, sys, "pid", PID);
  SET_METHOD(isolate, sys, "ppid", PPID);
  SET_METHOD(isolate, sys, "setpgid", SetPgid);
  SET_METHOD(isolate, sys, "getpgrp", GetPgrp);
  SET_METHOD(isolate, sys, "tid", TID);
  SET_METHOD(isolate, sys, "fork", Fork);
  SET_METHOD(isolate, sys, "exec", Exec);
  SET_METHOD(isolate, sys, "getsid", GetSid);
  SET_METHOD(isolate, sys, "setsid", SetSid);
  SET_METHOD(isolate, sys, "errno", Errno);
  SET_METHOD(isolate, sys, "getchar", Getchar);
  SET_METHOD(isolate, sys, "putchar", Putchar);
  SET_METHOD(isolate, sys, "strerror", StrError);
  SET_METHOD(isolate, sys, "cpuUsage", CPUUsage);
  SET_METHOD(isolate, sys, "getrUsage", GetrUsage);
  SET_METHOD(isolate, sys, "hrtime", HRTime);
  SET_METHOD(isolate, sys, "cwd", Cwd);
  SET_METHOD(isolate, sys, "env", Env);
  SET_METHOD(isolate, sys, "ioctl", Ioctl);
  SET_METHOD(isolate, sys, "spawn", Spawn);
  SET_METHOD(isolate, sys, "waitpid", WaitPID);
  SET_METHOD(isolate, sys, "runMicroTasks", RunMicroTasks);
  SET_METHOD(isolate, sys, "nextTick", EnqueueMicrotask);
  SET_METHOD(isolate, sys, "exit", Exit);
  SET_METHOD(isolate, sys, "kill", Kill);
  SET_METHOD(isolate, sys, "usleep", USleep);
  SET_METHOD(isolate, sys, "pages", AvailablePages);
  SET_METHOD(isolate, sys, "nanosleep", NanoSleep);
  SET_METHOD(isolate, sys, "mmap", MMap);
  SET_METHOD(isolate, sys, "munmap", MUnmap);
  SET_METHOD(isolate, sys, "reboot", Reboot);
  SET_METHOD(isolate, sys, "getenv", Getenv);
  SET_METHOD(isolate, sys, "setenv", Setenv);
  SET_METHOD(isolate, sys, "unsetenv", Unsetenv);
#ifndef STATIC
  SET_METHOD(isolate, sys, "dlopen", DLOpen);
  SET_METHOD(isolate, sys, "dlsym", DLSym);
  SET_METHOD(isolate, sys, "dlerror", DLError);
  SET_METHOD(isolate, sys, "dlclose", DLClose);
  SET_VALUE(isolate, sys, "RTLD_LAZY", Integer::New(isolate, RTLD_LAZY));
  SET_VALUE(isolate, sys, "RTLD_NOW", Integer::New(isolate, RTLD_NOW));
#endif
  SET_VALUE(isolate, sys, "CLOCK_MONOTONIC", Integer::New(isolate, 
    CLOCK_MONOTONIC));
  SET_VALUE(isolate, sys, "TFD_NONBLOCK", Integer::New(isolate, 
    TFD_NONBLOCK));
  SET_VALUE(isolate, sys, "TFD_CLOEXEC", Integer::New(isolate, 
    TFD_CLOEXEC));
  SET_VALUE(isolate, sys, "FD_CLOEXEC", Integer::New(isolate, 
    FD_CLOEXEC));
  SET_VALUE(isolate, sys, "O_CLOEXEC", Integer::New(isolate, 
    O_CLOEXEC));
  SET_VALUE(isolate, sys, "F_GETFL", Integer::New(isolate, F_GETFL));
  SET_VALUE(isolate, sys, "F_SETFL", Integer::New(isolate, F_SETFL));
  SET_VALUE(isolate, sys, "F_GETFD", Integer::New(isolate, F_GETFD));
  SET_VALUE(isolate, sys, "F_SETFD", Integer::New(isolate, F_SETFD));
  SET_VALUE(isolate, sys, "STDIN_FILENO", Integer::New(isolate, 
    STDIN_FILENO));
  SET_VALUE(isolate, sys, "STDOUT_FILENO", Integer::New(isolate, 
    STDOUT_FILENO));
  SET_VALUE(isolate, sys, "STDERR_FILENO", Integer::New(isolate, 
    STDERR_FILENO));    
  SET_VALUE(isolate, sys, "PROT_READ", Integer::New(isolate, PROT_READ));
  SET_VALUE(isolate, sys, "PROT_WRITE", Integer::New(isolate, PROT_WRITE));
  SET_VALUE(isolate, sys, "MAP_SHARED", Integer::New(isolate, MAP_SHARED));
  SET_VALUE(isolate, sys, "MAP_ANONYMOUS", Integer::New(isolate, MAP_ANONYMOUS));
  SET_VALUE(isolate, sys, "RB_AUTOBOOT", Integer::New(isolate, RB_AUTOBOOT));
  SET_VALUE(isolate, sys, "RB_HALT_SYSTEM", Integer::New(isolate, RB_HALT_SYSTEM));
  SET_VALUE(isolate, sys, "RB_POWER_OFF", Integer::New(isolate, RB_POWER_OFF));
  SET_VALUE(isolate, sys, "RB_SW_SUSPEND", Integer::New(isolate, RB_SW_SUSPEND));
  SET_VALUE(isolate, sys, "TIOCNOTTY", Integer::New(isolate, TIOCNOTTY));
  SET_VALUE(isolate, sys, "TIOCSCTTY", Integer::New(isolate, TIOCSCTTY));
  SET_VALUE(isolate, sys, "WNOHANG", Integer::New(isolate, WNOHANG));
  SET_VALUE(isolate, sys, "WUNTRACED", Integer::New(isolate, WUNTRACED));
  SET_VALUE(isolate, sys, "WCONTINUED", Integer::New(isolate, WCONTINUED));
  SET_VALUE(isolate, sys, "RUSAGE_SELF", Integer::New(isolate, RUSAGE_SELF));
  SET_VALUE(isolate, sys, "RUSAGE_THREAD", Integer::New(isolate, RUSAGE_THREAD));
#ifdef __BYTE_ORDER
  // These don't work on alpine. will have to investigate why not
  SET_VALUE(isolate, sys, "BYTE_ORDER", Integer::New(isolate, __BYTE_ORDER));
  SET_VALUE(isolate, sys, "LITTLE_ENDIAN", Integer::New(isolate, __LITTLE_ENDIAN));
  SET_VALUE(isolate, sys, "BIG_ENDIAN", Integer::New(isolate, __BIG_ENDIAN));
#endif
  long cpus = sysconf(_SC_NPROCESSORS_ONLN);
  long physical_pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGESIZE);
  SET_VALUE(isolate, sys, "cpus", Integer::New(isolate, 
    cpus));
  SET_VALUE(isolate, sys, "physicalPages", Integer::New(isolate, 
    physical_pages));
  SET_VALUE(isolate, sys, "pageSize", Integer::New(isolate, 
    page_size));
  SET_MODULE(isolate, target, "sys", sys);
}
