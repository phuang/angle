//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceVkOHOS.cpp:
//    Implements the class methods for WindowSurfaceVkOHOS.
//

#include "libANGLE/renderer/vulkan/ohos/WindowSurfaceVkOHOS.h"

#include <native_window/external_window.h>

#include "libANGLE/renderer/vulkan/vk_renderer.h"

namespace rx
{

WindowSurfaceVkOHOS::WindowSurfaceVkOHOS(const egl::SurfaceState &surfaceState,
                                         EGLNativeWindowType window)
    : WindowSurfaceVk(surfaceState, window)
{}

angle::Result WindowSurfaceVkOHOS::createSurfaceVk(vk::ErrorContext *context,
                                                   gl::Extents *extentsOut)
{
    VkSurfaceCreateInfoOHOS createInfo = {};
    createInfo.sType  = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS;
    createInfo.flags  = 0;
    createInfo.window = reinterpret_cast<OHNativeWindow*>(mNativeWindowType);
    ASSERT(vkCreateSurfaceOHOS);
    ANGLE_VK_TRY(context, vkCreateSurfaceOHOS(context->getRenderer()->getInstance(), &createInfo,
                                                 nullptr, &mSurface));

    return getCurrentWindowSize(context, extentsOut);
}

angle::Result WindowSurfaceVkOHOS::getCurrentWindowSize(vk::ErrorContext *context,
                                                        gl::Extents *extentsOut)
{
    vk::Renderer *renderer                 = context->getRenderer();
    const VkPhysicalDevice &physicalDevice = renderer->getPhysicalDevice();
    VkSurfaceCapabilitiesKHR surfaceCaps;
    ANGLE_VK_TRY(context,
                 vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface, &surfaceCaps));
    *extentsOut = gl::Extents(surfaceCaps.currentExtent.width, surfaceCaps.currentExtent.height, 1);
    return angle::Result::Continue;
}

}  // namespace rx
