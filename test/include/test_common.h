#ifndef TEST_COMMON_H
#define TEST_COMMON_H

// clang-format off
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cmocka.h>
// clang-format on

#define APPLY_TO_EACH_ARG(fn, arg_type, ...)                                 \
  do {                                                                       \
    arg_type const args[] = {__VA_ARGS__};                                   \
    for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); i++) fn(args[i]); \
  } while (0)

#endif // TEST_COMMON_H
