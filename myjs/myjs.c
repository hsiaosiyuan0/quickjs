#include "include/quickjs-libc.h"
#include "include/quickjs.h"
#include <stdio.h>
#include <stdlib.h>

int readfile(const char *filename, char **out) {
  FILE *f = fopen(filename, "rb");
  if (f == NULL)
    return -1;

  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  *out = malloc(fsize + 1);
  fread(*out, fsize, 1, f);
  fclose(f);
  return fsize;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Error: a js file is required\n");
    return -1;
  }

  int len;
  char *code;
  const char *filename = argv[1];

  len = readfile(filename, &code);
  if (len == -1) {
    printf("Error: unable to readfile: %s\n", filename);
    return -1;
  }

  printf("========== Code ==========\n");
  fwrite(code, sizeof(char), len, stdout);

  printf("\n======== Execution =======\n");

  JSRuntime *rt = JS_NewRuntime();
  JSContext *ctx = JS_NewContext(rt);

  // 添加 os 模块前的准备工作
  js_std_init_handlers(rt);
  js_init_module_os(ctx, "os");

  js_std_add_helpers(ctx, 0, NULL);

  JSValue val = JS_Eval(ctx, code, len, filename, JS_EVAL_TYPE_MODULE);
  if (JS_IsException(val)) {
    js_std_dump_error(ctx);
    return -1;
  }
  js_std_loop(ctx);
  return 0;
}
