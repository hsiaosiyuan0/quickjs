#include "include/quickjs-libc.h"
#include "include/quickjs.h"
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

typedef struct {
  JSContext *ctx;
  JSValue func;
  uv_timer_t *timer;
  int is_interval;
} TimerData;

// 新增：统一资源释放回调
static void timer_close_cb(uv_handle_t *handle) {
  TimerData *td = (TimerData *)handle->data;
  JS_FreeValue(td->ctx, td->func);
  free(td);
  free(handle);
}

static void timer_cb(uv_timer_t *handle) {
  TimerData *td = (TimerData *)handle->data;
  JSContext *ctx = td->ctx;

  JSValue ret = JS_Call(ctx, td->func, JS_UNDEFINED, 0, NULL);
  if (JS_IsException(ret)) {
    js_std_dump_error(ctx);
  }
  JS_FreeValue(ctx, ret);

  if (!td->is_interval) {
    uv_timer_stop(handle);
    uv_close((uv_handle_t *)handle, timer_close_cb);
  }
}

static JSValue js_setTimeout(JSContext *ctx, JSValue this_val, int argc,
                             JSValue *argv) {
  int64_t delay;
  if (JS_ToInt64(ctx, &delay, argv[1]) != 0) {
    return JS_EXCEPTION;
  }

  JSValue func = JS_DupValue(ctx, argv[0]);

  uv_timer_t *timer = malloc(sizeof(uv_timer_t));
  TimerData *td = malloc(sizeof(TimerData));
  td->ctx = ctx;
  td->func = func;
  td->timer = timer;
  td->is_interval = 0;

  uv_timer_init(uv_default_loop(), timer);
  timer->data = td;
  uv_timer_start(timer, timer_cb, delay, 0);

  return JS_NewInt64(ctx, (int64_t)timer);
}

static JSValue js_setInterval(JSContext *ctx, JSValue this_val, int argc,
                              JSValue *argv) {
  int64_t interval;
  if (JS_ToInt64(ctx, &interval, argv[1]) != 0) {
    return JS_EXCEPTION;
  }

  JSValue func = JS_DupValue(ctx, argv[0]);

  uv_timer_t *timer = malloc(sizeof(uv_timer_t));
  TimerData *td = malloc(sizeof(TimerData));
  td->ctx = ctx;
  td->func = func;
  td->timer = timer;
  td->is_interval = 1;

  uv_timer_init(uv_default_loop(), timer);
  timer->data = td;
  uv_timer_start(timer, timer_cb, interval, interval);

  return JS_NewInt64(ctx, (int64_t)timer);
}

static JSValue js_clearTimer(JSContext *ctx, JSValue this_val, int argc,
                             JSValue *argv) {
  int64_t timer_ptr;
  if (JS_ToInt64(ctx, &timer_ptr, argv[0]) != 0) {
    return JS_EXCEPTION;
  }

  uv_timer_t *timer = (uv_timer_t *)timer_ptr;
  uv_timer_stop(timer);
  uv_close((uv_handle_t *)timer, timer_close_cb); // 使用统一回调

  return JS_UNDEFINED;
}

int readfile(const char *filename, char **out) {
  FILE *f = fopen(filename, "rb");
  if (!f)
    return -1;

  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  *out = malloc(fsize + 1);
  fread(*out, fsize, 1, f);
  fclose(f);
  (*out)[fsize] = 0;
  return fsize;
}

// 清理函数封装
void cleanup_resources(JSRuntime *rt, JSContext *ctx, uv_loop_t *loop,
                       char *code, JSValue val) {
  // 安全判断 JSValue 类型
  if (!JS_IsUndefined(val)) {
    JS_FreeValue(ctx, val);
  }

  // 释放文件内容内存
  if (code != NULL) {
    free(code);
  }

  // 清理 QuickJS 运行时
  if (rt != NULL) {
    js_std_free_handlers(rt);
    if (ctx != NULL) {
      JS_FreeContext(ctx);
    }
    JS_FreeRuntime(rt);
  }

  // 关闭 libuv 事件循环
  if (loop != NULL) {
    uv_loop_close(loop);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <js-file>\n", argv[0]);
    return 1;
  }

  char *code = NULL;
  JSRuntime *rt = NULL;
  JSContext *ctx = NULL;
  uv_loop_t *loop = uv_default_loop();
  JSValue val = JS_UNDEFINED;

  // 读取文件
  int len = readfile(argv[1], &code);
  if (len == -1) {
    fprintf(stderr, "Error reading file: %s\n", argv[1]);
    cleanup_resources(NULL, NULL, loop, code, val);
    return 1;
  }

  // 初始化 QuickJS 运行时
  rt = JS_NewRuntime();
  if (!rt) {
    fprintf(stderr, "Error creating JS runtime\n");
    cleanup_resources(NULL, NULL, loop, code, val);
    return 1;
  }

  // 创建上下文
  ctx = JS_NewContext(rt);
  if (!ctx) {
    fprintf(stderr, "Error creating JS context\n");
    cleanup_resources(rt, NULL, loop, code, val);
    return 1;
  }

  // 初始化标准库
  js_std_init_handlers(rt);
  js_std_add_helpers(ctx, 0, NULL);

  // 注册全局函数
  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, "setTimeout",
                    JS_NewCFunction(ctx, js_setTimeout, "setTimeout", 2));
  JS_SetPropertyStr(ctx, global, "setInterval",
                    JS_NewCFunction(ctx, js_setInterval, "setInterval", 2));
  JS_SetPropertyStr(ctx, global, "clearTimeout",
                    JS_NewCFunction(ctx, js_clearTimer, "clearTimeout", 1));
  JS_SetPropertyStr(ctx, global, "clearInterval",
                    JS_NewCFunction(ctx, js_clearTimer, "clearInterval", 1));
  JS_FreeValue(ctx, global);

  // 执行脚本
  val = JS_Eval(ctx, code, len, argv[1], JS_EVAL_TYPE_MODULE);
  if (JS_IsException(val)) {
    js_std_dump_error(ctx);
    cleanup_resources(rt, ctx, loop, code, val);
    return 1;
  }

  // 主事件循环
  while (1) {
    int js_pending;
    do {
      JSContext *ctx;
      js_pending = JS_ExecutePendingJob(rt, &ctx);
      if (js_pending < 0) {
        js_std_dump_error(ctx);
        break;
      }
    } while (js_pending > 0);

    uv_run(loop, UV_RUN_NOWAIT);

    if (!uv_loop_alive(loop))
      break;
  }

  // 正常退出时的清理
  cleanup_resources(rt, ctx, loop, code, val);
  return 0;
}