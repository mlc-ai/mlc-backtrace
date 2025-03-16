#ifndef MLC_BACKTRACE_H_
#define MLC_BACKTRACE_H_

#include <stdint.h>
#if !defined(MLC_BACKTRACE_API) && defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define MLC_BACKTRACE_API EMSCRIPTEN_KEEPALIVE
#endif
#if !defined(MLC_BACKTRACE_API) && defined(_MSC_VER)
// mlc-backtrace is always a static library, which we don't export/import symbols.
#define MLC_BACKTRACE_API
#endif
#ifndef MLC_BACKTRACE_API
#define MLC_BACKTRACE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int64_t num_bytes;
  const char *bytes;
} MLCByteArray;

MLC_BACKTRACE_API MLCByteArray MLCTraceback(const char *filename, const char *lineno, const char *func_name);

#ifdef __cplusplus
} // MLC_EXTERN_C
#endif

#endif // MLC_BACKTRACE_H_
