//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// NativeBufferImageSiblingVkOHOS.cpp: Implements NativeBufferImageSiblingVkOHOS.

#include "libANGLE/renderer/vulkan/ohos/NativeBufferImageSiblingVkOHOS.h"

#include <native_buffer/native_buffer.h>

// #include "common/android_util.h"

#include "libANGLE/Display.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/ohos/DisplayVkOHOS.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"

namespace rx
{

namespace
{
VkImageTiling OHNBUsageToVkImageTiling(const OH_NativeBuffer_Config &config)
{
    // A note about the choice of OPTIMAL here.

    // When running OHOS on certain GPUs, there are problems creating Vulkan
    // image siblings of AHardwareBuffers because it's currently assumed that
    // the underlying driver can create linear tiling images that have input
    // attachment usage, which isn't supported on NVIDIA for example, resulting
    // in failure to create the image siblings. Yet, we don't currently take
    // advantage of linear elsewhere in ANGLE. To maintain maximum
    // compatibility on OHOS for such drivers, use optimal tiling for image
    // siblings.
    //
    // Note that while we have switched to optimal unconditionally in this path
    // versus linear, it's possible that previously compatible linear usages
    // might become uncompatible after switching to optimal. However, from what
    // we've seen on Samsung/NVIDIA/Intel/AMD GPUs so far, formats generally
    // have more possible usages in optimal tiling versus linear tiling:
    //
    // http://vulkan.gpuinfo.org/displayreport.php?id=10804#formats_linear
    // http://vulkan.gpuinfo.org/displayreport.php?id=10804#formats_optimal
    //
    // http://vulkan.gpuinfo.org/displayreport.php?id=10807#formats_linear
    // http://vulkan.gpuinfo.org/displayreport.php?id=10807#formats_optimal
    //
    // http://vulkan.gpuinfo.org/displayreport.php?id=10809#formats_linear
    // http://vulkan.gpuinfo.org/displayreport.php?id=10809#formats_optimal
    //
    // http://vulkan.gpuinfo.org/displayreport.php?id=10787#formats_linear
    // http://vulkan.gpuinfo.org/displayreport.php?id=10787#formats_optimal
    //
    // Also, as an aside, in terms of what's generally expected from the Vulkan
    // ICD in OHOS when determining AHB compatibility, if the vendor wants
    // to declare a particular combination of format/tiling/usage/etc as not
    // supported AHB-wise, it's up to the ICD vendor to zero out bits in
    // supportedHandleTypes in the vkGetPhysicalDeviceImageFormatProperties2
    // query:
    //
    // ``` *
    // [VUID-VkImageCreateInfo-pNext-00990](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#VUID-VkImageCreateInfo-pNext-00990)
    // If the pNext chain includes a VkExternalMemoryImageCreateInfo structure,
    // its handleTypes member must only contain bits that are also in
    // VkExternalImageFormatProperties::externalMemoryProperties.compatibleHandleTypes,
    // as returned by vkGetPhysicalDeviceImageFormatProperties2 with format,
    // imageType, tiling, usage, and flags equal to those in this structure,
    // and with a VkPhysicalDeviceExternalImageFormatInfo structure included in
    // the pNext chain, with a handleType equal to any one of the handle types
    // specified in VkExternalMemoryImageCreateInfo::handleTypes ```

    return VK_IMAGE_TILING_OPTIMAL;
}

// Map AHB usage flags to VkImageUsageFlags using this table from the Vulkan spec
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/chap11.html#memory-external-android-hardware-buffer-usage
VkImageUsageFlags OHNBConfigUsageToVkImageUsage(const OH_NativeBuffer_Config &config,
                                                bool isDepthOrStencilFormat,
                                                bool isExternal)
{
    VkImageUsageFlags usage = 0;

    if (!isExternal)
    {
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if ((config.usage & NATIVEBUFFER_USAGE_HW_TEXTURE) != 0)
    {
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }

    if ((config.usage & NATIVEBUFFER_USAGE_HW_RENDER) != 0)
    {
        if (isDepthOrStencilFormat)
        {
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        else
        {
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }

    return usage;
}

// Map AHB usage flags to VkImageCreateFlags using this table from the Vulkan spec
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/chap11.html#memory-external-android-hardware-buffer-usage
VkImageCreateFlags OHNBConfigUsageToVkImageCreateFlags(const OH_NativeBuffer_Config &config)
{
    VkImageCreateFlags imageCreateFlags = vk::kVkImageCreateFlagsNone;

    // if ((config.usage & AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP) != 0)
    // {
    //     imageCreateFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    // }

    // if ((config.usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT) != 0)
    // {
    //     imageCreateFlags |= VK_IMAGE_CREATE_PROTECTED_BIT;
    // }

    return imageCreateFlags;
}

// Deduce texture type based on AHB usage flags and layer count
gl::TextureType OHNBConfigUsageToTextureType(const OH_NativeBuffer_Config &config,
                                             const uint32_t layerCount)
{
    gl::TextureType textureType = layerCount > 1 ? gl::TextureType::_2DArray : gl::TextureType::_2D;
    // if ((ahbDescription.usage & AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP) != 0)
    // {
    //     textureType = layerCount > gl::kCubeFaceCount ? gl::TextureType::CubeMapArray
    //                                                   : gl::TextureType::CubeMap;
    // }
    return textureType;
}

GLenum GetPixelFormatInfo(int pixelFormat, bool *isYUV)
{
    *isYUV = false;
    switch (pixelFormat)
    {
        case NATIVEBUFFER_PIXEL_FMT_RGBA_8888:
            return GL_RGBA8;
        case NATIVEBUFFER_PIXEL_FMT_RGBX_8888:
            return GL_RGB8;
        case NATIVEBUFFER_PIXEL_FMT_RGB_888:
            return GL_RGB8;
        case NATIVEBUFFER_PIXEL_FMT_RGB_565:
            return GL_RGB565;
        case NATIVEBUFFER_PIXEL_FMT_BGRA_8888:
            return GL_BGRA8_EXT;
        case NATIVEBUFFER_PIXEL_FMT_BGRA_5551:
            return GL_RGB5_A1;
        case NATIVEBUFFER_PIXEL_FMT_BGRA_4444:
            return GL_RGBA4;
        // case NATIVEBUFFER_PIXEL_FMT_RGBA16_FLOAT:
        //     return GL_RGBA16F;
        case NATIVEBUFFER_PIXEL_FMT_RGBA_1010102:
            return GL_RGB10_A2;
        // case NATIVEBUFFER_PIXEL_FMT_BLOB:
        //     return GL_NONE;
        // case ANGLE_AHB_FORMAT_D16_UNORM:
        //     return GL_DEPTH_COMPONENT16;
        // case ANGLE_AHB_FORMAT_D24_UNORM:
        //     return GL_DEPTH_COMPONENT24;
        // case ANGLE_AHB_FORMAT_D24_UNORM_S8_UINT:
        //     return GL_DEPTH24_STENCIL8;
        // case ANGLE_AHB_FORMAT_D32_FLOAT:
        //     return GL_DEPTH_COMPONENT32F;
        // case ANGLE_AHB_FORMAT_D32_FLOAT_S8_UINT:
        //     return GL_DEPTH32F_STENCIL8;
        // case ANGLE_AHB_FORMAT_S8_UINT:
        //     return GL_STENCIL_INDEX8;
        // case ANGLE_AHB_FORMAT_R8_UNORM:
        //     return GL_R8;
        case NATIVEBUFFER_PIXEL_FMT_YUV_422_I:
        case NATIVEBUFFER_PIXEL_FMT_YCBCR_422_SP:
        case NATIVEBUFFER_PIXEL_FMT_YCRCB_422_SP:
        case NATIVEBUFFER_PIXEL_FMT_YCBCR_420_SP:
        case NATIVEBUFFER_PIXEL_FMT_YCRCB_420_SP:
        case NATIVEBUFFER_PIXEL_FMT_YCBCR_422_P:
        case NATIVEBUFFER_PIXEL_FMT_YCRCB_422_P:
        case NATIVEBUFFER_PIXEL_FMT_YCBCR_420_P:
        case NATIVEBUFFER_PIXEL_FMT_YCRCB_420_P:
        case NATIVEBUFFER_PIXEL_FMT_YUYV_422_PKG:
        case NATIVEBUFFER_PIXEL_FMT_UYVY_422_PKG:
        case NATIVEBUFFER_PIXEL_FMT_YVYU_422_PKG:
        case NATIVEBUFFER_PIXEL_FMT_VYUY_422_PKG:
            *isYUV = true;
            return GL_RGB8;
        default:
            // Treat unknown formats as RGB. They are vendor-specific YUV formats that would sample
            // as RGB.
            *isYUV = true;
            return GL_RGB8;
    }
}
}  // namespace

NativeBufferImageSiblingVkOHOS::NativeBufferImageSiblingVkOHOS(EGLClientBuffer buffer)
    : mBuffer(buffer),
      mNativeBuffer(nullptr),
      mFormat(GL_NONE),
      mRenderable(false),
      mTextureable(false),
      mYUV(false),
      mLevelCount(0),
      mUsage(0),
      mSamples(0),
      mImage(nullptr)
{}

NativeBufferImageSiblingVkOHOS::~NativeBufferImageSiblingVkOHOS() {}

// Static
egl::Error NativeBufferImageSiblingVkOHOS::ValidateNativeBuffer(vk::Renderer *renderer,
                                                                EGLClientBuffer buffer,
                                                                const egl::AttributeMap &attribs)
{
    OHNativeWindowBuffer *windowBuffer = reinterpret_cast<OHNativeWindowBuffer *>(buffer);
    OH_NativeBuffer *nativeBuffer      = nullptr;
    if (windowBuffer != nullptr)
    {
        if (OH_NativeBuffer_FromNativeWindowBuffer(windowBuffer, &nativeBuffer) != 0)
        {
            return egl::Error(EGL_BAD_PARAMETER,
                              "Failed to obtain native buffer through given window buffer.");
        }
    }
    else
    {
        return egl::Error(EGL_BAD_PARAMETER,
                          "Failed to obtain Window buffer through given client buffer handler.");
    }

    VkNativeBufferFormatPropertiesOHOS bufferFormatProperties = {};
    bufferFormatProperties.sType = VK_STRUCTURE_TYPE_NATIVE_BUFFER_FORMAT_PROPERTIES_OHOS;
    bufferFormatProperties.pNext = nullptr;

    VkNativeBufferPropertiesOHOS bufferProperties = {};
    bufferProperties.sType                        = VK_STRUCTURE_TYPE_NATIVE_BUFFER_PROPERTIES_OHOS;
    bufferProperties.pNext                        = nullptr;

    vk::AddToPNextChain(&bufferProperties, &bufferFormatProperties);

    VkDevice device = renderer->getDevice();
    VkResult result = vkGetNativeBufferPropertiesOHOS(device, nativeBuffer, &bufferProperties);
    if (result != VK_SUCCESS)
    {
        return egl::Error(EGL_BAD_PARAMETER, "Failed to query OHNativeBuffer properties");
    }

    OH_NativeBuffer_Config config;
    OH_NativeBuffer_GetConfig(nativeBuffer, &config);

    if (bufferFormatProperties.format == VK_FORMAT_UNDEFINED)
    {
        ASSERT(bufferFormatProperties.externalFormat != 0);
        // We must have an external format, check that it supports texture sampling
        if (!(bufferFormatProperties.formatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
        {
            std::ostringstream err;
            err << "Sampling from AHardwareBuffer externalFormat 0x" << std::hex
                << bufferFormatProperties.externalFormat << " is unsupported.";
            return egl::Error(EGL_BAD_PARAMETER, err.str());
        }
    }
    else
    {
        angle::FormatID formatID = vk::GetFormatIDFromVkFormat(bufferFormatProperties.format);
        const bool hasNecessaryFormatSupport =
            (config.usage & NATIVEBUFFER_USAGE_HW_RENDER) != 0
                ? HasFullTextureFormatSupport(renderer, formatID)
                : HasNonRenderableTextureFormatSupport(renderer, formatID);
        if (!hasNecessaryFormatSupport)
        {
            std::ostringstream err;
            err << "AHardwareBuffer format " << bufferFormatProperties.format
                << " does not support enough features to use as a texture.";
            return egl::Error(EGL_BAD_PARAMETER, err.str());
        }
    }

    if (attribs.getAsInt(EGL_PROTECTED_CONTENT_EXT, EGL_FALSE) == EGL_TRUE)
    {
        return egl::Error(EGL_BAD_ACCESS, "EGL_PROTECTED_CONTENT_EXT is not supported on OHOS");
    }

    return egl::NoError();
}

egl::Error NativeBufferImageSiblingVkOHOS::initialize(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    return angle::ToEGL(initImpl(displayVk), EGL_BAD_PARAMETER);
}

angle::Result NativeBufferImageSiblingVkOHOS::initImpl(DisplayVk *displayVk)
{
    vk::Renderer *renderer = displayVk->getRenderer();

    OHNativeWindowBuffer *windowBuffer = reinterpret_cast<OHNativeWindowBuffer *>(mBuffer);
    OH_NativeBuffer *nativeBuffer      = nullptr;
    OH_NativeBuffer_FromNativeWindowBuffer(windowBuffer, &nativeBuffer);

    OH_NativeBuffer_Config config;
    OH_NativeBuffer_GetConfig(nativeBuffer, &config);
    mSize.width  = config.width;
    mSize.height = config.height;
    mSize.depth  = 1;
    mUsage       = config.usage;

    VkNativeBufferPropertiesOHOS bufferProperties = {};
    bufferProperties.sType                        = VK_STRUCTURE_TYPE_NATIVE_BUFFER_PROPERTIES_OHOS;
    bufferProperties.pNext                        = nullptr;

    VkNativeBufferFormatPropertiesOHOS bufferFormatProperties = {};
    bufferFormatProperties.sType = VK_STRUCTURE_TYPE_NATIVE_BUFFER_FORMAT_PROPERTIES_OHOS;
    bufferFormatProperties.pNext = nullptr;

    vk::AddToPNextChain(&bufferProperties, &bufferFormatProperties);

    VkDevice device = renderer->getDevice();
    ANGLE_VK_TRY(displayVk,
                 vkGetNativeBufferPropertiesOHOS(device, nativeBuffer, &bufferProperties));

    const bool isExternal = bufferFormatProperties.format == VK_FORMAT_UNDEFINED;

    VkExternalFormatOHOS externalFormat = {};
    externalFormat.sType                = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_OHOS;
    externalFormat.externalFormat       = 0;

    // Use bufferFormatProperties.format directly when possible.  For RGBX, the spec requires the
    // corresponding format to be RGB, which is not _technically_ correct.  The Vulkan backend uses
    // the RGBX8_ANGLE format, so that's overriden.
    //
    // Where bufferFormatProperties.format returns UNDEFINED, NativePixelFormatToGLInternalFormat is
    // used to infer the format.
    const vk::Format *vkFormat = nullptr;
    if (config.format == NATIVEBUFFER_PIXEL_FMT_RGBX_8888)
    {
        vkFormat = &renderer->getFormat(GL_RGBX8_ANGLE);
    }
    else if (!isExternal)
    {
        vkFormat = &renderer->getFormat(vk::GetFormatIDFromVkFormat(bufferFormatProperties.format));
    }
    else
    {
        bool isYUV = false;
        GLenum glFormat = GetPixelFormatInfo(config.format, &isYUV);
        vkFormat = &renderer->getFormat(glFormat);
    }

    const angle::Format &imageFormat = vkFormat->getActualRenderableImageFormat();
    bool isDepthOrStencilFormat      = imageFormat.hasDepthOrStencilBits();
    mFormat                          = gl::Format(vkFormat->getIntendedGLFormat());

    bool externalRenderTargetSupported =
        renderer->getFeatures().supportsExternalFormatResolve.enabled && false;
    // Can assume based on us getting here already. The supportsYUVSamplerConversion
    // check below should serve as a backup otherwise.
    bool externalTexturingSupported = true;

    // Query AHB description and do the following -
    // 1. Derive VkImageTiling mode based on AHB usage flags
    // 2. Map AHB usage flags to VkImageUsageFlags
    VkImageTiling imageTilingMode = OHNBUsageToVkImageTiling(config);
    VkImageUsageFlags usage =
        OHNBConfigUsageToVkImageUsage(config, isDepthOrStencilFormat, isExternal);

    if (isExternal)
    {
        ANGLE_VK_CHECK(displayVk, bufferFormatProperties.externalFormat != 0, VK_ERROR_UNKNOWN);
        externalFormat.externalFormat = bufferFormatProperties.externalFormat;

        // VkImageCreateInfo struct: If the pNext chain includes a VkExternalFormatOHOS structure
        // whose externalFormat member is not 0, usage must not include any usages except
        // VK_IMAGE_USAGE_SAMPLED_BIT
        if (externalFormat.externalFormat != 0 && !externalRenderTargetSupported)
        {
            // Clear all other bits except sampled
            usage &= VK_IMAGE_USAGE_SAMPLED_BIT;
        }

        // If the pNext chain includes a VkExternalFormatOHOS structure whose externalFormat
        // member is not 0, tiling must be VK_IMAGE_TILING_OPTIMAL
        imageTilingMode = VK_IMAGE_TILING_OPTIMAL;
    }

    // If forceSampleUsageForAhbBackedImages feature is enabled force enable
    // VK_IMAGE_USAGE_SAMPLED_BIT
    if (renderer->getFeatures().forceSampleUsageForAhbBackedImages.enabled || true)
    {
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {};
    externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageCreateInfo.pNext = &externalFormat;
    externalMemoryImageCreateInfo.handleTypes =
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OHOS_NATIVE_BUFFER_BIT_OHOS;

    VkExtent3D vkExtents;
    gl_vk::GetExtent(mSize, &vkExtents);

    // Setup level count
    mLevelCount = 1;

    // No support for rendering to external YUV AHB with multiple miplevels
    ANGLE_VK_CHECK(displayVk, (!externalRenderTargetSupported || mLevelCount == 1),
                   VK_ERROR_INITIALIZATION_FAILED);

    // Setup layer count
    const uint32_t layerCount = mSize.depth;
    vkExtents.depth           = 1;

    mImage = new vk::ImageHelper();

    // disable robust init for this external image.
    bool robustInitEnabled = false;

    mImage->setTilingMode(imageTilingMode);
    VkImageCreateFlags imageCreateFlags = OHNBConfigUsageToVkImageCreateFlags(config);
    vk::YcbcrConversionDesc conversionDesc{};

    if (isExternal)
    {
        if (externalRenderTargetSupported)
        {
            angle::FormatID externalFormatID =
                renderer->getExternalFormatTable()->getOrAllocExternalFormatID(
                    bufferFormatProperties.externalFormat, VK_FORMAT_UNDEFINED,
                    bufferFormatProperties.formatFeatures);

            vkFormat = &renderer->getFormat(externalFormatID);
        }
        else
        {
            // If not renderable, don't burn a slot on it.
            vkFormat = &renderer->getFormat(angle::FormatID::NONE);
        }
    }

    if (isExternal || imageFormat.isYUV)
    {
        // Note from Vulkan spec: Since GL_OES_EGL_image_external does not require the same sampling
        // and conversion calculations as Vulkan does, achieving identical results between APIs may
        // not be possible on some implementations.
        ANGLE_VK_CHECK(displayVk, renderer->getFeatures().supportsYUVSamplerConversion.enabled,
                       VK_ERROR_FEATURE_NOT_PRESENT);
        ASSERT(externalFormat.pNext == nullptr);

        // This may not actually mean the format is YUV. But the rest of ANGLE makes this
        // assumption and needs this member variable.
        mYUV = true;

        vk::YcbcrLinearFilterSupport linearFilterSupported =
            (bufferFormatProperties.formatFeatures &
             VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT) != 0
                ? vk::YcbcrLinearFilterSupport::Supported
                : vk::YcbcrLinearFilterSupport::Unsupported;

        conversionDesc.update(
            renderer, isExternal ? bufferFormatProperties.externalFormat : 0,
            bufferFormatProperties.suggestedYcbcrModel, bufferFormatProperties.suggestedYcbcrRange,
            bufferFormatProperties.suggestedXChromaOffset,
            bufferFormatProperties.suggestedYChromaOffset, vk::kDefaultYCbCrChromaFilter,
            bufferFormatProperties.samplerYcbcrConversionComponents,
            isExternal ? angle::FormatID::NONE : imageFormat.id, linearFilterSupported);
    }

    const gl::TextureType textureType = OHNBConfigUsageToTextureType(config, layerCount);
    const angle::FormatID actualRenderableFormatID = vkFormat->getActualRenderableImageFormatID();

    VkImageFormatListCreateInfoKHR imageFormatListInfoStorage;
    vk::ImageHelper::ImageListFormats imageListFormatsStorage;

    const void *imageCreateInfoPNext = vk::ImageHelper::DeriveCreateInfoPNext(
        displayVk, usage, actualRenderableFormatID, &externalMemoryImageCreateInfo,
        &imageFormatListInfoStorage, &imageListFormatsStorage, &imageCreateFlags);

    ANGLE_TRY(mImage->initExternal(displayVk, textureType, vkExtents,
                                   vkFormat->getIntendedFormatID(), actualRenderableFormatID, 1,
                                   usage, imageCreateFlags, vk::ImageLayout::ExternalPreInitialized,
                                   imageCreateInfoPNext, gl::LevelIndex(0), mLevelCount, layerCount,
                                   robustInitEnabled, hasProtectedContent(), conversionDesc,
                                   nullptr));

    VkImportNativeBufferInfoOHOS importHardwareBufferInfo = {};
    importHardwareBufferInfo.sType  = VK_STRUCTURE_TYPE_IMPORT_NATIVE_BUFFER_INFO_OHOS;
    importHardwareBufferInfo.buffer = nativeBuffer;

    VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {};
    dedicatedAllocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocInfo.pNext          = &importHardwareBufferInfo;
    dedicatedAllocInfo.image          = mImage->getImage().getHandle();
    dedicatedAllocInfo.buffer         = VK_NULL_HANDLE;
    const void *dedicatedAllocInfoPtr = &dedicatedAllocInfo;

    VkMemoryRequirements externalMemoryRequirements = {};
    externalMemoryRequirements.size                 = bufferProperties.allocationSize;
    externalMemoryRequirements.alignment            = 0;
    externalMemoryRequirements.memoryTypeBits       = bufferProperties.memoryTypeBits;

    const VkMemoryPropertyFlags flags =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        (hasProtectedContent() ? VK_MEMORY_PROPERTY_PROTECTED_BIT : 0);

    ANGLE_TRY(mImage->initExternalMemory(displayVk, renderer->getMemoryProperties(),
                                         externalMemoryRequirements, 1, &dedicatedAllocInfoPtr,
                                         vk::kForeignDeviceQueueIndex, flags));

    if (isExternal)
    {
        // External format means that we are working with VK_FORMAT_UNDEFINED,
        // so hasImageFormatFeatureBits will assert. Set these based on
        // presence of extensions or assumption.
        mRenderable  = externalRenderTargetSupported;
        mTextureable = externalTexturingSupported;
    }
    else
    {
        constexpr uint32_t kColorRenderableRequiredBits = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        constexpr uint32_t kDepthStencilRenderableRequiredBits =
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        mRenderable = renderer->hasImageFormatFeatureBits(actualRenderableFormatID,
                                                          kColorRenderableRequiredBits) ||
                      renderer->hasImageFormatFeatureBits(actualRenderableFormatID,
                                                          kDepthStencilRenderableRequiredBits);
        constexpr uint32_t kTextureableRequiredBits =
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        mTextureable =
            renderer->hasImageFormatFeatureBits(actualRenderableFormatID, kTextureableRequiredBits);
    }

    mNativeBuffer = nativeBuffer;
    OH_NativeBuffer_Reference(mNativeBuffer);

    return angle::Result::Continue;
}

void NativeBufferImageSiblingVkOHOS::onDestroy(const egl::Display *display)
{
    if (mNativeBuffer)
    {
        OH_NativeBuffer_Unreference(mNativeBuffer);
        mNativeBuffer = nullptr;
    }

    ASSERT(mImage == nullptr);
}

gl::Format NativeBufferImageSiblingVkOHOS::getFormat() const
{
    return mFormat;
}

bool NativeBufferImageSiblingVkOHOS::isRenderable(const gl::Context *context) const
{
    return mRenderable;
}

bool NativeBufferImageSiblingVkOHOS::isTexturable(const gl::Context *context) const
{
    return mTextureable;
}

bool NativeBufferImageSiblingVkOHOS::isYUV() const
{
    return mYUV;
}

bool NativeBufferImageSiblingVkOHOS::hasFrontBufferUsage() const
{
    return false;
}

bool NativeBufferImageSiblingVkOHOS::isCubeMap() const
{
    return false;
}

bool NativeBufferImageSiblingVkOHOS::hasProtectedContent() const
{
    return false;
}

gl::Extents NativeBufferImageSiblingVkOHOS::getSize() const
{
    return mSize;
}

size_t NativeBufferImageSiblingVkOHOS::getSamples() const
{
    return mSamples;
}

uint32_t NativeBufferImageSiblingVkOHOS::getLevelCount() const
{
    return mLevelCount;
}

// ExternalImageSiblingVk interface
vk::ImageHelper *NativeBufferImageSiblingVkOHOS::getImage() const
{
    return mImage;
}

void NativeBufferImageSiblingVkOHOS::release(vk::Renderer *renderer)
{
    if (mImage != nullptr)
    {
        // TODO: Handle the case where the EGLImage is used in two contexts not in the same share
        // group.  https://issuetracker.google.com/169868803
        mImage->releaseImage(renderer);
        mImage->releaseStagedUpdates(renderer);
        SafeDelete(mImage);
    }
}

}  // namespace rx
