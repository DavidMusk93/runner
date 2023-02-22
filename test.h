#pragma once

#include <stdio.h>

#include "macro.h"
#include "misc.h"

#define ALWAYS_FLUSH_OUTPUT() \
  setbuf(stdout, 0);          \
  setbuf(stderr, 0)

#define log(fmt, ...)                                                              \
  __log(stdout, "%s %d#%d @%s " fmt, now_string(0, 0), __pid(), __tid(), __func__, \
        ##__VA_ARGS__)
#ifdef NDEBUG
#define debug(fmt, ...)
#else
#define debug(fmt, ...) log("(%s@%d) " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#endif
