/*
 * Copyright (c) 2023 Jeffrey Chapuis
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Sync two video streams, output main video stream unchanged.
 */

#include "framesync.h"
#include "internal.h"

typedef struct DummySyncContext {
    FFFrameSync fs;
} DummySyncContext;

static int do_dummysync(FFFrameSync *fs)
{
    AVFilterContext *ctx = fs->parent;
    AVFrame *main, *dummy;
    int ret;

    if ((ret = ff_framesync_dualinput_get(fs, &main, &dummy)) < 0)
        return ret;

    return ff_filter_frame(ctx->outputs[0], main);
}

static av_cold int init(AVFilterContext *ctx)
{
    DummySyncContext *s = ctx->priv;

    s->fs.on_event = do_dummysync;
    return 0;
}

static int config_output(AVFilterLink *outlink)
{
    AVFilterContext *ctx = outlink->src;
    DummySyncContext *s = ctx->priv;
    AVFilterLink *mainlink = ctx->inputs[0];
    int ret;

    if ((ret = ff_framesync_init_dualinput(&s->fs, ctx)) < 0)
        return ret;

    outlink->w = mainlink->w;
    outlink->h = mainlink->h;
    outlink->time_base = mainlink->time_base;
    outlink->sample_aspect_ratio = mainlink->sample_aspect_ratio;
    outlink->frame_rate = mainlink->frame_rate;
    if ((ret = ff_framesync_configure(&s->fs)) < 0)
        return ret;

    outlink->time_base = s->fs.time_base;
    if (av_cmp_q(mainlink->time_base, outlink->time_base) ||
        av_cmp_q(ctx->inputs[1]->time_base, outlink->time_base))
        av_log(ctx, AV_LOG_WARNING, "not matching timebases found between first input: %d/%d and second input %d/%d, results may be incorrect!\n",
               mainlink->time_base.num, mainlink->time_base.den,
               ctx->inputs[1]->time_base.num, ctx->inputs[1]->time_base.den);

    return 0;
}

static int activate(AVFilterContext *ctx)
{
    DummySyncContext *s = ctx->priv;
    return ff_framesync_activate(&s->fs);
}

static av_cold void uninit(AVFilterContext *ctx)
{
    DummySyncContext *s = ctx->priv;
    ff_framesync_uninit(&s->fs);
}

static const AVFilterPad dummysync_inputs[] = {
    {
        .name         = "main",
        .type         = AVMEDIA_TYPE_VIDEO,
    },{
        .name         = "dummy",
        .type         = AVMEDIA_TYPE_VIDEO,
    },
};

static const AVFilterPad dummysync_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_VIDEO,
        .config_props  = config_output,
    },
};

const AVFilter ff_vf_dummysync = {
    .name          = "dummysync",
    .description   = NULL_IF_CONFIG_SMALL("Sync two video streams, output main video stream unchanged."),
    .init          = init,
    .uninit        = uninit,
    .activate      = activate,
    .priv_size     = sizeof(DummySyncContext),
    FILTER_INPUTS(dummysync_inputs),
    FILTER_OUTPUTS(dummysync_outputs),
    .flags         = AVFILTER_FLAG_SUPPORT_TIMELINE_INTERNAL |
                     AVFILTER_FLAG_SLICE_THREADS             |
                     AVFILTER_FLAG_METADATA_ONLY,
};
