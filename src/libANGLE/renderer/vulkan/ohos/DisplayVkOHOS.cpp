//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVkOHOS.cpp:
//    Implements the class methods for DisplayVkOHOS.
//

#include "libANGLE/renderer/vulkan/ohos/DisplayVkOHOS.h"

#include <hilog/log.h>
#include <native_window/external_window.h>

#include <vulkan/vulkan.h>

#include "common/angle_version_info.h"
#include "libANGLE/renderer/driver_utils.h"
#include "libANGLE/renderer/vulkan/ohos/NativeBufferImageSiblingVkOHOS.h"
#include "libANGLE/renderer/vulkan/ohos/WindowSurfaceVkOHOS.h"
#include "libANGLE/renderer/vulkan/vk_caps_utils.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"

namespace rx
{

DisplayVkOHOS::DisplayVkOHOS(const egl::DisplayState &state) : DisplayVk(state) {}

egl::Error DisplayVkOHOS::initialize(egl::Display *display)
{
    ANGLE_TRY(DisplayVk::initialize(display));

    std::stringstream strstr;
    strstr << "Version (" << angle::GetANGLEVersionString() << "), ";
    strstr << "Renderer (" << mRenderer->getRendererDescription() << ")";
    OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "ANGLE", "%{public}s", strstr.str().c_str());

    return egl::NoError();
}

bool DisplayVkOHOS::isValidNativeWindow(EGLNativeWindowType window) const
{
    uint64_t surfaceId = 0;
    return (OH_NativeWindow_GetSurfaceId(reinterpret_cast<OHNativeWindow *>(window), &surfaceId) ==
            0);
}

SurfaceImpl *DisplayVkOHOS::createWindowSurfaceVk(const egl::SurfaceState &state,
                                                  EGLNativeWindowType window)
{
    return new WindowSurfaceVkOHOS(state, window);
}

egl::ConfigSet DisplayVkOHOS::generateConfigs()
{
    // ANGLE's Vulkan back-end on OHOS traditionally supports EGLConfig's with GL_RGBA8,
    // GL_RGB8, and GL_RGB565.  The OHOS Vulkan loader used to support all three of these
    // (e.g. OHOS 7), but this has changed as OHOS now supports Vulkan devices that do not
    // support all of those formats.  The loader always supports GL_RGBA8.  Other formats are
    // optionally supported, depending on the underlying driver support.  This includes GL_RGB10_A2
    // and GL_RGBA16F, which ANGLE also desires to support EGLConfig's with.
    //
    // The problem for ANGLE is that Vulkan requires a VkSurfaceKHR in order to query available
    // formats from the loader, but ANGLE must determine which EGLConfig's to expose before it has
    // a VkSurfaceKHR.  The VK_GOOGLE_surfaceless_query extension allows ANGLE to query formats
    // without having a VkSurfaceKHR.  The old path is still kept until this extension becomes
    // universally available.

    // Assume GL_RGB8 and GL_RGBA8 is always available.
    angle::FastVector<GLenum, 5> kColorFormats = {GL_RGBA8, GL_RGB8};
    std::vector<GLenum> kDesiredColorFormats   = {GL_RGB565, GL_RGB10_A2, GL_RGBA16F};
    if (!getFeatures().supportsSurfacelessQueryExtension.enabled)
    {
        // Old path: Assume GL_RGB565 is available, as it is generally available on the devices
        // that support Vulkan.
        kColorFormats.push_back(GL_RGB565);
    }
    else
    {
        // DisplayVk should have already queried and cached supported surface formats.
        for (GLenum glFormat : kDesiredColorFormats)
        {
            VkFormat vkFormat =
                mRenderer->getFormat(glFormat).getActualRenderableImageVkFormat(mRenderer);
            ASSERT(vkFormat != VK_FORMAT_UNDEFINED);
            if (isConfigFormatSupported(vkFormat))
            {
                kColorFormats.push_back(glFormat);
            }
        }
    }

    std::vector<GLenum> depthStencilFormats(
        egl_vk::kConfigDepthStencilFormats,
        egl_vk::kConfigDepthStencilFormats + ArraySize(egl_vk::kConfigDepthStencilFormats));

    if (getCaps().stencil8)
    {
        depthStencilFormats.push_back(GL_STENCIL_INDEX8);
    }
    return egl_vk::GenerateConfigs(kColorFormats.data(), kColorFormats.size(),
                                   depthStencilFormats.data(), depthStencilFormats.size(), this);
}

void DisplayVkOHOS::enableRecordableIfSupported(egl::Config *config)
{
    // TODO(b/181163023): Determine how to properly query for support. This is a hack to unblock
    // launching SwANGLE on Cuttlefish.
    // anglebug.com/42265110: This is also required for app compatiblity.

    const bool isRGBA8888Config = (config->redSize == 8 && config->greenSize == 8 &&
                                   config->blueSize == 8 && config->alphaSize == 8);
    const bool isRGB888Config   = (config->redSize == 8 && config->greenSize == 8 &&
                                 config->blueSize == 8 && config->alphaSize == 0);
    const bool isRGB10A2Config  = (config->redSize == 10 && config->greenSize == 10 &&
                                  config->blueSize == 10 && config->alphaSize == 2);

    // enabled recordable only for RGBA8888, RGB888 and RGB10_A2 configs
    const EGLBoolean enableRecordableBit =
        (isRGBA8888Config || isRGB888Config || isRGB10A2Config) ? EGL_TRUE : EGL_FALSE;

    config->recordable = enableRecordableBit;
}

void DisplayVkOHOS::checkConfigSupport(egl::Config *config)
{
    // TODO(geofflang): Test for native support and modify the config accordingly.
    // anglebug.com/42261400

    enableRecordableIfSupported(config);
}

egl::Error DisplayVkOHOS::validateImageClientBuffer(const gl::Context *context,
                                                    EGLenum target,
                                                    EGLClientBuffer clientBuffer,
                                                    const egl::AttributeMap &attribs) const
{
    switch (target)
    {
        case EGL_NATIVE_BUFFER_OHOS:
            return NativeBufferImageSiblingVkOHOS::ValidateNativeBuffer(mRenderer, clientBuffer,
                                                                        attribs);

        default:
            return DisplayVk::validateImageClientBuffer(context, target, clientBuffer, attribs);
    }
}

ExternalImageSiblingImpl *DisplayVkOHOS::createExternalImageSibling(
    const gl::Context *context,
    EGLenum target,
    EGLClientBuffer buffer,
    const egl::AttributeMap &attribs)
{
    switch (target)
    {
        case EGL_NATIVE_BUFFER_OHOS:
            return new NativeBufferImageSiblingVkOHOS(buffer);

        default:
            return DisplayVk::createExternalImageSibling(context, target, buffer, attribs);
    }
}

const char *DisplayVkOHOS::getWSIExtension() const
{
    return VK_OHOS_SURFACE_EXTENSION_NAME;
}

bool IsVulkanOHOSDisplayAvailable()
{
    return true;
}

DisplayImpl *CreateVulkanOHOSDisplay(const egl::DisplayState &state)
{
    return new DisplayVkOHOS(state);
}
}  // namespace rx
