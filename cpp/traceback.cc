#ifndef _MSC_VER
#include "./common.h"
#include <backtrace.h>
#include <cstring>
#include <cxxabi.h>
#include <iostream>

namespace mlc {
namespace backtrace {
namespace {

static int32_t MLC_TRACEBACK_LIMIT = GetTracebackLimit();
static backtrace_state *_bt_state = backtrace_create_state(
    /*filename=*/
    nullptr,
    /*threaded=*/1,
    /*error_callback=*/
    +[](void * /*data*/, const char *msg, int /*errnum*/) -> void {
      std::cerr << "Failed to initialize libbacktrace: " << msg << std::endl;
    },
    /*data=*/nullptr);
thread_local TracebackStorage storage;

const char *CxaDemangle(const char *name) {
  static thread_local struct _TLS {
    size_t cxa_length = 1024;
    char *cxa_buffer = nullptr;
    _TLS() { this->cxa_buffer = static_cast<char *>(std::malloc(cxa_length * sizeof(char))); }
    ~_TLS() { std::free(cxa_buffer); }
  } _tls;

  int status = 0;
  size_t length = _tls.cxa_length;
  char *demangled_name = abi::__cxa_demangle(name, _tls.cxa_buffer, &length, &status);
  if (length > _tls.cxa_length) {
    _tls.cxa_length = length;
    _tls.cxa_buffer = demangled_name;
  }
  return (demangled_name && length && !status) ? demangled_name : name;
}

MLCByteArray TracebackImpl() {
  storage.buffer.clear();

  auto callback = +[](void *data, uintptr_t pc, const char *filename, int lineno, const char *symbol) -> int {
    if (!filename) {
      filename = "<unknown>";
    }
    if (!symbol) {
      backtrace_syminfo(
          /*state=*/
          _bt_state, /*addr=*/pc, /*callback=*/
          +[](void *data, uintptr_t /*pc*/, const char *symname, uintptr_t /*symval*/, uintptr_t /*symsize*/) {
            *reinterpret_cast<const char **>(data) = symname;
          },
          /*error_callback=*/+[](void * /*data*/, const char * /*msg*/, int /*errnum*/) {},
          /*data=*/&symbol);
    }
    thread_local char number_buffer[32];
    symbol = symbol ? CxaDemangle(symbol) : StringifyPointer(pc, number_buffer, sizeof(number_buffer));
    if (IsForeignFrame(filename, lineno, symbol)) {
      return 1;
    }
    if (!EndsWith(filename, "core.pyx") &&       //
        !EndsWith(filename, "func.h") &&         //
        !EndsWith(filename, "func_details.h") && //
        !EndsWith(filename, "visitor.h") &&      //
        !EndsWith(filename, "mlc/core/all.h") && //
        !EndsWith(filename, "mlc/base/all.h")    //
    ) {
      TracebackStorage *storage = reinterpret_cast<TracebackStorage *>(data);
      storage->Append(filename);
      storage->Append(lineno);
      storage->Append(symbol);
    }
    return 0;
  };
  backtrace_full(
      /*state=*/
      _bt_state, /*skip=*/1, /*callback=*/callback,
      /*error_callback=*/[](void * /*data*/, const char * /*msg*/, int /*errnum*/) {},
      /*data=*/&storage);
  return {static_cast<int64_t>(storage.buffer.size()), storage.buffer.data()};
}

} // namespace
} // namespace backtrace
} // namespace mlc

MLC_BACKTRACE_API MLCByteArray MLCTraceback(const char *, const char *, const char *) {
  using namespace mlc::backtrace;
  if (_bt_state) {
    return TracebackImpl();
  }
  return {0, nullptr};
}
#endif // _MSC_VER
