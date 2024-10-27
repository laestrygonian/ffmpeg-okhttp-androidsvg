/*
 * Librsvg rasterization wrapper
 * Copyright (c) 2017 Rostislav Pehlivanov <atomnuker@gmail.com>
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

#include <jni.h>
#include <android/bitmap.h>
#include "libavcodec/ffjni.h"
#include "libavcodec/jni.h"
#include "libavutil/imgutils.h"

#include "avcodec.h"
#include "codec_internal.h"
#include "decode.h"
#include "libavutil/opt.h"


struct JNISVGFields {

    jclass svg_class;

    jmethodID svg_init_method;

    jmethodID svg_close_method;

    jmethodID svg_decode_method;

    jmethodID svg_width_method;

    jmethodID svg_height_method;

};


#define OFFSET(x) offsetof(struct JNISVGFields, x)
static const struct FFJniField jfields_svg_mapping[] = {
    { "com/solarized/firedown/ffmpegutils/FFmpegSVGDecoder", NULL, NULL, FF_JNI_CLASS, OFFSET(svg_class), 1 },
        { "com/solarized/firedown/ffmpegutils/FFmpegSVGDecoder", "<init>", "()V", FF_JNI_METHOD, OFFSET(svg_init_method), 1 },
        { "com/solarized/firedown/ffmpegutils/FFmpegSVGDecoder", "decodeClose", "()V", FF_JNI_METHOD, OFFSET(svg_close_method), 1 },
        { "com/solarized/firedown/ffmpegutils/FFmpegSVGDecoder", "decodeData", "([B)Landroid/graphics/Bitmap;", FF_JNI_METHOD, OFFSET(svg_decode_method), 1 },
        { "com/solarized/firedown/ffmpegutils/FFmpegSVGDecoder", "decodeData", "([B)Landroid/graphics/Bitmap;", FF_JNI_METHOD, OFFSET(svg_decode_method), 1 },
        { "com/solarized/firedown/ffmpegutils/FFmpegSVGDecoder", "getDocumentWidth", "()I", FF_JNI_METHOD, OFFSET(svg_width_method), 1 },
        { "com/solarized/firedown/ffmpegutils/FFmpegSVGDecoder", "getDocumentHeight", "()I", FF_JNI_METHOD, OFFSET(svg_height_method), 1 },
    { NULL }
};
#undef OFFSET


typedef struct LibASVGContext {
    AVClass *class;

    struct JNISVGFields jfields;

    jobject thiz;

} LibASVGContext;


static int libasvg_decode_close(AVCodecContext *avctx)
{
    LibASVGContext *c = avctx->priv_data;
    JNIEnv *env = NULL;
    int ret = 0;

    av_log(c, AV_LOG_DEBUG, "libasvg_close\n");

    if (!c->thiz) {
        return 0;
    }

    env = ff_jni_get_env(c);

    if (!env) {
        return AVERROR(EINVAL);
    }

    (*env)->CallVoidMethod(env, c->thiz, c->jfields.svg_close_method);

    if (ff_jni_exception_check(env, 1, c->thiz) < 0) {
        ret = AVERROR_EXTERNAL;
    }

    (*env)->DeleteGlobalRef(env, c->thiz);

    c->thiz = NULL;

    ff_jni_reset_jfields(env, &c->jfields, jfields_svg_mapping, 1, c);

    return ret;
}


static int libasvg_decode_init(AVCodecContext *avctx)
{
    LibASVGContext *c = avctx->priv_data;
    jobject object = NULL;
    JNIEnv *env = NULL;
    int ret = 0;

    av_jni_get_java_vm(c);

    env = ff_jni_get_env(c);

    if (!env) {
        return AVERROR(EINVAL);
    }

    ret = ff_jni_init_jfields(env, &c->jfields, jfields_svg_mapping, 1, c);

    if (ret < 0) {
        av_log(c, AV_LOG_ERROR, "failed to initialize jni fields\n");
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    object = (*env)->NewObject(env, c->jfields.svg_class, c->jfields.svg_init_method);

    if (!object) {
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    c->thiz = (*env)->NewGlobalRef(env, object);

    if (!c->thiz) {
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    done:

    if(ret < 0) {
        ff_jni_reset_jfields(env, &c->jfields, jfields_svg_mapping, 1, c);
        (*env)->DeleteGlobalRef(env, c->thiz);
    }

    (*env)->DeleteLocalRef(env, object);

    return ret;



}

static int libasvg_decode_frame(AVCodecContext *avctx, AVFrame *frame,
                                int *got_frame, AVPacket *pkt)
{
    int ret;
    LibASVGContext *c = avctx->priv_data;
    JNIEnv *env = NULL;
    jbyteArray array = NULL;
    jobject bitmap = NULL;
    AndroidBitmapInfo bitmapInfo;
    void* bitmapPixels = NULL;
    *got_frame = 0;

    av_log(c, AV_LOG_DEBUG, "libasvg_decode_frame\n");

    env = ff_jni_get_env(c);

    if (!env) {
        return AVERROR(EINVAL);
    }

    array = (*env)->NewByteArray(env, pkt->size);

    if(array == NULL) {
        av_log(c, AV_LOG_ERROR, "Error during decode, no memory\n");
        goto end;
    }

    (*env)->SetByteArrayRegion(env, array, 0, pkt->size, (jbyte *) pkt->data);

    av_log(c, AV_LOG_DEBUG, "libasvg_decode_frame, pkt->size: %d\n", pkt->size);


    bitmap = (*env)->CallObjectMethod(env, c->thiz,
                                           c->jfields.svg_decode_method, array);

    if(!bitmap) {
        av_log(avctx, AV_LOG_ERROR, "Error rendering svg\n");
        ret = AVERROR_INVALIDDATA;
        goto end;
    }



    AndroidBitmap_getInfo(env, bitmap, &bitmapInfo);

    av_log(c, AV_LOG_DEBUG, "libasvg_decode_frame, width: %d height: %d\n", bitmapInfo.width, bitmapInfo.height);

    ret = ff_set_dimensions(avctx, bitmapInfo.width, bitmapInfo.height);


    if (ret < 0)
        goto end;

    avctx->pix_fmt = AV_PIX_FMT_RGBA;

    ret = ff_get_buffer(avctx, frame, 0);

    if (ret < 0)
        goto end;

    frame->pict_type = AV_PICTURE_TYPE_I;
    frame->flags |= AV_FRAME_FLAG_KEY;

    AndroidBitmap_lockPixels(env, bitmap, &bitmapPixels);

    av_image_fill_arrays(frame->data, frame->linesize,
                         (const unsigned char *) bitmapPixels, AV_PIX_FMT_RGBA,
                         bitmapInfo.width, bitmapInfo.height, 1);

    AndroidBitmap_unlockPixels(env, bitmap);

    //Hack set correct dimens


    avctx->coded_width = (*env)->CallIntMethod(env, c->thiz,
                                           c->jfields.svg_width_method);

    avctx->coded_height = (*env)->CallIntMethod(env, c->thiz,
                                           c->jfields.svg_height_method);


    *got_frame = 1;
    ret = 0;

    av_log(c, AV_LOG_DEBUG, "libasvg_decode_frame end");

end:

    (*env)->DeleteLocalRef(env, bitmap);
    (*env)->DeleteLocalRef(env, array);


    return ret;
}

#define OFFSET(x) offsetof(LibASVGContext, x)
#define DEC (AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM)
static const AVOption options[] = {
    { NULL },
};

static const AVClass libasvg_decoder_class = {
    .class_name = "Libasvg",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

const FFCodec ff_libasvg_decoder = {
    .p.name         = "libasvg",
    CODEC_LONG_NAME("Libasvg android rasterizer"),
    .p.priv_class   = &libasvg_decoder_class,
    .p.type         = AVMEDIA_TYPE_VIDEO,
    .p.id           = AV_CODEC_ID_SVG,
    .p.capabilities = AV_CODEC_CAP_DR1,
    .p.wrapper_name = "libasvg",
    .init           = libasvg_decode_init,
    FF_CODEC_DECODE_CB(libasvg_decode_frame),
    .close          = libasvg_decode_close,
    .priv_data_size = sizeof(LibASVGContext),
};
