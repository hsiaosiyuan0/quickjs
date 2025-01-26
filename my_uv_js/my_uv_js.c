#include "include/quickjs-libc.h"
#include "include/quickjs.h"
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

typedef struct {
    JSContext *ctx;
    JSValue func;
} TimerData;

static void timer_cb(uv_timer_t *handle) {
    TimerData *td = (TimerData *)handle->data;
    JSContext *ctx = td->ctx;
    JSValue func = td->func;

    JSValue ret = JS_Call(ctx, func, JS_UNDEFINED, 0, NULL);
    if (JS_IsException(ret)) {
        js_std_dump_error(ctx);
    }
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, func);
    free(td);
    uv_close((uv_handle_t*)handle, (uv_close_cb)free);
}

static JSValue js_setTimeout(JSContext *ctx, JSValue this_val,
                             int argc, JSValue *argv) {
    int64_t delay;
    if (JS_ToInt64(ctx, &delay, argv[1]) != 0) {
        return JS_EXCEPTION;
    }

    JSValue func = JS_DupValue(ctx, argv[0]);

    uv_timer_t *timer = malloc(sizeof(uv_timer_t));
    TimerData *td = malloc(sizeof(TimerData));
    td->ctx = ctx;
    td->func = func;

    uv_timer_init(uv_default_loop(), timer);
    timer->data = td;
    uv_timer_start(timer, timer_cb, delay, 0);

    return JS_UNDEFINED;
}

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
    (*out)[fsize] = 0;
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

    printf("========== Code ==========\n%s\n", code);
    printf("\n======== Execution =======\n");

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    uv_loop_t *loop = uv_default_loop();

    js_std_init_handlers(rt);
    js_std_add_helpers(ctx, 0, NULL);

    // 将 setTimeout 注册到全局对象
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue setTimeout = JS_NewCFunction(ctx, js_setTimeout, "setTimeout", 2);
    JS_SetPropertyStr(ctx, global, "setTimeout", setTimeout);
    JS_FreeValue(ctx, global);

    JSValue val = JS_Eval(ctx, code, len, filename, JS_EVAL_TYPE_MODULE);
    if (JS_IsException(val)) {
        js_std_dump_error(ctx);
        return -1;
    }

    int has_pending_events;
    do {
        JSContext *ctx;
        int js_pending;
        do {
            js_pending = JS_ExecutePendingJob(rt, &ctx);
            if (js_pending < 0) {
                js_std_dump_error(ctx);
                break;
            }
        } while (js_pending > 0);

        uv_run(loop, UV_RUN_NOWAIT);
        has_pending_events = uv_loop_alive(loop);
    } while (has_pending_events);

    // 清理资源
    JS_FreeValue(ctx, val);
    free(code);
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    uv_loop_close(loop);
    return 0;
}