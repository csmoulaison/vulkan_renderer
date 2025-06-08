typedef struct
{
	uint32_t asset_handle;
	Vec3     position;
	mat4     orientation;
} StaticMesh;

typedef struct
{
	Vec3       clear_color;

	Vec3       camera_position;
	Vec3       camera_target;

	StaticMesh static_meshes[STATIC_MESHES_LEN];
	uint32_t   static_meshes_len;
} RenderList;

