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
// VulkanHpp Samples : PushConstants
//                     Use push constants in a simple shader, validate the correct value was read.

#if defined( _MSC_VER )
// no need to ignore any warnings with MSVC
#elif defined( __clang__ )
#  pragma clang diagnostic ignored "-Wmissing-braces"
#elif defined( __GNUC__ )
#else
// unknow compiler... just ignore the warnings for yourselves ;)
#endif

#include "../../samples/utils/geometries.hpp"
#include "../../samples/utils/math.hpp"
#include "../utils/shaders.hpp"
#include "../utils/utils.hpp"
#include "SPIRV/GlslangToSpv.h"
#include "vulkan/vulkan.hpp"

#include <iostream>
#include <thread>

static char const * AppName    = "PushConstants";
static char const * EngineName = "Vulkan.hpp";

const std::string fragmentShaderText = R"(
#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (push_constant) uniform pushBlock
{
  int   iFoo;
  float fBar;
} pushConstantsBlock;

layout (location = 0) in vec2 inTexCoords;

layout (location = 0) out vec4 outColor;

void main()
{
  vec4 green = vec4(0.0f, 1.0f, 0.0f, 1.0f);
  vec4 red   = vec4(1.0f, 0.0f, 0.0f, 1.0f);

  // Start with passing color
  vec4 resColor = green;

  // See if we've read in the correct push constants
  if ((pushConstantsBlock.iFoo != 2) || (pushConstantsBlock.fBar != 1.0f))
  {
    resColor = red;
  }

  // Create a border to see the cube more easily
  if ((inTexCoords.x < 0.01f) || (0.99f < inTexCoords.x)
   || (inTexCoords.y < 0.01f) || (0.99f < inTexCoords.y))
  {
    resColor *= vec4(0.1f, 0.1f, 0.1f, 1.0f);
  }

  outColor = resColor;
}
)";

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

    vk::raii::su::BufferData uniformBufferData(
      *physicalDevice, *device, sizeof( glm::mat4x4 ), vk::BufferUsageFlagBits::eUniformBuffer );
    glm::mat4x4 mvpcMatrix = vk::su::createModelViewProjectionClipMatrix( surfaceData.extent );
    vk::raii::su::copyToDevice( *uniformBufferData.deviceMemory, mvpcMatrix );

    vk::Format colorFormat =
      vk::su::pickSurfaceFormat( physicalDevice->getSurfaceFormatsKHR( **surfaceData.surface ) ).format;
    std::unique_ptr<vk::raii::RenderPass> renderPass =
      vk::raii::su::makeUniqueRenderPass( *device, colorFormat, depthBufferData.format );

    glslang::InitializeProcess();
    std::unique_ptr<vk::raii::ShaderModule> vertexShaderModule =
      vk::raii::su::makeUniqueShaderModule( *device, vk::ShaderStageFlagBits::eVertex, vertexShaderText_PT_T );
    std::unique_ptr<vk::raii::ShaderModule> fragmentShaderModule =
      vk::raii::su::makeUniqueShaderModule( *device, vk::ShaderStageFlagBits::eFragment, fragmentShaderText );
    glslang::FinalizeProcess();

    std::vector<std::unique_ptr<vk::raii::Framebuffer>> framebuffers = vk::raii::su::makeUniqueFramebuffers(
      *device, *renderPass, swapChainData.imageViews, depthBufferData.imageView, surfaceData.extent );

    vk::raii::su::BufferData vertexBufferData(
      *physicalDevice, *device, sizeof( texturedCubeData ), vk::BufferUsageFlagBits::eVertexBuffer );
    vk::raii::su::copyToDevice(
      *vertexBufferData.deviceMemory, texturedCubeData, sizeof( texturedCubeData ) / sizeof( texturedCubeData[0] ) );

    // Create binding and layout for the following, matching contents of shader
    //   binding 0 = uniform buffer (MVP)
    std::unique_ptr<vk::raii::DescriptorSetLayout> descriptorSetLayout = vk::raii::su::makeUniqueDescriptorSetLayout(
      *device, { { vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex } } );

    /* VULKAN_KEY_START */

    // Set up our push constant range, which mirrors the declaration of
    vk::PushConstantRange                     pushConstantRanges( vk::ShaderStageFlagBits::eFragment, 0, 8 );
    vk::PipelineLayoutCreateInfo              pipelineLayoutCreateInfo( {}, **descriptorSetLayout, pushConstantRanges );
    std::unique_ptr<vk::raii::PipelineLayout> pipelineLayout =
      vk::raii::su::make_unique<vk::raii::PipelineLayout>( *device, pipelineLayoutCreateInfo );

    // Create a single pool to contain data for our descriptor set
    std::array<vk::DescriptorPoolSize, 2> poolSizes = { vk::DescriptorPoolSize( vk::DescriptorType::eUniformBuffer, 1 ),
                                                        vk::DescriptorPoolSize(
                                                          vk::DescriptorType::eCombinedImageSampler, 1 ) };
    vk::DescriptorPoolCreateInfo          descriptorPoolCreateInfo(
      vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, poolSizes );
    std::unique_ptr<vk::raii::DescriptorPool> descriptorPool =
      vk::raii::su::make_unique<vk::raii::DescriptorPool>( *device, descriptorPoolCreateInfo );

    // Populate descriptor sets
    vk::DescriptorSetAllocateInfo            descriptorSetAllocateInfo( **descriptorPool, **descriptorSetLayout );
    std::unique_ptr<vk::raii::DescriptorSet> descriptorSet = vk::raii::su::make_unique<vk::raii::DescriptorSet>(
      std::move( vk::raii::DescriptorSets( *device, descriptorSetAllocateInfo ).front() ) );

    // Populate with info about our uniform buffer for MVP
    vk::DescriptorBufferInfo bufferInfo( **uniformBufferData.buffer, 0, sizeof( glm::mat4x4 ) );
    vk::WriteDescriptorSet   writeDescriptorSet(
      **descriptorSet, 0, 0, vk::DescriptorType::eUniformBuffer, {}, bufferInfo );
    device->updateDescriptorSets( writeDescriptorSet, nullptr );

    // Create our push constant data, which matches shader expectations
    std::array<unsigned, 2> pushConstants = { { (unsigned)2, (unsigned)0x3F800000 } };

    // Ensure we have enough room for push constant data
    assert( ( sizeof( pushConstants ) <= physicalDevice->getProperties().limits.maxPushConstantsSize ) &&
            "Too many push constants" );
    commandBuffer->begin( vk::CommandBufferBeginInfo() );
    commandBuffer->pushConstants<unsigned>( **pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, pushConstants );

    /* VULKAN_KEY_END */

    std::unique_ptr<vk::raii::PipelineCache> pipelineCache =
      vk::raii::su::make_unique<vk::raii::PipelineCache>( *device, vk::PipelineCacheCreateInfo() );
    std::unique_ptr<vk::raii::Pipeline> graphicsPipeline = vk::raii::su::makeUniqueGraphicsPipeline(
      *device,
      *pipelineCache,
      *vertexShaderModule,
      nullptr,
      *fragmentShaderModule,
      nullptr,
      sizeof( texturedCubeData[0] ),
      { { vk::Format::eR32G32B32A32Sfloat, 0 }, { vk::Format::eR32G32Sfloat, 16 } },
      vk::FrontFace::eClockwise,
      true,
      *pipelineLayout,
      *renderPass );

    std::unique_ptr<vk::raii::Semaphore> imageAcquiredSemaphore =
      vk::raii::su::make_unique<vk::raii::Semaphore>( *device, vk::SemaphoreCreateInfo() );
    vk::Result result;
    uint32_t   imageIndex;
    std::tie( result, imageIndex ) =
      swapChainData.swapChain->acquireNextImage( vk::su::FenceTimeout, **imageAcquiredSemaphore );
    assert( result == vk::Result::eSuccess );
    assert( imageIndex < swapChainData.images.size() );

    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color        = vk::ClearColorValue( std::array<float, 4>( { { 0.2f, 0.2f, 0.2f, 0.2f } } ) );
    clearValues[1].depthStencil = vk::ClearDepthStencilValue( 1.0f, 0 );

    vk::RenderPassBeginInfo renderPassBeginInfo(
      **renderPass, **framebuffers[imageIndex], vk::Rect2D( vk::Offset2D( 0, 0 ), surfaceData.extent ), clearValues );
    commandBuffer->beginRenderPass( renderPassBeginInfo, vk::SubpassContents::eInline );
    commandBuffer->bindPipeline( vk::PipelineBindPoint::eGraphics, **graphicsPipeline );
    commandBuffer->bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, **pipelineLayout, 0, { **descriptorSet }, nullptr );

    commandBuffer->bindVertexBuffers( 0, { **vertexBufferData.buffer }, { 0 } );
    commandBuffer->setViewport( 0,
                                vk::Viewport( 0.0f,
                                              0.0f,
                                              static_cast<float>( surfaceData.extent.width ),
                                              static_cast<float>( surfaceData.extent.height ),
                                              0.0f,
                                              1.0f ) );
    commandBuffer->setScissor( 0, vk::Rect2D( vk::Offset2D( 0, 0 ), surfaceData.extent ) );

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
