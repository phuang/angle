//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// NativeBufferImageSiblingOHOS.h: Defines the NativeBufferImageSiblingOHOS to wrap EGL images
// created from ANativeWindowBuffer objects

#ifndef LIBANGLE_RENDERER_GL_EGL_OHOS_NATIVEBUFFERIMAGESIBLINGOHOS_H_
#define LIBANGLE_RENDERER_GL_EGL_OHOS_NATIVEBUFFERIMAGESIBLINGOHOS_H_

#include "libANGLE/renderer/gl/egl/ExternalImageSiblingEGL.h"

namespace rx
{

class NativeBufferImageSiblingOHOS : public ExternalImageSiblingEGL
{
  public:
    NativeBufferImageSiblingOHOS(EGLClientBuffer buffer, const egl::AttributeMap &attribs);
    ~NativeBufferImageSiblingOHOS() override;

    egl::Error initialize(const egl::Display *display) override;

    // ExternalImageSiblingImpl interface
    gl::Format getFormat() const override;
    bool isRenderable(const gl::Context *context) const override;
    bool isTexturable(const gl::Context *context) const override;
    bool isYUV() const override;
    bool hasProtectedContent() const override;
    gl::Extents getSize() const override;
    size_t getSamples() const override;

    // ExternalImageSiblingEGL interface
    EGLClientBuffer getBuffer() const override;
    void getImageCreationAttributes(std::vector<EGLint> *outAttributes) const override;

  private:
    EGLClientBuffer mBuffer;
    gl::Extents mSize;
    gl::Format mFormat;
    bool mYUV;
    bool mHasProtectedContent;
    GLint mColorSpace;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EGL_OHOS_NATIVEBUFFERIMAGESIBLINGOHOS_H_
