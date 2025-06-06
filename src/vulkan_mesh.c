// Holds the raw vertex/index data as well as whatever else might be stored as a result of loading
// the obj file.
// This is strictly data which is no longer needed after initialization.
typedef struct
{
	VulkanMeshVertex vertices[];
	uint32_t         vertices_len;
	
	uint32_t         indices[];
	uint32_t         indices_len;


} VulkanMeshData;

// NOW - vulkan_load_mesh needs to get reimplemented properly here.
// The main idea is that we are getting all of the info which can be gleaned from the .obj file.
// A good way of scoping this is to not require any context from the VulkanRenderer if possible.
// Eventually, the renderer will be using this data in a way which might be dynamically messing with
// memory at runtime, and this might be API dependant, so shouldn't be considered here.
//
// That is, because this vulkan_load_mesh function and indeed VulkanMeshData struct is probably
// going to emerge unscathed in all but name as a part of the renderer front end, at least until we
// move away from the .obj format.
void vulkan_load_mesh(VulkanMeshData* data, char* mesh_filename, char* texture_filename)
{
	Vec2     tmp_texture_uvs[8000];
	uint32_t tmp_texture_uvs_len = 0;

	// Note that tmp_face_elements do not correspond with "f" records, but rather with one of the
	// elements in those records.
	struct 
	{
		uint32_t vertex_index;
		uint32_t texture_uv_index;
	} tmp_face_elements[32000];
	uint32_t tmp_face_elements_len = 0;

	while(true)
	{
		char keyword[128];
		int32_t res = fscanf(file, "%s", keyword);

		if(res == EOF)
		{
			break;
		}

		if(strcmp(keyword, "v") == 0)
		{
			Vec3* pos = &vertices[vertices_len].position;
			fscanf(file, "%f %f %f", &pos->x, &pos->y, &pos->z);
			vertices_len++;
		}
		else if(strcmp(keyword, "vt") == 0)
		{
			Vec2* tmp_texture_uv = &tmp_texture_uvs[tmp_texture_uvs_len];
			fscanf(file, "%f %f", &tmp_texture_uv->x, &tmp_texture_uv->y);
			tmp_texture_uv->y = 1 - tmp_texture_uv->y;
			tmp_texture_uvs_len++;
		}
		else if(strcmp(keyword, "f") == 0)
		{
			int32_t throwaways[3];
			int32_t values_len = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", 
				&tmp_face_elements[tmp_face_elements_len + 0].vertex_index, 
				&tmp_face_elements[tmp_face_elements_len + 0].texture_uv_index, 
				&throwaways[0], 
				&tmp_face_elements[tmp_face_elements_len + 1].vertex_index, 
				&tmp_face_elements[tmp_face_elements_len + 1].texture_uv_index, 
				&throwaways[1], 
				&tmp_face_elements[tmp_face_elements_len + 2].vertex_index, 
				&tmp_face_elements[tmp_face_elements_len + 2].texture_uv_index, 
				&throwaways[2]);

			if(values_len != 9)
			{
				panic();
			}
			tmp_face_elements_len += 3;
		}
	}
	fclose(file);

	uint32_t         indices [32000];
	uint32_t         indices_len  = 0;
	for(uint32_t element_index = 0; element_index < tmp_face_elements_len; element_index++)
	{
		uint32_t index = tmp_face_elements[element_index].vertex_index - 1;
		indices[element_index] = index;
		vertices[index].texture_uv = tmp_texture_uvs[tmp_face_elements[element_index].texture_uv_index - 1];

		indices_len++;
	}

	// Allocate device local memory buffer
	// 
	// TODO - This assumes only one mesh.
	renderer->mesh_datas[mesh_index] = (VulkanMeshData)
	{
		.vertex_memory      = (void*)vertices,
		.index_memory       = (void*)indices,
		.vertex_data_stride = sizeof(VulkanMeshVertex),
		.vertices_len       = vertices_len,
		.indices_len        = indices_len
	};
}
