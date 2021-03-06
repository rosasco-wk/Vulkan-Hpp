// Copyright(c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// VulkanHpp Samples : DynamicUniform
//                     Draw 2 Cubes using dynamic uniform buffer

#if defined( _MSC_VER )
#  pragma warning( disable : 4127 )  // conditional expression is constant
#endif

#include "../../samples/utils/geometries.hpp"
#include "../../samples/utils/math.hpp"
#include "../utils/shaders.hpp"
#include "../utils/utils.hpp"
#include "SPIRV/GlslangToSpv.h"
#include "vulkan/vulkan_raii.hpp"

#include <iostream>
#include <thread>

static char const * AppName    = "DynamicUniform";
static char const * EngineName = "Vulkan.hpp";

int main( int /*argc*/, char ** /*argv*/ )
{
  try
  {
    std::unique_ptr<vk::raii::Context>  context = vk::raii::su::make_unique<vk::raii::Context>();
    std::unique_ptr<vk::raii::Instance> instance =
      vk::raii::su::makeUniqueInstance( *context, AppName, EngineName, {}, vk::su::getInstanceExtensions() );
#if !defined( NDEBUG )
    std::unique_ptr<vk::raii::DebugUtilsMessengerEXT> debugUtilsMessenger =
      vk::raii::su::makeUniqueDebugUtilsMessengerEXT( *instance );
#endif
    std::unique_ptr<vk::raii::PhysicalDevice> physicalDevice = vk::raii::su::makeUniquePhysicalDevice( *instance );

    vk::raii::su::SurfaceData surfaceData( *instance, AppName, vk::Extent2D( 500, 500 ) );

    std::pair<uint32_t, uint32_t> graphicsAndPresentQueueFamilyIndex =
      vk::raii::su::findGraphicsAndPresentQueueFamilyIndex( *physicalDevice, *surfaceData.surface );
    std::unique_ptr<vk::raii::Device> device = vk::raii::su::makeUniqueDevice(
      *physicalDevice, graphicsAndPresentQueueFamilyIndex.first, vk::su::getDeviceExtensions() );

    std::unique_ptr<vk::raii::CommandPool> commandPool =
      vk::raii::su::makeUniqueCommandPool( *device, graphicsAndPresentQueueFamilyIndex.first );
    std::unique_ptr<vk::raii::CommandBuffer> commandBuffer =
      vk::raii::su::makeUniqueCommandBuffer( *device, *commandPool );

    std::unique_ptr<vk::raii::Queue> graphicsQueue =
      vk::raii::su::make_unique<vk::raii::Queue>( *device, graphicsAndPresentQueueFamilyIndex.first, 0 );
    std::unique_ptr<vk::raii::Queue> presentQueue =
      vk::raii::su::make_unique<vk::raii::Queue>( *device, graphicsAndPresentQueueFamilyIndex.second, 0 );

    vk::raii::su::SwapChainData swapChainData( *physicalDevice,
                                               *device,
                                               *surfaceData.surface,
                                               surfaceData.extent,
                                               vk::ImageUsageFlagBits::eColorAttachment |
                                                 vk::ImageUsageFlagBits::eTransferSrc,
                                               {},
                                               graphicsAndPresentQueueFamilyIndex.first,
                                               graphicsAndPresentQueueFamilyIndex.second );

    vk::raii::su::DepthBufferData depthBufferData(
      *physicalDevice, *device, vk::Format::eD16Unorm, surfaceData.extent );

    vk::Format colorFormat =
      vk::su::pickSurfaceFormat( physicalDevice->getSurfaceFormatsKHR( **surfaceData.surface ) ).format;
    std::unique_ptr<vk::raii::RenderPass> renderPass =
      vk::raii::su::makeUniqueRenderPass( *device, colorFormat, depthBufferData.format );

    glslang::InitializeProcess();
    std::unique_ptr<vk::raii::ShaderModule> vertexShaderModule =
      vk::raii::su::makeUniqueShaderModule( *device, vk::ShaderStageFlagBits::eVertex, vertexShaderText_PC_C );
    std::unique_ptr<vk::raii::ShaderModule> fragmentShaderModule =
      vk::raii::su::makeUniqueShaderModule( *device, vk::ShaderStageFlagBits::eFragment, fragmentShaderText_C_C );
    glslang::FinalizeProcess();

    std::vector<std::unique_ptr<vk::raii::Framebuffer>> framebuffers = vk::raii::su::makeUniqueFramebuffers(
      *device, *renderPass, swapChainData.imageViews, depthBufferData.imageView, surfaceData.extent );

    vk::raii::su::BufferData vertexBufferData(
      *physicalDevice, *device, sizeof( coloredCubeData ), vk::BufferUsageFlagBits::eVertexBuffer );
    vk::raii::su::copyToDevice(
      *vertexBufferData.deviceMemory, coloredCubeData, sizeof( coloredCubeData ) / sizeof( coloredCubeData[0] ) );

    /* VULKAN_KEY_START */

    vk::PhysicalDeviceLimits limits = physicalDevice->getProperties().limits;
    if ( limits.maxDescriptorSetUniformBuffersDynamic < 1 )
    {
      std::cout << "No dynamic uniform buffers supported\n";
      exit( -1 );
    }

    /* Set up uniform buffer with 2 transform matrices in it */
    glm::mat4x4 mvpcs[2];
    glm::mat4x4 model = glm::mat4x4( 1.0f );
    glm::mat4x4 view =
      glm::lookAt( glm::vec3( 0.0f, 3.0f, -10.0f ), glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 0.0f, -1.0f, 0.0f ) );
    glm::mat4x4 projection = glm::perspective( glm::radians( 45.0f ), 1.0f, 0.1f, 100.0f );
    // clang-format off
    glm::mat4x4 clip = glm::mat4x4( 1.0f,  0.0f, 0.0f, 0.0f,
                                    0.0f, -1.0f, 0.0f, 0.0f,
                                    0.0f,  0.0f, 0.5f, 0.0f,
                                    0.0f,  0.0f, 0.5f, 1.0f );  // vulkan clip space has inverted y and half z !
    // clang-format on
    mvpcs[0] = clip * projection * view * model;

    model    = glm::translate( model, glm::vec3( -1.5f, 1.5f, -1.5f ) );
    mvpcs[1] = clip * projection * view * model;

    vk::DeviceSize bufferSize = sizeof( glm::mat4x4 );
    if ( limits.minUniformBufferOffsetAlignment )
    {
      bufferSize =
        ( bufferSize + limits.minUniformBufferOffsetAlignment - 1 ) & ~( limits.minUniformBufferOffsetAlignment - 1 );
    }

    vk::raii::su::BufferData uniformBufferData(
      *physicalDevice, *device, 2 * bufferSize, vk::BufferUsageFlagBits::eUniformBuffer );
    vk::raii::su::copyToDevice( *uniformBufferData.deviceMemory, mvpcs, 2, bufferSize );

    // create a DescriptorSetLayout with vk::DescriptorType::eUniformBufferDynamic
    std::unique_ptr<vk::raii::DescriptorSetLayout> descriptorSetLayout = vk::raii::su::makeUniqueDescriptorSetLayout(
      *device, { { vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eVertex } } );
    std::unique_ptr<vk::raii::PipelineLayout> pipelineLayout =
      vk::raii::su::makeUniquePipelineLayout( *device, *descriptorSetLayout );

    // create a DescriptorPool with vk::DescriptorType::eUniformBufferDynamic
    std::unique_ptr<vk::raii::DescriptorPool> descriptorPool =
      vk::raii::su::makeUniqueDescriptorPool( *device, { { vk::DescriptorType::eUniformBufferDynamic, 1 } } );
    std::unique_ptr<vk::raii::DescriptorSet> descriptorSet =
      vk::raii::su::makeUniqueDescriptorSet( *device, *descriptorPool, *descriptorSetLayout );

    vk::raii::su::updateDescriptorSets(
      *device,
      *descriptorSet,
      { { vk::DescriptorType::eUniformBufferDynamic, *uniformBufferData.buffer, nullptr } },
      {} );

    std::unique_ptr<vk::raii::PipelineCache> pipelineCache =
      vk::raii::su::make_unique<vk::raii::PipelineCache>( *device, vk::PipelineCacheCreateInfo() );
    std::unique_ptr<vk::raii::Pipeline> graphicsPipeline = vk::raii::su::makeUniqueGraphicsPipeline(
      *device,
      *pipelineCache,
      *vertexShaderModule,
      nullptr,
      *fragmentShaderModule,
      nullptr,
      sizeof( coloredCubeData[0] ),
      { { vk::Format::eR32G32B32A32Sfloat, 0 }, { vk::Format::eR32G32Sfloat, 16 } },
      vk::FrontFace::eClockwise,
      true,
      *pipelineLayout,
      *renderPass );

    // Get the index of the next available swapchain image:
    std::unique_ptr<vk::raii::Semaphore> imageAcquiredSemaphore =
      vk::raii::su::make_unique<vk::raii::Semaphore>( *device, vk::SemaphoreCreateInfo() );
    vk::Result result;
    uint32_t   imageIndex;
    std::tie( result, imageIndex ) =
      swapChainData.swapChain->acquireNextImage( vk::su::FenceTimeout, **imageAcquiredSemaphore );
    assert( result == vk::Result::eSuccess );
    assert( imageIndex < swapChainData.images.size() );

    commandBuffer->begin( {} );

    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color        = vk::ClearColorValue( std::array<float, 4>( { { 0.2f, 0.2f, 0.2f, 0.2f } } ) );
    clearValues[1].depthStencil = vk::ClearDepthStencilValue( 1.0f, 0 );
    vk::RenderPassBeginInfo renderPassBeginInfo(
      **renderPass, **framebuffers[imageIndex], vk::Rect2D( vk::Offset2D( 0, 0 ), surfaceData.extent ), clearValues );
    commandBuffer->beginRenderPass( renderPassBeginInfo, vk::SubpassContents::eInline );
    commandBuffer->bindPipeline( vk::PipelineBindPoint::eGraphics, **graphicsPipeline );

    commandBuffer->setViewport( 0,
                                vk::Viewport( 0.0f,
                                              0.0f,
                                              static_cast<float>( surfaceData.extent.width ),
                                              static_cast<float>( surfaceData.extent.height ),
                                              0.0f,
                                              1.0f ) );
    commandBuffer->setScissor( 0, vk::Rect2D( vk::Offset2D( 0, 0 ), surfaceData.extent ) );

    /* The first draw should use the first matrix in the buffer */
    uint32_t dynamicOffset = 0;
    commandBuffer->bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, **pipelineLayout, 0, { **descriptorSet }, dynamicOffset );

    commandBuffer->bindVertexBuffers( 0, { **vertexBufferData.buffer }, { 0 } );
    commandBuffer->draw( 12 * 3, 1, 0, 0 );

    // the second draw should use the second matrix in the buffer;
    dynamicOffset = (uint32_t)bufferSize;
    commandBuffer->bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, **pipelineLayout, 0, { **descriptorSet }, dynamicOffset );
    commandBuffer->draw( 12 * 3, 1, 0, 0 );

    commandBuffer->endRenderPass();
    commandBuffer->end();

    std::unique_ptr<vk::raii::Fence> drawFence = vk::raii::su::make_unique<vk::raii::Fence>( *device, vk::FenceCreateInfo() );

    vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
    vk::SubmitInfo         submitInfo( **imageAcquiredSemaphore, waitDestinationStageMask, **commandBuffer );
    graphicsQueue->submit( submitInfo, **drawFence );

    while ( vk::Result::eTimeout == device->waitForFences( { **drawFence }, VK_TRUE, vk::su::FenceTimeout ) )
      ;

    vk::PresentInfoKHR presentInfoKHR( nullptr, **swapChainData.swapChain, imageIndex );
    result = presentQueue->presentKHR( presentInfoKHR );
    switch ( result )
    {
      case vk::Result::eSuccess: break;
      case vk::Result::eSuboptimalKHR:
        std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
        break;
      default: assert( false );  // an unexpected result is returned !
    }
    std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );

    /* VULKAN_KEY_END */
  }
  catch ( vk::SystemError & err )
  {
    std::cout << "vk::SystemError: " << err.what() << std::endl;
    exit( -1 );
  }
  catch ( std::exception & err )
  {
    std::cout << "std::exception: " << err.what() << std::endl;
    exit( -1 );
  }
  catch ( ... )
  {
    std::cout << "unknown error\n";
    exit( -1 );
  }
  return 0;
}
