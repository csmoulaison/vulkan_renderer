typedef struct 
{
	VkDescriptorType   type;
	VkShaderStageFlags shader_stage_flags;
	VkDeviceSize       offset_in_host_memory;
	VkDeviceSize       range_in_host_memory;
} VulkanDescriptorSetConfig;

typedef struct
{
	VkFormat format;
	uint32_t offset_in_vertex_data;
} VulkanVertexInputAttributeConfig;

VkShaderModule vulkan_create_shader_module(VulkanRenderer* renderer, char* filename)
{
	FILE* file = fopen(filename, "r");
	if(!file)
	{
		printf("Failed to open file: %s\n", filename);
		panic();
	}
	fseek(file, 0, SEEK_END);
	uint32_t fsize = ftell(file);
	fseek(file, 0, SEEK_SET);
	char src[fsize];

	char c;
	uint32_t i = 0;
	while((c = fgetc(file)) != EOF)
	{
		src[i] = c;
		i++;
	}
	fclose(file);
	
	VkShaderModuleCreateInfo shader_module_create_info = 
	{
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext    = 0,
		.flags    = 0,
		.codeSize = fsize,
		.pCode    = (uint32_t*)src
	};

	VkShaderModule module;
	vk_verify(vkCreateShaderModule(renderer->device, &shader_module_create_info, 0, &module));
	
	return module;
}

void vulkan_create_graphics_pipeline(
	VulkanRenderer*                   renderer,
	VulkanPipeline*                   pipeline,
	char*                             vertex_shader_filename,
	char*                             fragment_shader_filename,
	VulkanDescriptorSetConfig*        descriptor_set_configs,
	uint8_t                           descriptor_sets_len,
	VulkanVertexInputAttributeConfig* vertex_input_attribute_configs,
	uint8_t                           vertex_input_attributes_len,
	size_t                            vertex_data_stride)
{
	// Define descriptor info.
	VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[descriptor_sets_len];
	VkDescriptorPoolSize         descriptor_pool_sizes         [descriptor_sets_len];
	VkWriteDescriptorSet         write_descriptor_sets         [descriptor_sets_len];
	VkDescriptorBufferInfo       descriptor_buffer_infos       [descriptor_sets_len];
	VkDescriptorImageInfo        descriptor_image_infos        [descriptor_sets_len];

	for(uint8_t binding = 0; binding < descriptor_sets_len; binding++)
	{
		VulkanDescriptorSetConfig* config = &descriptor_set_configs[binding];

		descriptor_set_layout_bindings[binding] = (VkDescriptorSetLayoutBinding)
		{
			.binding            = binding,
			.descriptorType     = config->type,
			.descriptorCount    = 1,
			.stageFlags         = config->shader_stage_flags,
			.pImmutableSamplers = 0
		};

		descriptor_pool_sizes[binding] = (VkDescriptorPoolSize)
		{
			.type            = config->type,
			.descriptorCount = 1
		};

		// Only used with uniform buffer or uniform buffer dynamic descriptor types.
		descriptor_buffer_infos[binding] = (VkDescriptorBufferInfo)
		{
			.buffer = renderer->host_mapped_buffer.buffer,
			.offset = config->offset_in_host_memory,
			.range  = config->range_in_host_memory
		};

		// Only used with image sampler descriptor type.
		descriptor_image_infos[binding] = (VkDescriptorImageInfo)
		{
			.sampler     = renderer->texture_sampler,
			// TODO - This is dependant on having only one texture, of course.
			.imageView   = renderer->texture_images[0].view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		write_descriptor_sets[binding] = (VkWriteDescriptorSet)
		{
			.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext            = 0,
			.dstSet           = 0, // set later
			.dstBinding       = binding,
			.dstArrayElement  = 0,
			.descriptorCount  = 1,
			.descriptorType   = config->type,
			.pImageInfo       = &descriptor_image_infos[binding],
			.pBufferInfo      = &descriptor_buffer_infos[binding],
			.pTexelBufferView = 0
		};
	}

	// Create descriptors resources.
	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = 
	{
		.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext        = 0,
		.flags        = 0,
		.bindingCount = descriptor_sets_len,
		.pBindings    = descriptor_set_layout_bindings
	};
	vk_verify(vkCreateDescriptorSetLayout(renderer->device, &descriptor_set_layout_create_info, 0, &pipeline->descriptor_set_layout));

	VkDescriptorPoolCreateInfo descriptor_pool_create_info = 
	{
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext         = 0,
		.flags         = 0,
		.poolSizeCount = descriptor_sets_len,
		.pPoolSizes    = descriptor_pool_sizes,
		.maxSets       = 1
	};
	vk_verify(vkCreateDescriptorPool(renderer->device, &descriptor_pool_create_info, 0, &pipeline->descriptor_pool));

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = 
	{
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext              = 0,
		.descriptorPool     = pipeline->descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts        = &pipeline->descriptor_set_layout
	};
	vk_verify(vkAllocateDescriptorSets(renderer->device, &descriptor_set_allocate_info, &pipeline->descriptor_set));

	for(uint8_t binding = 0; binding < descriptor_sets_len; binding++)
	{
		write_descriptor_sets[binding].dstSet = pipeline->descriptor_set;
	}

	vkUpdateDescriptorSets(renderer->device, descriptor_sets_len, write_descriptor_sets, 0, 0);

	// Define vertex input attribute descriptions.
	VkVertexInputAttributeDescription vertex_input_attribute_descriptions[vertex_input_attributes_len];
	for(uint8_t location = 0; location < vertex_input_attributes_len; location++)
	{
		vertex_input_attribute_descriptions[location] = (VkVertexInputAttributeDescription)
		{
			.binding  = 0,
			.location = location,
			.format   = vertex_input_attribute_configs[location].format,
			.offset   = vertex_input_attribute_configs[location].offset_in_vertex_data
		};
	}

	// Compile shaders.
	VkShaderModule vertex_shader   = vulkan_create_shader_module(renderer, vertex_shader_filename);
	VkShaderModule fragment_shader = vulkan_create_shader_module(renderer, fragment_shader_filename);
	VkPipelineShaderStageCreateInfo shader_stage_create_infos[2] =
	{
		{
			.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext               = 0,
			.flags               = 0,
			.stage               = VK_SHADER_STAGE_VERTEX_BIT,
			.module              = vertex_shader,
			.pName               = "main",
			.pSpecializationInfo = 0,
		},
		{
			.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext               = 0,
			.flags               = 0,
			.stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module              = fragment_shader,
			.pName               = "main",
			.pSpecializationInfo = 0,
		}
	};

	// Define dynamic states.
	const VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	// Create pipeline layout.
	VkPipelineLayoutCreateInfo pipeline_layout_create_info = 
	{
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext                  = 0,
		.flags                  = 0,
		.setLayoutCount         = 1,
		.pSetLayouts            = &pipeline->descriptor_set_layout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges    = 0
	};
	vk_verify(vkCreatePipelineLayout(renderer->device, &pipeline_layout_create_info, 0, &pipeline->layout));

	// Create graphics pipeline.
	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = 
	{
		.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext               = &(VkPipelineRenderingCreateInfoKHR)
		{
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			.pNext                   = 0,
			.viewMask                = 0,
			.colorAttachmentCount    = 1,
			.pColorAttachmentFormats = &renderer->surface_format.format,
			.depthAttachmentFormat   = VK_FORMAT_D32_SFLOAT,
			.stencilAttachmentFormat = 0
		},
		.flags               = 0,
		.stageCount          = 2,
		.pStages             = shader_stage_create_infos,
		.pVertexInputState   = &(VkPipelineVertexInputStateCreateInfo)
		{
			.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext                           = 0,
			.flags                           = 0,
			.vertexBindingDescriptionCount   = 1,
			.pVertexBindingDescriptions      = &(VkVertexInputBindingDescription)
			{
				.binding   = 0,
				.stride    = vertex_data_stride,
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			},
			.vertexAttributeDescriptionCount = vertex_input_attributes_len,
			.pVertexAttributeDescriptions    = vertex_input_attribute_descriptions
		},
		.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo)
		{
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext                  = 0,
			.flags                  = 0,
			.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE
		},
		.pTessellationState   = 0,
		.pViewportState      = &(VkPipelineViewportStateCreateInfo)
		{
			.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext         = 0,
			.flags         = 0,
			.viewportCount = 1,
			.scissorCount  = 1,
			// pViewports and pScissors null because they're set later during rendering.
			.pViewports    = 0,
			.pScissors     = 0
		},
		.pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) 
		{
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext                   = 0,
			.flags                   = 0,
			.depthClampEnable        = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode             = VK_POLYGON_MODE_FILL,
			.cullMode                = VK_CULL_MODE_NONE,
			.frontFace               = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable         = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp          = 0.0f,
			.depthBiasSlopeFactor    = 0.0f,
			.lineWidth               = 1.0f
		},
		.pMultisampleState   = &(VkPipelineMultisampleStateCreateInfo) 
		{
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.rasterizationSamples  = renderer->device_framebuffer_sample_counts,
			.sampleShadingEnable   = VK_FALSE,
			.minSampleShading      = VK_FALSE,
			.pSampleMask           = 0,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable      = VK_FALSE
		},
		.pDepthStencilState  = &(VkPipelineDepthStencilStateCreateInfo)
		{
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.depthTestEnable       = VK_TRUE,
			.depthWriteEnable      = VK_TRUE,
			.depthCompareOp        = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable     = VK_FALSE,
			.front                 = {},
			.back                  = {},
			.minDepthBounds        = 0,
			.maxDepthBounds        = 0
		},
		.pColorBlendState    = &(VkPipelineColorBlendStateCreateInfo)
		{
			.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext             = 0,
			.flags             = 0,
			.logicOpEnable     = VK_FALSE,
			.logicOp           = 0,
			.attachmentCount   = 1,
			.pAttachments      = &(VkPipelineColorBlendAttachmentState)
			{
				.blendEnable         = VK_FALSE,
				.srcColorBlendFactor = 0,
				.dstColorBlendFactor = 0,
				.colorBlendOp        = 0,
				.srcAlphaBlendFactor = 0,
				.dstAlphaBlendFactor = 0,
				.alphaBlendOp        = 0,
				.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
			},
			.blendConstants    = { 0, 0, 0, 0 }
		},
		.pDynamicState       = &(VkPipelineDynamicStateCreateInfo)
		{
			.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext             = 0,
			.flags             = 0,
			.dynamicStateCount = 2,
			.pDynamicStates    = dynamic_states
		},
		.layout              = pipeline->layout,
		.renderPass          = 0,
		.subpass             = 0,
		.basePipelineHandle  = 0,
		.basePipelineIndex   = 0,
	};

	vk_verify(vkCreateGraphicsPipelines(renderer->device, 0, 1, &graphics_pipeline_create_info, 0, &pipeline->pipeline));

	// Cleanup shader modules.
	vkDestroyShaderModule(renderer->device, vertex_shader,   0);
	vkDestroyShaderModule(renderer->device, fragment_shader, 0);
}
