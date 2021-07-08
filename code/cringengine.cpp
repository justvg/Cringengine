#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

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

	VkDeviceCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	CreateInfo.queueCreateInfoCount = 1;
	CreateInfo.pQueueCreateInfos = &QueueCreateInfo;
	CreateInfo.enabledExtensionCount = ArrayCount(Extensions);
	CreateInfo.ppEnabledExtensionNames = Extensions;

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
	VkCompositeAlphaFlagBitsKHR compositeAlpha = (SurfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR :
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
	CreateInfo.compositeAlpha = compositeAlpha;
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

VkPipelineLayout CreatePipelineLayout(VkDevice Device)
{
	VkPipelineLayoutCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

	VkPipelineLayout PipelineLayout = 0;
	VkCheck(vkCreatePipelineLayout(Device, &CreateInfo, 0, &PipelineLayout));
	assert(PipelineLayout);

	return PipelineLayout;
}

struct SVertex
{
	float px, py, pz;
	float nx, ny, nz;
};

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
	AttributeDescrs[1].offset = OffsetOf(SVertex, nx);

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

struct SBuffer
{
	VkBuffer Buffer;
	VmaAllocation Allocation;
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
	assert(Buffer.Buffer);
	assert(Buffer.Allocation);

	return Buffer;
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

			VkShaderModule VS = LoadShader(Device, "shaders_bytecode\\default.vert.spv");
			VkShaderModule FS = LoadShader(Device, "shaders_bytecode\\default.frag.spv");

			VkPipelineLayout PipelineLayout = CreatePipelineLayout(Device);

			VkPipeline GraphicsPipeline = CreateGraphicsPipeline(Device, RenderPass, PipelineLayout, VS, FS);

			SVertex TriangleVertices[] =
			{
				{ -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f },
				{ 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f },
				{ 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f }
			};
			SBuffer VertexBuffer = CreateBuffer(MemoryAllocator, sizeof(TriangleVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
			
			void* VertexBufferPtr;
			VkCheck(vmaMapMemory(MemoryAllocator, VertexBuffer.Allocation, &VertexBufferPtr));
			memcpy(VertexBufferPtr, TriangleVertices, sizeof(TriangleVertices));
			vmaUnmapMemory(MemoryAllocator, VertexBuffer.Allocation);

			while (!glfwWindowShouldClose(Window))
			{
				glfwPollEvents();

				ResizeSwapchainIfChanged(Swapchain, Device, PhysicalDevice, Surface, SwapchainFormat, DepthFormat, RenderPass, MemoryAllocator);

				uint32_t ImageIndex = 0;
				VkCheck(vkAcquireNextImageKHR(Device, Swapchain.VkSwapchain, UINT64_MAX, AcquireSemaphore, VK_NULL_HANDLE, &ImageIndex));

				VkCheck(vkResetCommandPool(Device, CommandPool, 0));

				VkCommandBufferBeginInfo CommandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
				CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
				VkCheck(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo));

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

				VkDeviceSize Offset = 0;
				vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &VertexBuffer.Buffer, &Offset);

				vkCmdDraw(CommandBuffer, 3, 1, 0, 0);

				vkCmdEndRenderPass(CommandBuffer);

				VkImageMemoryBarrier RenderEndBarrier = CreateImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, Swapchain.Images[ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT);
				vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &RenderEndBarrier);

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