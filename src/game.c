#include "input.c"

typedef struct
{
	double time_since_initialize;
	StaticMesh static_meshes[STATIC_MESHES_LEN];
} GameMemory;

void game_initialize(void* mem, uint32_t mem_bytes)
{
	srand(time(NULL));

    GameMemory* game = (GameMemory*)mem;

    game->time_since_initialize = 0;

	for(uint8_t mesh_index = 0; mesh_index < STATIC_MESHES_LEN; mesh_index++)
	{
	    glm_mat4_identity(game->static_meshes[mesh_index].orientation);
	    game->static_meshes[mesh_index].asset_handle = mesh_index;
	}
	game->static_meshes[0].position = vec3_new(0, 0, 3);
	game->static_meshes[1].position = vec3_new(2, 1, 1);
}

void game_loop(
    void*         memory,
    size_t        memory_bytes,
    float         dt,
    uint32_t      window_w,
    uint32_t      window_h,
    InputContext* input,
    RenderList*   render_list)
{
    GameMemory* game = (GameMemory*)memory;

    game->time_since_initialize += dt;

    if(input->move_forward.held)
    {
	    float mouse_magnitude = vec3_magnitude(vec3_new(input->mouse_delta_x, input->mouse_delta_y, 0));
	    glm_rotate(game->static_meshes[0].orientation, dt * radians(mouse_magnitude * dt * 400), vec3_new(-input->mouse_delta_y, -input->mouse_delta_x, 0).data);
    }
    if(input->move_back.held)
    {
	    game->static_meshes[0].position.z += input->mouse_delta_y * dt * 0.5;
    }
    if(input->move_right.held)
    {
	    game->static_meshes[0].position = vec3_add(game->static_meshes[0].position, vec3_scale(vec3_new(input->mouse_delta_x, input->mouse_delta_y, 0), dt * 0.5));
    }

	// NOW - define another transform on GameMemory -> define on RenderList -> define on UBO

	render_list->clear_color     = vec3_new(0.01, 0.008, 0.02);

	render_list->camera_position = vec3_new(0, 0, 0);
	render_list->camera_target   = game->static_meshes[0].position;

	memcpy(render_list->static_meshes, game->static_meshes, sizeof(StaticMesh) * STATIC_MESHES_LEN);
	render_list->static_meshes_len = STATIC_MESHES_LEN;
}
