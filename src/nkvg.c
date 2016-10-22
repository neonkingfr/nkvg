/*
 * Copyright (c) 2016 Yaroslav Tsarko eriktsarko@gmail.com

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
 * EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "nkvg.h"
#include "nkvg-render.h"

#include <alloca.h>
#include <assert.h>
#include <errno.h>

#include <GL/gl3w.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

#define COMMAND_BUFFER_SIZE (4 * 1024)

struct nk_vg_context {
    struct nk_buffer commands_buffer;
    NVGcontext *nvg_context;
};

static int nk_vg_device_create(struct nk_vg_context *ctx);
static void nk_vg_device_destroy(struct nk_vg_context *ctx);

int nk_vg_create(struct nk_context *nk_ctx, struct nk_vg **out)
{
    assert(nk_ctx);
    assert(out);

    int retval                      = 0;
    struct nk_vg *nk_vg             = 0;
    struct nk_vg_context *nk_vg_ctx = 0;

    nk_vg = calloc(1, sizeof(*nk_vg));
    if (!nk_vg) {
        return ENOMEM;
    }

    nk_vg_ctx = calloc(1, sizeof(*nk_vg_ctx));
    if (!nk_vg_ctx) {
        retval = ENOMEM;
        goto err;
    }

    retval = nk_vg_device_create(nk_vg_ctx);
    if (retval != 0) {
        goto err;
    }

    nk_vg->nkvg_ctx = nk_vg_ctx;
    nk_vg->nk_ctx   = nk_ctx;

    *out = nk_vg;

    return 0;

err:
    *out = 0;

    if (nk_vg_ctx) {
        free(nk_vg_ctx);
    }

    if (nk_vg) {
        free(nk_vg);
    }

    return retval;
}

void nk_vg_destroy(struct nk_vg **nkvg)
{
    assert(nkvg);

    nk_vg_device_destroy((*nkvg)->nkvg_ctx);
    free(*nkvg);

    *nkvg = 0;
}

NVGcontext *nk_vg_context(struct nk_vg *nkvg)
{
    assert(nkvg);
    return nkvg->nkvg_ctx->nvg_context;
}

int nk_vg_add_font(struct nk_vg *nkvg, const char *id, const char *filename)
{
    int ret = nvgCreateFont(nkvg->nkvg_ctx->nvg_context, id, filename);

    return ret;
}

void nk_vg_render(struct nk_vg *nkvg, struct nk_color bg, int w, int h)
{
    assert(nkvg);

    struct nk_vg_context *ctx = nkvg->nkvg_ctx;
    assert(ctx);

    void *commands = nk_buffer_memory(&nkvg->nk_ctx->memory);
    if ((ctx->commands_buffer.memory.size >= nkvg->nk_ctx->memory.allocated) &&
        (memcmp(commands, ctx->commands_buffer.memory.ptr,
                nkvg->nk_ctx->memory.allocated) == 0)) {
        /* Buffers are equal, frame didn`t change */
        goto end;
    }

    /* Frame changed */

    nk_buffer_clear(&ctx->commands_buffer);

    if (nkvg->nk_ctx->memory.allocated > ctx->commands_buffer.memory.size) {
        nk_size new_size = ctx->commands_buffer.memory.size * 2;
        if (nkvg->nk_ctx->memory.allocated > new_size) {
            new_size = nkvg->nk_ctx->memory.allocated * 2;
        }

        // fprintf(stderr, "reallocating command buffer to new size: %zu\n",
        //     new_size);

        void *mem = realloc(ctx->commands_buffer.memory.ptr, new_size);
        if (!mem) {
            fprintf(stderr,
                    "failed to reallocate command buffer. This is possibly an\
                indication of out of memory. The program will now terminate\n");
            abort();
        }

        nk_buffer_free(&ctx->commands_buffer);

        memset(mem, 0, new_size);

        nk_buffer_init_fixed(&ctx->commands_buffer, mem, new_size);
    }

    memcpy(ctx->commands_buffer.memory.ptr, commands,
           nkvg->nk_ctx->memory.allocated);

    float background[4];
    nk_color_fv(background, bg);

    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glClearColor(background[0], background[1], background[2], background[3]);
    nk_nanovg_render(nkvg, w, h);

end:
    nk_clear(nkvg->nk_ctx);
}

static int nk_vg_device_create(struct nk_vg_context *ctx)
{
    ctx->nvg_context = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!ctx->nvg_context) {
        return 1;
    }

    int ret   = 0;
    void *mem = calloc(1, COMMAND_BUFFER_SIZE);
    if (!mem) {
        ret = ENOMEM;
        goto err;
    }

    nk_buffer_init_fixed(&ctx->commands_buffer, mem, COMMAND_BUFFER_SIZE);

    return 0;

err:
    nvgDeleteGL3(ctx->nvg_context);

    return ret;
}

static void nk_vg_device_destroy(struct nk_vg_context *ctx)
{
    free(ctx->commands_buffer.memory.ptr);
    nk_buffer_free(&ctx->commands_buffer);
    nvgDeleteGL3(ctx->nvg_context);
}
