/*
 * Copyright (c) 2014 Lukasz Marek <lukasz.m.luki@gmail.com>
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
#include "libavcodec/ffjni.h"
#include "libavcodec/jni.h"

#include "libavutil/avstring.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "url.h"


struct JNIOkhttpFields {

    jclass okhttp_class;

    jmethodID init_method;

    jmethodID okhttp_open_method;

    jmethodID okhttp_read_method;

    jmethodID okhttp_seek_method;

    jmethodID okhttp_close_method;

};


#define OFFSET(x) offsetof(struct JNIOkhttpFields, x)
static const struct FFJniField jfields_okhttp_mapping[] = {
    { "com/solarized/firedown/ffmpegutils/FFmpegOkhttp", NULL, NULL, FF_JNI_CLASS, OFFSET(okhttp_class), 1 },
        { "com/solarized/firedown/ffmpegutils/FFmpegOkhttp", "<init>", "(Ljava/lang/String;Ljava/lang/String;)V", FF_JNI_METHOD, OFFSET(init_method), 1 },
        { "com/solarized/firedown/ffmpegutils/FFmpegOkhttp", "okhttpOpen", "()I", FF_JNI_METHOD, OFFSET(okhttp_open_method), 1 },
        { "com/solarized/firedown/ffmpegutils/FFmpegOkhttp", "okhttpRead", "([BI)I", FF_JNI_METHOD, OFFSET(okhttp_read_method), 1 },
        { "com/solarized/firedown/ffmpegutils/FFmpegOkhttp", "okhttpSeek", "(JI)J", FF_JNI_METHOD, OFFSET(okhttp_seek_method), 1 },
        { "com/solarized/firedown/ffmpegutils/FFmpegOkhttp", "okhttpClose", "()V", FF_JNI_METHOD, OFFSET(okhttp_close_method), 1 },
    { NULL }
};
#undef OFFSET

typedef struct {
    const AVClass *class;

    char *headers;

    struct JNIOkhttpFields jfields;

    jbyteArray jarray;

    jobject thiz;

} OkhttpContext;


#define OFFSET(x) offsetof(OkhttpContext, x)
#define D AV_OPT_FLAG_DECODING_PARAM
#define E AV_OPT_FLAG_ENCODING_PARAM

static const AVOption options[] = {
    { "headers", "set custom HTTP headers, can override built in default headers", OFFSET(headers), AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, D | E },
    { NULL }
};
#undef OFFSET


#define SEGMENT_SIZE 8192

static int okhttp_close(URLContext *h)
{
    OkhttpContext *c = h->priv_data;
    JNIEnv *env = NULL;
    int ret = 0;

    av_log(c, AV_LOG_DEBUG, "okhttp_close\n");

    if (!c->thiz || !c->jarray) {
        return 0;
    }

    env = ff_jni_get_env(h);

    if (!env) {
        return AVERROR(EINVAL);
    }

    (*env)->CallVoidMethod(env, c->thiz, c->jfields.okhttp_close_method);

    if (ff_jni_exception_check(env, 1, c->thiz) < 0) {
        ret = AVERROR_EXTERNAL;
    }

    (*env)->DeleteGlobalRef(env, c->jarray);

    c->jarray = NULL;

    (*env)->DeleteGlobalRef(env, c->thiz);

    c->thiz = NULL;

    ff_jni_reset_jfields(env, &c->jfields, jfields_okhttp_mapping, 1, c);

    return ret;
}


static int okhttp_open(URLContext *h, const char *uri, int flags)
{
    OkhttpContext *c = h->priv_data;
    JNIEnv *env = NULL;
    jobject object = NULL;
    jobject url = NULL;
    jobject headers = NULL;
    jbyteArray array = NULL;
    int ret = 0;

    av_jni_get_java_vm(h);

    env = ff_jni_get_env(h);

    if (!env) {
        return AVERROR(EINVAL);
    }

    ret = ff_jni_init_jfields(env, &c->jfields, jfields_okhttp_mapping, 1, c);

    if (ret < 0) {
        av_log(c, AV_LOG_ERROR, "failed to initialize jni fields\n");
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    url = ff_jni_utf_chars_to_jstring(env, uri, c);

    if (!url) {
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    headers = ff_jni_utf_chars_to_jstring(env, c->headers, c);

    if (!headers) {
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    object = (*env)->NewObject(env, c->jfields.okhttp_class, c->jfields.init_method, url, headers);

    if (!object) {
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    c->thiz = (*env)->NewGlobalRef(env, object);

    if (!c->thiz) {
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    array = (*env)->NewByteArray(env, SEGMENT_SIZE);

    if(!array){
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    c->jarray = (*env)->NewGlobalRef(env, array);

    if (!c->jarray) {
        ret = AVERROR_EXTERNAL;
        goto done;
    }


    ret = (*env)->CallIntMethod(env, c->thiz, c->jfields.okhttp_open_method);

    if(ret) {
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    done:

    if(ret < 0) {
        ff_jni_reset_jfields(env, &c->jfields, jfields_okhttp_mapping, 1, c);
        (*env)->DeleteGlobalRef(env, c->thiz);
    }

    (*env)->DeleteLocalRef(env, array);
    (*env)->DeleteLocalRef(env, object);
    (*env)->DeleteLocalRef(env, url);
    (*env)->DeleteLocalRef(env, headers);

    return ret;

}


static int okhttp_read(URLContext *h, unsigned char *buf, int size)
{
    OkhttpContext *c = h->priv_data;
    JNIEnv *env = NULL;
    jsize bytes_read = 0;
    int buffer_size = 0;


    av_log(c, AV_LOG_DEBUG, "okhttp_read size: %d\n", size);

    env = ff_jni_get_env(h);

    if (!env) {
        return AVERROR(EINVAL);
    }

    if (ff_check_interrupt(&h->interrupt_callback)) {
        av_log(c, AV_LOG_ERROR, "okhttp_read interrupt callback\n");
        return AVERROR_EXIT;
    }

    //(*state->env)->SetByteArrayRegion(state->env, array, 0, buf_size, (jbyte *) buf);

    buffer_size = size > SEGMENT_SIZE ? SEGMENT_SIZE : size;


    bytes_read = (*env)->CallIntMethod(env, c->thiz,
                                           c->jfields.okhttp_read_method, c->jarray, buffer_size);

    if (ff_jni_exception_check(env, 1, c->thiz) < 0) {
        av_log(c, AV_LOG_ERROR, "okhttp_read, bytes_read exception\n");
        bytes_read = 0;
    }

    av_log(c, AV_LOG_DEBUG, "okhttp_read, bytes_read: %ld\n", bytes_read);


    if(bytes_read > 0) {

       (*env)->GetByteArrayRegion(env, c->jarray, 0, bytes_read, buf);

    }

    av_log(c, AV_LOG_DEBUG, "okhttp_read, bytes_read result: %d\n", bytes_read ? bytes_read : AVERROR_EOF);

    return bytes_read ? bytes_read : AVERROR_EOF;
}

static int64_t okhttp_seek(URLContext *h, int64_t off, int whence)
{
    OkhttpContext *c = h->priv_data;
    int64_t result = 0;
    JNIEnv *env;

    env = ff_jni_get_env(h);

    if (!env) {
        return AVERROR(EINVAL);
    }

    if (ff_check_interrupt(&h->interrupt_callback)) {
        av_log(c, AV_LOG_DEBUG, "okhttp_read callback: %d\n", h->interrupt_callback);
        return AVERROR_EXIT;
    }

    result = (*env)->CallLongMethod(env, c->thiz,
                                           c->jfields.okhttp_seek_method, off, whence);

    if (ff_jni_exception_check(env, 1, c->thiz) < 0) {
        result = AVERROR_EOF;
    }


    if(result < 0)
        result = AVERROR_EOF;

    av_log(c, AV_LOG_DEBUG, "okhttp_seek %ld\n", result);

    return result;

}


static const AVClass okhttp_context_class = {
    .class_name = "okhttp",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

const URLProtocol ff_http_protocol = {
    .name                = "http",
    .url_open            = okhttp_open,
    .url_read            = okhttp_read,
    .url_seek            = okhttp_seek,
    .url_close           = okhttp_close,
    .priv_data_size      = sizeof(OkhttpContext),
    .priv_data_class     = &okhttp_context_class,
    .flags               = URL_PROTOCOL_FLAG_NETWORK,
    .default_whitelist   = "http,https,tls,tcp,udp,crypto"
};

const URLProtocol ff_https_protocol = {
    .name                = "https",
    .url_open            = okhttp_open,
    .url_read            = okhttp_read,
    .url_seek            = okhttp_seek,
    .url_close           = okhttp_close,
    .priv_data_size      = sizeof(OkhttpContext),
    .priv_data_class     = &okhttp_context_class,
    .flags               = URL_PROTOCOL_FLAG_NETWORK,
    .default_whitelist   = "http,https,tls,tcp,udp,crypto"
};
