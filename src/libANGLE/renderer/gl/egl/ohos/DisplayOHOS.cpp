//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayOHOS.cpp: Android implementation of egl::Display

#include "libANGLE/renderer/gl/egl/ohos/DisplayOHOS.h"

// #include <android/log.h>
#include <native_window/external_window.h>

#include "libANGLE/Display.h"
#include "libANGLE/renderer/gl/egl/ohos/NativeBufferImageSiblingOHOS.h"

#define EGL_NATIVE_BUFFER_OHOS 0x34E1

namespace rx
{

DisplayOHOS::DisplayOHOS(const egl::DisplayState &state) : DisplayEGL(state) {}

DisplayOHOS::~DisplayOHOS() {}

bool DisplayOHOS::isValidNativeWindow(EGLNativeWindowType window) const
{
    uint64_t surface_id = 0;
    return OH_NativeWindow_GetSurfaceId(reinterpret_cast<OHNativeWindow *>(window), &surface_id) ==
           0;
}

egl::Error DisplayOHOS::validateImageClientBuffer(const gl::Context *context,
                                                  EGLenum target,
                                                  EGLClientBuffer clientBuffer,
                                                  const egl::AttributeMap &attribs) const
{
    switch (target)
    {
        case EGL_NATIVE_BUFFER_OHOS:
            return egl::NoError();

        default:
            return DisplayEGL::validateImageClientBuffer(context, target, clientBuffer, attribs);
    }
}

ExternalImageSiblingImpl *DisplayOHOS::createExternalImageSibling(const gl::Context *context,
                                                                  EGLenum target,
                                                                  EGLClientBuffer buffer,
                                                                  const egl::AttributeMap &attribs)
{
    switch (target)
    {
        case EGL_NATIVE_BUFFER_OHOS:
            return new NativeBufferImageSiblingOHOS(buffer, attribs);

        default:
            return DisplayEGL::createExternalImageSibling(context, target, buffer, attribs);
    }
}

}  // namespace rx
