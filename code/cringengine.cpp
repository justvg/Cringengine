#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <glm.hpp>
#include <ext/quaternion_float.hpp>
#include <ext/quaternion_transform.hpp>
#include <gtc/matrix_transform.hpp>
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat2;
using glm::mat3;
using glm::mat4;
using glm::quat;

#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>
#include <meshoptimizer.h>

#include <stdio.h>
#include <vector>

#define ArrayCount(Arr) (sizeof(Arr)/sizeof((Arr)[0]))
#define Assert(Expr) if(!(Expr)) { *(int *)0 = 0; }
#define OffsetOf(Struct, Member) ((uint64_t)&(((Struct *)0)->Member))
#define VkCheck(Call) \
				{ \
					VkResult Result = Call; \
					Assert(Result == VK_SUCCESS); \
				}

VkInstance CreateInstance()
{
	VkApplicationInfo AppInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	AppInfo.pApplicationName = "Cringengine";
	AppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	AppInfo.pEngineName = "Cringengine";
	AppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	AppInfo.apiVersion = VK_API_VERSION_1_1;

	uint32_t GLFWExtensionCount;
	const char** GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionCount);
	std::vector<const char*> Extensions(GLFWExtensions, GLFWExtensions + GLFWExtensionCount);

	VkInstanceCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	CreateInfo.pApplicationInfo = &AppInfo;
	CreateInfo.enabledExtensionCount = (uint32_t)Extensions.size();
	CreateInfo.ppEnabledExtensionNames = Extensions.data();

	const char* ValidationLayers[] =
	{
		"VK_LAYER_KHRONOS_validation"
	};
	CreateInfo.enabledLayerCount = ArrayCount(ValidationLayers);
	CreateInfo.ppEnabledLayerNames = ValidationLayers;

	VkInstance Instance = 0;
	VkCheck(vkCreateInstance(&CreateInfo, 0, &Instance));
	Assert(Instance);

	return Instance;
}

uint32_t GetGraphicsFamilyIndex(VkPhysicalDevice PhysicalDevice)
{
	uint32_t QueueFamilyPropertyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyPropertyCount, 0);

	std::vector<VkQueueFamilyProperties> QueueFamilyProperties(QueueFamilyPropertyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyPropertyCount, QueueFamilyProperties.data());

	for (uint32_t I = 0; I < QueueFamilyPropertyCount; I++)
		if (QueueFamilyProperties[I].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			return I;

	return VK_QUEUE_FAMILY_IGNORED;
}

bool SupportsPresentation(VkInstance Instance, VkPhysicalDevice PhysicalDevices, uint32_t FamilyIndex)
{
	bool Result = glfwGetPhysicalDevicePresentationSupport(Instance, PhysicalDevices, FamilyIndex);
	return Result;
}

VkPhysicalDevice PickPhysicalDevice(VkInstance Instance)
{
	uint32_t PhysicalDeviceCount = 0;
	VkCheck(vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, 0));

	std::vector<VkPhysicalDevice> PhysicalDevices(PhysicalDeviceCount);
	VkCheck(vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, PhysicalDevices.data()));

	VkPhysicalDevice Discrete = 0;
	VkPhysicalDevice Fallback = 0;
	for (uint32_t I = 0; I < PhysicalDeviceCount; I++)
	{
		VkPhysicalDeviceProperties Properties;
		vkGetPhysicalDeviceProperties(PhysicalDevices[I], &Properties);

		printf("GPU%d: %s\n", I, Properties.deviceName);

		uint32_t GraphicsFamilyIndex = GetGraphicsFamilyIndex(PhysicalDevices[I]);
		if (GraphicsFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
			continue;

		if (!SupportsPresentation(Instance, PhysicalDevices[I], GraphicsFamilyIndex))
			continue;

		if (!Discrete && (Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU))
		{
			Discrete = PhysicalDevices[I];
		}
		else if (!Fallback)
		{
			Fallback = PhysicalDevices[I];
		}
	}

	VkPhysicalDevice PhysicalDevice = Discrete ? Discrete : Fallback;
	if (PhysicalDevice)
	{
		VkPhysicalDeviceProperties Properties;
		vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
		printf("Selected GPU: %s\n", Properties.deviceName);
	}
	else
	{
		printf("ERROR: No GPUs found!\n");
	}

	Assert(PhysicalDevice);
	return PhysicalDevice;
}

VkDevice CreateDevice(VkPhysicalDevice PhysicalDevice, uint32_t FamilyIndex)
{
	VkDeviceQueueCreateInfo QueueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	QueueCreateInfo.queueFamilyIndex = FamilyIndex;
	QueueCreateInfo.queueCount = 1;
	float Priority = 1.0f;
	QueueCreateInfo.pQueuePriorities = &Priority;

	const char* Extensions[] =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkPhysicalDeviceFeatures DeviceFeatures = {};
	DeviceFeatures.multiDrawIndirect = true;

	VkDeviceCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	CreateInfo.queueCreateInfoCount = 1;
	CreateInfo.pQueueCreateInfos = &QueueCreateInfo;
	CreateInfo.enabledExtensionCount = ArrayCount(Extensions);
	CreateInfo.ppEnabledExtensionNames = Extensions;
	CreateInfo.pEnabledFeatures = &DeviceFeatures;

	VkDevice Device = 0;
	VkCheck(vkCreateDevice(PhysicalDevice, &CreateInfo, 0, &Device));
	Assert(Device);

	return Device;
}

VkSurfaceKHR CreateSurface(VkInstance Instance, GLFWwindow* Window)
{
	VkSurfaceKHR Surface = 0;
	VkCheck(glfwCreateWindowSurface(Instance, Window, 0, &Surface));
	Assert(Surface);

	return Surface;
}

VkBool32 SurfaceSupportsPresentation(VkPhysicalDevice PhysicalDevice, uint32_t FamilyIndex, VkSurfaceKHR Surface)
{
	VkBool32 SupportsPresentation = false;
	VkCheck(vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, FamilyIndex, Surface, &SupportsPresentation));

	return SupportsPresentation;
}

VkFormat GetSwapchainFormat(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface)
{
	uint32_t FormatsCount = 0;
	VkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatsCount, 0));

	std::vector<VkSurfaceFormatKHR> Formats(FormatsCount);
	VkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatsCount, Formats.data()));

	if ((FormatsCount == 1) && (Formats[0].format == VK_FORMAT_UNDEFINED))
		return VK_FORMAT_R8G8B8A8_UNORM;

	for (uint32_t I = 0; I < FormatsCount; I++)
		if ((Formats[I].format == VK_FORMAT_R8G8B8A8_UNORM) || (Formats[I].format == VK_FORMAT_B8G8R8A8_UNORM))
			return Formats[I].format;

	return Formats[0].format;
}

VkFormat FindDepthFormat(VkPhysicalDevice PhysicalDevice)
{
	VkFormat Formats[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;

	for (uint32_t I = 0; I < ArrayCount(Formats); I++)
	{
		VkFormatProperties FormatProps;
		vkGetPhysicalDeviceFormatProperties(PhysicalDevice, Formats[I], &FormatProps);

		if ((Tiling == VK_IMAGE_TILING_OPTIMAL) && (FormatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
			return Formats[I];
	}

	Assert(!"Can't find depth format!");
	return VK_FORMAT_UNDEFINED;
}

VkRenderPass CreateRenderPass(VkDevice Device, VkFormat ColorFormat, VkFormat DepthFormat)
{
	VkAttachmentDescription Attachments[2] = {};

	Attachments[0].format = ColorFormat;
	Attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	Attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	Attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	Attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	Attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	Attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Attachments[1].format = DepthFormat;
	Attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	Attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	Attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	Attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	Attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference ColorAttachment = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference DepthAttachment = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription Subpass = {};
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.colorAttachmentCount = 1;
	Subpass.pColorAttachments = &ColorAttachment;
	Subpass.pDepthStencilAttachment = &DepthAttachment;

	VkRenderPassCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	CreateInfo.attachmentCount = ArrayCount(Attachments);
	CreateInfo.pAttachments = Attachments;
	CreateInfo.subpassCount = 1;
	CreateInfo.pSubpasses = &Subpass;

	VkRenderPass RenderPass = 0;
	VkCheck(vkCreateRenderPass(Device, &CreateInfo, 0, &RenderPass));
	Assert(RenderPass);

	return RenderPass;
}

VmaAllocator CreateVulkanMemoryAllocator(VkInstance Instance, VkPhysicalDevice PhysicalDevice, VkDevice Device)
{
	VmaAllocatorCreateInfo CreateInfo = {};
	CreateInfo.instance = Instance;
	CreateInfo.physicalDevice = PhysicalDevice;
	CreateInfo.device = Device;

	VmaAllocator Allocator = 0;
	VkCheck(vmaCreateAllocator(&CreateInfo, &Allocator));
	Assert(Allocator);

	return Allocator;
}

VkImageView CreateImageView(VkDevice Device, VkImage Image, VkFormat Format, VkImageAspectFlags AspectFlags)
{
	VkImageViewCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	CreateInfo.image = Image;
	CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	CreateInfo.format = Format;
	CreateInfo.subresourceRange.aspectMask = AspectFlags;
	CreateInfo.subresourceRange.levelCount = 1;
	CreateInfo.subresourceRange.layerCount = 1;

	VkImageView ImageView = 0;
	VkCheck(vkCreateImageView(Device, &CreateInfo, 0, &ImageView));
	Assert(ImageView);

	return ImageView;
}

struct SSwapchain
{
	VkSwapchainKHR VkSwapchain;
	std::vector<VkImage> Images;
	std::vector<VkImageView> ImageViews;
	std::vector<VkFramebuffer> Framebuffers;

	VkImage DepthImage;
	VmaAllocation DepthImageMemory;
	VkImageView DepthImageView;

	uint32_t Width, Height;
};

VkSwapchainKHR CreateSwapchain(VkDevice Device, VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkFormat Format, const VkSurfaceCapabilitiesKHR& SurfaceCaps, VkSwapchainKHR OldSwapchain = 0)
{
	VkCompositeAlphaFlagBitsKHR CompositeAlpha = (SurfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR :
												 (SurfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR :
												 (SurfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR :
 												  VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

	VkSwapchainCreateInfoKHR CreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	CreateInfo.surface = Surface;
	CreateInfo.minImageCount = std::max(2u, SurfaceCaps.minImageCount);
	CreateInfo.imageFormat = Format;
	CreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	CreateInfo.imageExtent = SurfaceCaps.currentExtent;
	CreateInfo.imageArrayLayers = 1;
	CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	CreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	CreateInfo.compositeAlpha = CompositeAlpha;
	CreateInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;//VK_PRESENT_MODE_FIFO_KHR;
	CreateInfo.oldSwapchain = OldSwapchain;

	VkSwapchainKHR Swapchain = 0;
	VkCheck(vkCreateSwapchainKHR(Device, &CreateInfo, 0, &Swapchain));

	return Swapchain;
}

SSwapchain CreateSwapchain(VkDevice Device, VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkFormat ColorFormat, VkFormat DepthFormat, VkRenderPass RenderPass, VmaAllocator MemoryAllocator, VkSwapchainKHR OldSwapchain = 0)
{
	SSwapchain Swapchain = {};

	VkSurfaceCapabilitiesKHR SurfaceCaps;
	VkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &SurfaceCaps));

	Swapchain.VkSwapchain = CreateSwapchain(Device, PhysicalDevice, Surface, ColorFormat, SurfaceCaps, OldSwapchain);
	Assert(Swapchain.VkSwapchain);

	uint32_t ImageCount = 0;
	VkCheck(vkGetSwapchainImagesKHR(Device, Swapchain.VkSwapchain, &ImageCount, 0));

	std::vector<VkImage> Images(ImageCount);
	VkCheck(vkGetSwapchainImagesKHR(Device, Swapchain.VkSwapchain, &ImageCount, Images.data()));

	std::vector<VkImageView> ImageViews(ImageCount);
	for (uint32_t I = 0; I < ImageCount; I++)
	{
		ImageViews[I] = CreateImageView(Device, Images[I], ColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	VkImageCreateInfo DepthImageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	DepthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	DepthImageCreateInfo.format = DepthFormat;
	DepthImageCreateInfo.extent.width = SurfaceCaps.currentExtent.width;
	DepthImageCreateInfo.extent.height = SurfaceCaps.currentExtent.height;
	DepthImageCreateInfo.extent.depth = 1;
	DepthImageCreateInfo.mipLevels = 1;
	DepthImageCreateInfo.arrayLayers = 1;
	DepthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	DepthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	DepthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	DepthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo DepthAllocationCreateInfo = {};
	DepthAllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkImage DepthImage = 0;
	VmaAllocation DepthImageMemory = 0;
	VkCheck(vmaCreateImage(MemoryAllocator, &DepthImageCreateInfo, &DepthAllocationCreateInfo, &DepthImage, &DepthImageMemory, 0));
	Assert(DepthImage);
	Assert(DepthImageMemory);

	VkImageView DepthImageView = CreateImageView(Device, DepthImage, DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	std::vector<VkFramebuffer> Framebuffers(ImageCount);
	for (uint32_t I = 0; I < ImageCount; I++)
	{
		VkImageView Attachments[] = { ImageViews[I], DepthImageView };

		VkFramebufferCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		CreateInfo.renderPass = RenderPass;
		CreateInfo.attachmentCount = ArrayCount(Attachments);
		CreateInfo.pAttachments = Attachments;
		CreateInfo.width = SurfaceCaps.currentExtent.width;
		CreateInfo.height = SurfaceCaps.currentExtent.height;
		CreateInfo.layers = 1;

		VkCheck(vkCreateFramebuffer(Device, &CreateInfo, 0, &Framebuffers[I]));
	}

	Swapchain.Images = Images;
	Swapchain.ImageViews = ImageViews;
	Swapchain.Framebuffers = Framebuffers;
	Swapchain.DepthImage = DepthImage;
	Swapchain.DepthImageMemory = DepthImageMemory;
	Swapchain.DepthImageView = DepthImageView;
	Swapchain.Width = SurfaceCaps.currentExtent.width;
	Swapchain.Height = SurfaceCaps.currentExtent.height;

	return Swapchain;
}

void DestroySwapchain(SSwapchain Swapchain, VkDevice Device, VmaAllocator MemoryAllocator)
{
	uint32_t ImageCount = (uint32_t)Swapchain.Images.size();
	for (uint32_t I = 0; I < ImageCount; I++)
	{
		vkDestroyFramebuffer(Device, Swapchain.Framebuffers[I], 0);
		vkDestroyImageView(Device, Swapchain.ImageViews[I], 0);
	}

	vkDestroyImageView(Device, Swapchain.DepthImageView, 0);
	vmaDestroyImage(MemoryAllocator, Swapchain.DepthImage, Swapchain.DepthImageMemory);

	vkDestroySwapchainKHR(Device, Swapchain.VkSwapchain, 0);
}

void ResizeSwapchainIfChanged(SSwapchain& Swapchain, VkDevice Device, VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkFormat ColorFormat, VkFormat DepthFormat, VkRenderPass RenderPass, VmaAllocator MemoryAllocator)
{
	VkSurfaceCapabilitiesKHR SurfaceCaps = {};
	VkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &SurfaceCaps));

	while ((SurfaceCaps.currentExtent.width == 0) || (SurfaceCaps.currentExtent.height == 0))
	{
		glfwWaitEvents();

		VkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &SurfaceCaps));
	}

	if ((Swapchain.Width != SurfaceCaps.currentExtent.width) || (Swapchain.Height != SurfaceCaps.currentExtent.height))
	{
		SSwapchain OldSwapchain = Swapchain;
		Swapchain = CreateSwapchain(Device, PhysicalDevice, Surface, ColorFormat, DepthFormat, RenderPass, MemoryAllocator, Swapchain.VkSwapchain);

		VkCheck(vkDeviceWaitIdle(Device));
		DestroySwapchain(OldSwapchain, Device, MemoryAllocator);
	}
}

VkCommandPool CreateCommandPool(VkDevice Device, VkCommandPoolCreateFlags Flags, uint32_t FamilyIndex)
{
	VkCommandPoolCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	CreateInfo.flags = Flags;
	CreateInfo.queueFamilyIndex = FamilyIndex;

	VkCommandPool CommandPool = 0;
	VkCheck(vkCreateCommandPool(Device, &CreateInfo, 0, &CommandPool));
	Assert(CommandPool);

	return CommandPool;
}

void AllocateCommandBuffers(VkDevice Device, VkCommandPool CommandPool, VkCommandBuffer* CommandBuffers, uint32_t Count)
{
	VkCommandBufferAllocateInfo AllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	AllocateInfo.commandPool = CommandPool;
	AllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	AllocateInfo.commandBufferCount = Count;

	VkCheck(vkAllocateCommandBuffers(Device, &AllocateInfo, CommandBuffers));

	for (uint32_t I = 0; I < Count; I++)
	{
		Assert(CommandBuffers[I]);
	}
}

VkSemaphore CreateSemaphore(VkDevice Device)
{
	VkSemaphoreCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkSemaphore Semaphore = 0;
	VkCheck(vkCreateSemaphore(Device, &CreateInfo, 0, &Semaphore));

	return Semaphore;
}

VkQueryPool CreateQueryPool(VkDevice Device)
{
	VkQueryPoolCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	CreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	CreateInfo.queryCount = 2;

	VkQueryPool QueryPool = 0;
	VkCheck(vkCreateQueryPool(Device, &CreateInfo, 0, &QueryPool));
	Assert(QueryPool);

	return QueryPool;
}

VkImageMemoryBarrier CreateImageMemoryBarrier(VkAccessFlags SrcAccessMask, VkAccessFlags DstAccessMask, VkImageLayout OldLayout, VkImageLayout NewLayout, VkImage Image, VkImageAspectFlags AspectMask)
{
	VkImageMemoryBarrier Barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	Barrier.srcAccessMask = SrcAccessMask;
	Barrier.dstAccessMask = DstAccessMask;
	Barrier.oldLayout = OldLayout;
	Barrier.newLayout = NewLayout;
	Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.image = Image;
	Barrier.subresourceRange.aspectMask = AspectMask;
	Barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	Barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	return Barrier;
}

VkShaderModule LoadShader(VkDevice Device, const char* Path)
{
	FILE* File = fopen(Path, "rb");
	Assert(File);

	fseek(File, 0, SEEK_END);
	long FileSize = ftell(File);
	fseek(File, 0, SEEK_SET);

	uint8_t* Buffer = (uint8_t *)malloc(FileSize);
	Assert(Buffer);

	size_t Read = fread(Buffer, FileSize, 1, File);
	fclose(File);

	Assert(FileSize % 4 == 0);

	VkShaderModuleCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	CreateInfo.codeSize = FileSize;
	CreateInfo.pCode = (uint32_t*)Buffer;

	VkShaderModule ShaderModule = 0;
	VkCheck(vkCreateShaderModule(Device, &CreateInfo, 0, &ShaderModule));
	Assert(ShaderModule);

	return ShaderModule;
}

struct SBuffer
{
	VkBuffer Buffer;
	VmaAllocation Allocation;
	void* Data;
};

SBuffer CreateBuffer(VmaAllocator MemoryAllocator, VkDeviceSize Size, VkBufferUsageFlags BufferUsage, VmaMemoryUsage MemoryUsage)
{
	VkBufferCreateInfo BufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	BufferCreateInfo.size = Size;
	BufferCreateInfo.usage = BufferUsage;

	VmaAllocationCreateInfo AllocationCreateInfo = {};
	AllocationCreateInfo.usage = MemoryUsage;

	SBuffer Buffer = {};
	VkCheck(vmaCreateBuffer(MemoryAllocator, &BufferCreateInfo, &AllocationCreateInfo, &Buffer.Buffer, &Buffer.Allocation, 0));
	Assert(Buffer.Buffer);
	Assert(Buffer.Allocation);

	void* Data = 0;
	if ((MemoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU) || (MemoryUsage == VMA_MEMORY_USAGE_CPU_ONLY))
		vmaMapMemory(MemoryAllocator, Buffer.Allocation, &Data);
	Buffer.Data = Data;

	return Buffer;
}

void UploadBuffer(VkDevice Device, VkCommandPool CommandPool, VkCommandBuffer CommandBuffer, VkQueue Queue, const SBuffer& Buffer, const SBuffer& StagingBuffer, void *Data, uint64_t Size)
{
	Assert(StagingBuffer.Data);
	memcpy(StagingBuffer.Data, Data, Size);

	VkCheck(vkResetCommandPool(Device, CommandPool, 0));

	VkCommandBufferBeginInfo BeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkCheck(vkBeginCommandBuffer(CommandBuffer, &BeginInfo));

	VkBufferCopy CopyRegion = { 0, 0, Size };
	vkCmdCopyBuffer(CommandBuffer, StagingBuffer.Buffer, Buffer.Buffer, 1, &CopyRegion);

	VkCheck(vkEndCommandBuffer(CommandBuffer));

	VkSubmitInfo SubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &CommandBuffer;
	VkCheck(vkQueueSubmit(Queue, 1, &SubmitInfo, 0));

	VkCheck(vkDeviceWaitIdle(Device));
}

VkDescriptorPool CreateDescriptorPool(VkDevice Device)
{
	VkDescriptorPoolSize PoolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
	};

	VkDescriptorPoolCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	CreateInfo.maxSets = 10;
	CreateInfo.poolSizeCount = ArrayCount(PoolSizes);
	CreateInfo.pPoolSizes = PoolSizes;

	VkDescriptorPool DescriptorPool = 0;
	VkCheck(vkCreateDescriptorPool(Device, &CreateInfo, 0, &DescriptorPool));
	Assert(DescriptorPool);

	return DescriptorPool;
}

VkDescriptorSet CreateDescriptorSet(VkDevice Device, VkDescriptorPool DescriptorPool, VkDescriptorSetLayout DescriptorSetLayout)
{
	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	DescriptorSetAllocateInfo.descriptorPool = DescriptorPool;
	DescriptorSetAllocateInfo.descriptorSetCount = 1;
	DescriptorSetAllocateInfo.pSetLayouts = &DescriptorSetLayout;

	VkDescriptorSet DescriptorSet = 0;
	VkCheck(vkAllocateDescriptorSets(Device, &DescriptorSetAllocateInfo, &DescriptorSet));
	Assert(DescriptorSet);

	return DescriptorSet;
}

void UpdateDescriptorSet(VkDevice Device, VkDescriptorSet DescriptorSet, uint32_t Binding, VkDescriptorType DescriptorType, SBuffer Buffer, VkDeviceSize BufferRange)
{
	VkDescriptorBufferInfo BufferInfo = {};
	BufferInfo.buffer = Buffer.Buffer;
	BufferInfo.range = BufferRange;

	VkWriteDescriptorSet DescriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	DescriptorWrite.dstSet = DescriptorSet;
	DescriptorWrite.dstBinding = 0;
	DescriptorWrite.descriptorCount = 1;
	DescriptorWrite.descriptorType = DescriptorType;
	DescriptorWrite.pBufferInfo = &BufferInfo;
	
	vkUpdateDescriptorSets(Device, 1, &DescriptorWrite, 0, 0);
}

VkDescriptorSetLayoutBinding CreateDescriptorSetLayoutBinding(uint32_t Binding, VkDescriptorType DescriptorType, VkShaderStageFlags StageFlags)
{
	VkDescriptorSetLayoutBinding SetLayoutBinding = {};
	SetLayoutBinding.binding = Binding;
	SetLayoutBinding.descriptorType = DescriptorType;
	SetLayoutBinding.descriptorCount = 1;
	SetLayoutBinding.stageFlags = StageFlags;

	return SetLayoutBinding;
}

VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice Device, uint32_t SetBindingsCount, const VkDescriptorSetLayoutBinding* SetBindings)
{
	VkDescriptorSetLayoutCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	CreateInfo.bindingCount = SetBindingsCount;
	CreateInfo.pBindings = SetBindings;

	VkDescriptorSetLayout SetLayout = 0;
	VkCheck(vkCreateDescriptorSetLayout(Device, &CreateInfo, 0, &SetLayout));
	Assert(SetLayout);

	return SetLayout;
}

struct SCameraBuffer
{
	mat4 View;
	mat4 Proj;
};

struct SObjectTransform
{
	vec3 Position;
	float Scale;
	quat Orientation;
};

VkPipelineLayout CreatePipelineLayout(VkDevice Device, uint32_t SetLayoutCount, const VkDescriptorSetLayout* SetLayouts)
{
	VkPushConstantRange PushConstantRange = {};
	PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	PushConstantRange.offset = 0;
	PushConstantRange.size = sizeof(SObjectTransform);

	VkPipelineLayoutCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	CreateInfo.setLayoutCount = SetLayoutCount;
	CreateInfo.pSetLayouts = SetLayouts;
	CreateInfo.pushConstantRangeCount = 1;
	CreateInfo.pPushConstantRanges = &PushConstantRange;

	VkPipelineLayout PipelineLayout = 0;
	VkCheck(vkCreatePipelineLayout(Device, &CreateInfo, 0, &PipelineLayout));
	Assert(PipelineLayout);

	return PipelineLayout;
}

struct SVertex
{
	vec3 Position;
	vec3 Normal;
};

struct SMesh
{
	uint32_t VertexOffset;
	uint32_t VertexCount;

	uint32_t IndexOffset;
	uint32_t IndexCount;
};

struct SGeometry
{
	std::vector<SVertex> Vertices;
	std::vector<uint32_t> Indices;
	std::vector<SMesh> Meshes;
};

void LoadMesh(SGeometry& Geometry, const char* Path)
{
	fastObjMesh* File = fast_obj_read(Path);
	Assert(File);

	size_t IndexCount = 0;
	for (uint32_t I = 0; I < File->face_count; I++)
	{
		Assert((File->face_vertices[I] == 3) || (File->face_vertices[I] == 4));
		IndexCount += (size_t)3 * (File->face_vertices[I] - 2);
	}

	size_t VertexOffset = 0;
	size_t IndexOffset = 0;
	std::vector<SVertex> Vertices(IndexCount);
	for (uint32_t I = 0; I < File->face_count; I++)
	{
		for (uint32_t J = 0; J < File->face_vertices[I]; J++)
		{
			if (J >= 3)
			{
				Vertices[VertexOffset] = Vertices[VertexOffset - 3];
				Vertices[VertexOffset + 1] = Vertices[VertexOffset - 1];
				VertexOffset += 2;
			}

			SVertex& Vertex = Vertices[VertexOffset++];

			int PosIndex = File->indices[IndexOffset].p;
			int NorIndex = File->indices[IndexOffset].n;

			float px = File->positions[3 * PosIndex];
			float py = File->positions[3 * PosIndex + 1];
			float pz = File->positions[3 * PosIndex + 2];

			float nx = (NorIndex < 0) ? 0.0f : File->normals[3 * NorIndex];
			float ny = (NorIndex < 0) ? 0.0f : File->normals[3 * NorIndex + 1];
			float nz = (NorIndex < 0) ? 1.0f : File->normals[3 * NorIndex + 2];

			Vertex.Position = vec3(px, py, pz);
			Vertex.Normal = vec3(nx, ny, nz);

			IndexOffset++;
		}
	}
	Assert(VertexOffset == IndexCount);
	fast_obj_destroy(File);

	std::vector<uint32_t> Remap(IndexCount);
	size_t UniqueVertices = meshopt_generateVertexRemap(Remap.data(), 0, IndexCount, Vertices.data(), IndexCount, sizeof(SVertex));

	size_t PrevVertexCount = Geometry.Vertices.size();
	size_t PrevIndexCount = Geometry.Indices.size();
	Geometry.Vertices.resize(PrevVertexCount + UniqueVertices);
	Geometry.Indices.resize(PrevIndexCount + IndexCount);

	meshopt_remapVertexBuffer(Geometry.Vertices.data() + PrevVertexCount, Vertices.data(), IndexCount, sizeof(SVertex), Remap.data());
	meshopt_remapIndexBuffer(Geometry.Indices.data() + PrevIndexCount, 0, IndexCount, Remap.data());

	SMesh Mesh = {};
	Mesh.VertexOffset = PrevVertexCount;
	Mesh.VertexCount = UniqueVertices;
	Mesh.IndexOffset = PrevIndexCount;
	Mesh.IndexCount = IndexCount;

	Geometry.Meshes.push_back(Mesh);
}

VkPipeline CreateGraphicsPipeline(VkDevice Device, VkRenderPass RenderPass, VkPipelineLayout PipelineLayout, VkShaderModule VS, VkShaderModule FS)
{
	VkPipelineShaderStageCreateInfo ShaderStages[2] = {};
	ShaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	ShaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	ShaderStages[0].module = VS;
	ShaderStages[0].pName = "main";
	ShaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	ShaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	ShaderStages[1].module = FS;
	ShaderStages[1].pName = "main";

	VkVertexInputBindingDescription BindingDescr = {};
	BindingDescr.binding = 0;
	BindingDescr.stride = sizeof(SVertex);
	BindingDescr.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription AttributeDescrs[2] = {};
	AttributeDescrs[0].location = 0;
	AttributeDescrs[0].binding = 0;
	AttributeDescrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	AttributeDescrs[0].offset = 0;
	AttributeDescrs[1].location = 1;
	AttributeDescrs[1].binding = 0;
	AttributeDescrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	AttributeDescrs[1].offset = OffsetOf(SVertex, Normal);

	VkPipelineVertexInputStateCreateInfo VertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VertexInputInfo.vertexBindingDescriptionCount = 1;
	VertexInputInfo.pVertexBindingDescriptions = &BindingDescr;
	VertexInputInfo.vertexAttributeDescriptionCount = ArrayCount(AttributeDescrs);
	VertexInputInfo.pVertexAttributeDescriptions = AttributeDescrs;

	VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo ViewportStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	ViewportStateInfo.viewportCount = 1;
	ViewportStateInfo.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo RasterizationStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	RasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	RasterizationStateInfo.cullMode = VK_CULL_MODE_NONE;
	RasterizationStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	RasterizationStateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo MultisampleStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	MultisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo DepthStencilStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	DepthStencilStateInfo.depthTestEnable = VK_TRUE;
	DepthStencilStateInfo.depthWriteEnable = VK_TRUE;
	DepthStencilStateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	DepthStencilStateInfo.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState ColorAttachmentState = {};
	ColorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo ColorBlendStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	ColorBlendStateInfo.attachmentCount = 1;
	ColorBlendStateInfo.pAttachments = &ColorAttachmentState;

	VkDynamicState DynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicStateInfo.dynamicStateCount = ArrayCount(DynamicStates);
	dynamicStateInfo.pDynamicStates = DynamicStates;

	VkGraphicsPipelineCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	CreateInfo.stageCount = ArrayCount(ShaderStages);
	CreateInfo.pStages = ShaderStages;
	CreateInfo.pVertexInputState = &VertexInputInfo;
	CreateInfo.pInputAssemblyState = &InputAssemblyInfo;
	CreateInfo.pViewportState = &ViewportStateInfo;
	CreateInfo.pRasterizationState = &RasterizationStateInfo;
	CreateInfo.pMultisampleState = &MultisampleStateInfo;
	CreateInfo.pDepthStencilState = &DepthStencilStateInfo;
	CreateInfo.pColorBlendState = &ColorBlendStateInfo;
	CreateInfo.pDynamicState = &dynamicStateInfo;
	CreateInfo.layout = PipelineLayout;
	CreateInfo.renderPass = RenderPass;

	VkPipeline GraphicsPipeline = 0;
	VkCheck(vkCreateGraphicsPipelines(Device, 0, 1, &CreateInfo, 0, &GraphicsPipeline));
	Assert(GraphicsPipeline);

	return GraphicsPipeline;
}

int main()
{
	if (glfwInit())
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		GLFWwindow* Window = glfwCreateWindow(1024, 720, "Cringengine", 0, 0);
		if (Window)
		{
			VkInstance Instance = CreateInstance();
			VkPhysicalDevice PhysicalDevice = PickPhysicalDevice(Instance);

			VkPhysicalDeviceProperties PhysicalDeviceProps = {};
			vkGetPhysicalDeviceProperties(PhysicalDevice, &PhysicalDeviceProps);

			uint32_t GraphicsFamilyIndex = GetGraphicsFamilyIndex(PhysicalDevice);
			Assert(GraphicsFamilyIndex != VK_QUEUE_FAMILY_IGNORED);

			VkDevice Device = CreateDevice(PhysicalDevice, GraphicsFamilyIndex);

			VkSurfaceKHR Surface = CreateSurface(Instance, Window);
			Assert(SurfaceSupportsPresentation(PhysicalDevice, GraphicsFamilyIndex, Surface));

			VkQueue GraphicsQueue = 0;
			vkGetDeviceQueue(Device, GraphicsFamilyIndex, 0, &GraphicsQueue);

			VkFormat SwapchainFormat = GetSwapchainFormat(PhysicalDevice, Surface);
			VkFormat DepthFormat = FindDepthFormat(PhysicalDevice);

			VkRenderPass RenderPass = CreateRenderPass(Device, SwapchainFormat, DepthFormat);

			VmaAllocator MemoryAllocator = CreateVulkanMemoryAllocator(Instance, PhysicalDevice, Device);
			SSwapchain Swapchain = CreateSwapchain(Device, PhysicalDevice, Surface, SwapchainFormat, DepthFormat, RenderPass, MemoryAllocator);

			VkCommandPool CommandPool = CreateCommandPool(Device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, GraphicsFamilyIndex);

			VkCommandBuffer CommandBuffer = 0;
			AllocateCommandBuffers(Device, CommandPool, &CommandBuffer, 1);

			VkSemaphore AcquireSemaphore = CreateSemaphore(Device);
			VkSemaphore ReleaseSemaphore = CreateSemaphore(Device);

			VkQueryPool QueryPool = CreateQueryPool(Device);

			SBuffer StagingBuffer = CreateBuffer(MemoryAllocator, 64 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
			SBuffer VertexBuffer = CreateBuffer(MemoryAllocator, 64 * 1024 * 1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
			SBuffer IndexBuffer = CreateBuffer(MemoryAllocator, 64 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
			SBuffer StorageBuffer = CreateBuffer(MemoryAllocator, 64 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
			SBuffer IndirectBuffer = CreateBuffer(MemoryAllocator, 64 * 1024 * 1024, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			VkShaderModule VS = LoadShader(Device, "shaders_bytecode\\default.vert.spv");
			VkShaderModule FS = LoadShader(Device, "shaders_bytecode\\default.frag.spv");
			
			VkDescriptorPool DescriptorPool = CreateDescriptorPool(Device);

			VkDescriptorSetLayoutBinding CameraDescriptorSetLayoutBinding = CreateDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
			VkDescriptorSetLayout CameraDescriptorSetLayout = CreateDescriptorSetLayout(Device, 1, &CameraDescriptorSetLayoutBinding);

			VkDescriptorSet CameraDescriptorSet = CreateDescriptorSet(Device, DescriptorPool, CameraDescriptorSetLayout);
			SBuffer CameraDescriptorSetBindingBuffer = CreateBuffer(MemoryAllocator, 2*sizeof(mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
			UpdateDescriptorSet(Device, CameraDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, CameraDescriptorSetBindingBuffer, 2*sizeof(mat4));

			VkDescriptorSetLayoutBinding ObjectsDescriptorSetLayoutBinding = CreateDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
			VkDescriptorSetLayout ObjectsDescriptorSetLayout = CreateDescriptorSetLayout(Device, 1, &ObjectsDescriptorSetLayoutBinding);

			VkDescriptorSet ObjectsDescriptorSet = CreateDescriptorSet(Device, DescriptorPool, ObjectsDescriptorSetLayout);
			UpdateDescriptorSet(Device, ObjectsDescriptorSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, StorageBuffer, StorageBuffer.Allocation->GetSize());

			VkDescriptorSetLayout DescriptorSetLayouts[] = { CameraDescriptorSetLayout, ObjectsDescriptorSetLayout };
			VkPipelineLayout PipelineLayout = CreatePipelineLayout(Device, ArrayCount(DescriptorSetLayouts), DescriptorSetLayouts);
			VkPipeline GraphicsPipeline = CreateGraphicsPipeline(Device, RenderPass, PipelineLayout, VS, FS);

			SGeometry Geometry = {};
			LoadMesh(Geometry, "meshes\\kitten.obj");
			LoadMesh(Geometry, "meshes\\bunny.obj");

			const uint32_t ObjectsCount = 10000;
			std::vector<SObjectTransform> Transforms(ObjectsCount);
			std::vector<uint32_t> MeshIndices(ObjectsCount);

			const float SceneRadius = 100.0f;
			for (uint32_t I = 0; I < ObjectsCount; I++)
			{
				SObjectTransform& Transform = Transforms[I];
			
				MeshIndices[I] = rand() % Geometry.Meshes.size();

				Transform.Position.x = 2.0f * SceneRadius * (float(rand()) / RAND_MAX) - SceneRadius;
				Transform.Position.y = 2.0f * SceneRadius * (float(rand()) / RAND_MAX) - SceneRadius;
				Transform.Position.z = 2.0f * SceneRadius * (float(rand()) / RAND_MAX) - SceneRadius;

				Transform.Scale = ((float(rand()) / RAND_MAX) + 1) * 2;
				// Scaling for bunny.obj
				if (MeshIndices[I] == 1)
					Transform.Scale *= 0.25f;

				float Angle = glm::radians(90.0f * (float(rand()) / RAND_MAX));
				vec3 Axis = vec3((float(rand()) / RAND_MAX) * 2 - 1, (float(rand()) / RAND_MAX) * 2 - 1, (float(rand()) / RAND_MAX) * 2 - 1);
				Transform.Orientation = glm::rotate(quat(1, 0, 0, 0), Angle, Axis);
			}

			std::vector<VkDrawIndexedIndirectCommand> IndirectDraws(ObjectsCount);
			for (uint32_t I = 0; I < ObjectsCount; I++)
			{
				VkDrawIndexedIndirectCommand& Draw = IndirectDraws[I];
				uint32_t MeshIndex = MeshIndices[I];

				Draw.indexCount = Geometry.Meshes[MeshIndex].IndexCount;
				Draw.instanceCount = 1;
				Draw.firstIndex = Geometry.Meshes[MeshIndex].IndexOffset;
				Draw.vertexOffset = Geometry.Meshes[MeshIndex].VertexOffset;
				Draw.firstInstance = I;
			}

			UploadBuffer(Device, CommandPool, CommandBuffer, GraphicsQueue, VertexBuffer, StagingBuffer, Geometry.Vertices.data(), Geometry.Vertices.size() * sizeof(SVertex));
			UploadBuffer(Device, CommandPool, CommandBuffer, GraphicsQueue, IndexBuffer, StagingBuffer, Geometry.Indices.data(), Geometry.Indices.size() * sizeof(uint32_t));
			UploadBuffer(Device, CommandPool, CommandBuffer, GraphicsQueue, StorageBuffer, StagingBuffer, Transforms.data(), Transforms.size() * sizeof(SObjectTransform));
			UploadBuffer(Device, CommandPool, CommandBuffer, GraphicsQueue, IndirectBuffer, StagingBuffer, IndirectDraws.data(), IndirectDraws.size() * sizeof(VkDrawIndexedIndirectCommand));

			double FrameCpuTimeAverage = 0.0f;
			double FrameGpuTimeAverage = 0.0f;
			while (!glfwWindowShouldClose(Window))
			{
				double FrameCpuBeginTime = glfwGetTime();

				glfwPollEvents();

				ResizeSwapchainIfChanged(Swapchain, Device, PhysicalDevice, Surface, SwapchainFormat, DepthFormat, RenderPass, MemoryAllocator);

				SCameraBuffer CameraBufferData = {};
				CameraBufferData.View = glm::lookAt(vec3(0.0f, 0.0f, 3.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
				CameraBufferData.Proj = glm::perspective(70.0f, float(Swapchain.Width) / float(Swapchain.Height), 0.1f, FLT_MAX);
				memcpy(CameraDescriptorSetBindingBuffer.Data, &CameraBufferData, sizeof(CameraBufferData));

				uint32_t ImageIndex = 0;
				VkCheck(vkAcquireNextImageKHR(Device, Swapchain.VkSwapchain, UINT64_MAX, AcquireSemaphore, VK_NULL_HANDLE, &ImageIndex));

				VkCheck(vkResetCommandPool(Device, CommandPool, 0));

				VkCommandBufferBeginInfo CommandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
				CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
				VkCheck(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo));

				vkCmdResetQueryPool(CommandBuffer, QueryPool, 0, 2);
				vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, QueryPool, 0);

				VkViewport Viewport = { 0.0f, float(Swapchain.Height), float(Swapchain.Width), -float(Swapchain.Height), 0.0f, 1.0f };
				VkRect2D Scissor = { {0, 0}, {Swapchain.Width, Swapchain.Height} };
				vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
				vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

				VkImageMemoryBarrier RenderBeginBarrier = CreateImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, Swapchain.Images[ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT);
				vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &RenderBeginBarrier);

				VkClearValue ClearColorValue = { 0.125f, 0.25f, 0.5f };
				VkClearValue ClearDepthValue = { 1.0f, 0.0f };
				VkClearValue ClearValues[] = { ClearColorValue, ClearDepthValue };
				VkRenderPassBeginInfo RenderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
				RenderPassBeginInfo.renderPass = RenderPass;
				RenderPassBeginInfo.framebuffer = Swapchain.Framebuffers[ImageIndex];
				RenderPassBeginInfo.renderArea.extent.width = Swapchain.Width;
				RenderPassBeginInfo.renderArea.extent.height = Swapchain.Height;
				RenderPassBeginInfo.clearValueCount = ArrayCount(ClearValues);
				RenderPassBeginInfo.pClearValues = ClearValues;
				vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);

				VkDescriptorSet DescriptorSets[] = { CameraDescriptorSet, ObjectsDescriptorSet };
				vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, ArrayCount(DescriptorSets), DescriptorSets, 0, 0);

				VkDeviceSize Offset = 0;
				vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &VertexBuffer.Buffer, &Offset);
				vkCmdBindIndexBuffer(CommandBuffer, IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
				
				vkCmdDrawIndexedIndirect(CommandBuffer, IndirectBuffer.Buffer, 0, ObjectsCount, sizeof(VkDrawIndexedIndirectCommand));

				vkCmdEndRenderPass(CommandBuffer);

				VkImageMemoryBarrier RenderEndBarrier = CreateImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, Swapchain.Images[ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT);
				vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &RenderEndBarrier);

				vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, QueryPool, 1);

				VkCheck(vkEndCommandBuffer(CommandBuffer));

				VkPipelineStageFlags SubmitWaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				VkSubmitInfo SubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
				SubmitInfo.waitSemaphoreCount = 1;
				SubmitInfo.pWaitSemaphores = &AcquireSemaphore;
				SubmitInfo.pWaitDstStageMask = &SubmitWaitStage;
				SubmitInfo.commandBufferCount = 1;
				SubmitInfo.pCommandBuffers = &CommandBuffer;
				SubmitInfo.signalSemaphoreCount = 1;
				SubmitInfo.pSignalSemaphores = &ReleaseSemaphore;
				VkCheck(vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, 0));

				VkPresentInfoKHR PresentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
				PresentInfo.waitSemaphoreCount = 1;
				PresentInfo.pWaitSemaphores = &ReleaseSemaphore;
				PresentInfo.swapchainCount = 1;
				PresentInfo.pSwapchains = &Swapchain.VkSwapchain;
				PresentInfo.pImageIndices = &ImageIndex;
				VkCheck(vkQueuePresentKHR(GraphicsQueue, &PresentInfo));

				VkCheck(vkDeviceWaitIdle(Device));

				uint64_t Timestamps[2] = {};
				VkCheck(vkGetQueryPoolResults(Device, QueryPool, 0, ArrayCount(Timestamps), sizeof(Timestamps), Timestamps, sizeof(Timestamps[0]), VK_QUERY_RESULT_64_BIT));

				double FrameGpuBeginTime = double(Timestamps[0]) * PhysicalDeviceProps.limits.timestampPeriod * 1e-6;
				double FrameGpuEndTime = double(Timestamps[1]) * PhysicalDeviceProps.limits.timestampPeriod * 1e-6;
				double FrameGpuTime = FrameGpuEndTime - FrameGpuBeginTime;

				double FrameCpuEndTime = glfwGetTime();
				double FrameCpuTime = 1000.0*(FrameCpuEndTime - FrameCpuBeginTime);

				FrameCpuTimeAverage = 0.95*FrameCpuTimeAverage + 0.05*FrameCpuTime;
				FrameGpuTimeAverage = 0.95*FrameGpuTimeAverage + 0.05*FrameGpuTime;

				char Title[256];
				sprintf(Title, "cpu: %.2f ms; gpu: %.2f ms", FrameCpuTimeAverage, FrameGpuTimeAverage);

				glfwSetWindowTitle(Window, Title);
			}
		}
		else
		{
			printf("Can't create window\n");
		}
	}
	else
	{
		printf("Can't initalize GLFW\n");
	}

	return 0;
}