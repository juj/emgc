#pragma once

#include <emscripten.h>
#include "emgc.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void pin_(uintptr_t value);
void call_from_js_v(void (*func)(void));
void *call_from_js_p(void* (*func)(void));

#ifdef __cplusplus
}
#endif

void require_(int condition, const char *str, const char *file, int line)
{
  if (!condition)
  {
    printf("%s:%d: Test \"%s\" failed!\n", file, line, str);
    exit(1);
  }
}

#define CALL_INDIRECTLY(func) call_from_js_v((func));
#define CALL_INDIRECTLY_P(func) call_from_js_p((func));
#define PIN(x) pin_((uintptr_t)(x))
#define require(x) require_(x, #x, __FILE__, __LINE__)
