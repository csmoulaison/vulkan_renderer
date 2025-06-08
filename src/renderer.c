#define RENDERER_BACKEND_VULKAN   0
// For instance:
// #define RENDERER_BACKEND_OPEN_GL  1
// #define RENDERER_BACKEND_DIRECT_X 2
// #define RENDERER_BACKEND_WEB_GPU  3

typedef struct
{
	uint8_t backend;
	union
	{
		VulkanContext vulkan;

		// TODO - For instance:
		// OpenGlContext  open_gl;
		// DirectXContext direct_x;
		// WebGpuContext  web_gpu;
	};
} Renderer;

typedef struct
{
	VulkanPlatform vulkan;
} RendererPlatformData;

void renderer_initialize(Renderer* renderer, RendererPlatformData* platform_specific_data)
{
	renderer->backend = RENDERER_BACKEND_VULKAN;

	switch(renderer->backend)
	{
		case RENDERER_BACKEND_VULKAN:
		{
			vulkan_initialize(&renderer->vulkan, &platform_specific_data->vulkan);
			break;
		}
		default:
		{
			break;
		}
	}
}

void renderer_loop(Renderer* renderer, RenderList* render_list)
{
	vulkan_loop(&renderer->vulkan, render_list);
}
