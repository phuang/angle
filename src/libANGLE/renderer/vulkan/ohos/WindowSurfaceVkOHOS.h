//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceVkOHOS.h:
//    Defines the class interface for WindowSurfaceVkOHOS, implementing WindowSurfaceVk.
//

#ifndef LIBANGLE_RENDERER_VULKAN_OHOS_WINDOWSURFACEVKOHOS_H_
#define LIBANGLE_RENDERER_VULKAN_OHOS_WINDOWSURFACEVKOHOS_H_

#include "libANGLE/renderer/vulkan/SurfaceVk.h"

namespace rx
{

class WindowSurfaceVkOHOS : public WindowSurfaceVk
{
  public:
    WindowSurfaceVkOHOS(const egl::SurfaceState &surfaceState, EGLNativeWindowType window);

  private:
    angle::Result createSurfaceVk(vk::ErrorContext *context, gl::Extents *extentsOut) override;
    angle::Result getCurrentWindowSize(vk::ErrorContext *context, gl::Extents *extentsOut) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_OHOS_WINDOWSURFACEVKOHOS_H_
