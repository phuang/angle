//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// NativeBufferImageSiblingOHOS.cpp: Implements the NativeBufferImageSiblingOHOS class

#include "libANGLE/renderer/gl/egl/ohos/NativeBufferImageSiblingOHOS.h"

#include <native_buffer/native_buffer.h>
#include <native_window/external_window.h>

namespace rx
{
NativeBufferImageSiblingOHOS::NativeBufferImageSiblingOHOS(EGLClientBuffer buffer,
                                                           const egl::AttributeMap &attribs)
    : mBuffer(buffer), mFormat(GL_NONE), mYUV(false), mColorSpace(EGL_GL_COLORSPACE_LINEAR_KHR)
{
    if (attribs.contains(EGL_GL_COLORSPACE_KHR))
    {
        mColorSpace = attribs.getAsInt(EGL_GL_COLORSPACE_KHR);
    }
}

NativeBufferImageSiblingOHOS::~NativeBufferImageSiblingOHOS() {}

egl::Error NativeBufferImageSiblingOHOS::initialize(const egl::Display *display)
{
    int pixelFormat                    = 0;
    uint64_t usage                     = 0;
    OHNativeWindowBuffer *windowBuffer = reinterpret_cast<OHNativeWindowBuffer *>(mBuffer);

    OH_NativeBuffer *buffer = nullptr;
    int32_t retval          = OH_NativeBuffer_FromNativeWindowBuffer(windowBuffer, &buffer);
    assert(retval == 0);

    OH_NativeBuffer_Config config;
    OH_NativeBuffer_GetConfig(buffer, &config);

    mSize.width  = config.width;
    mSize.height = config.height;
    mSize.depth  = 1;

    switch (config.format)
    {
        case NATIVEBUFFER_PIXEL_FMT_RGB_565:
            mFormat = gl::Format(GL_RGB565);
            break;
        case NATIVEBUFFER_PIXEL_FMT_RGB_888:
        case NATIVEBUFFER_PIXEL_FMT_RGBX_8888:
            mFormat = gl::Format(GL_RGB8);
            break;
        case NATIVEBUFFER_PIXEL_FMT_RGBA_8888:
            mFormat = gl::Format(GL_RGBA8);
            break;
        case NATIVEBUFFER_PIXEL_FMT_BGRA_8888:
            mFormat = gl::Format(GL_BGRA8_EXT);
            break;
        case NATIVEBUFFER_PIXEL_FMT_RGBA_4444:
        case NATIVEBUFFER_PIXEL_FMT_BGRA_4444:
            mFormat = gl::Format(GL_RGBA4);
            break;
        default:
            assert(false);
    }
    mYUV                 = false;
    mHasProtectedContent = false;

    return egl::NoError();
}

gl::Format NativeBufferImageSiblingOHOS::getFormat() const
{
    return mFormat;
}

bool NativeBufferImageSiblingOHOS::isRenderable(const gl::Context *context) const
{
    return true;
}

bool NativeBufferImageSiblingOHOS::isTexturable(const gl::Context *context) const
{
    return true;
}

bool NativeBufferImageSiblingOHOS::isYUV() const
{
    return mYUV;
}

bool NativeBufferImageSiblingOHOS::hasProtectedContent() const
{
    return mHasProtectedContent;
}

gl::Extents NativeBufferImageSiblingOHOS::getSize() const
{
    return mSize;
}

size_t NativeBufferImageSiblingOHOS::getSamples() const
{
    return 0;
}

EGLClientBuffer NativeBufferImageSiblingOHOS::getBuffer() const
{
    return mBuffer;
}

void NativeBufferImageSiblingOHOS::getImageCreationAttributes(
    std::vector<EGLint> *outAttributes) const
{
    if (mColorSpace != EGL_GL_COLORSPACE_LINEAR_KHR)
    {
        outAttributes->push_back(EGL_GL_COLORSPACE_KHR);
        outAttributes->push_back(mColorSpace);
    }
}

}  // namespace rx
