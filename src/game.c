#include "input.c"

#define CUBEZ 3

typedef struct
{
	double time_since_initialize;
	mat4   cube_orientation;
	Vec3   cube_position;
	Vec3   cube_rotation_per_frame;
} GameMemory;

void game_initialize(void* mem, uint32_t mem_bytes)
{
	srand(time(NULL));

    GameMemory* game = (GameMemory*)mem;

    game->time_since_initialize = 0;
    glm_mat4_identity(game->cube_orientation);
    game->cube_position = vec3_new(0, 0, CUBEZ);
    game->cube_rotation_per_frame = vec3_new(
	    rand_t() * 2 - 1,
	    rand_t() * 2 - 1,
	    rand_t() * 2 - 1
    );
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
	    glm_rotate(game->cube_orientation, dt * radians(mouse_magnitude * dt * 400), vec3_new(-input->mouse_delta_y, -input->mouse_delta_x, 0).data);
    }
    if(input->move_back.held)
    {
	    game->cube_position.z += input->mouse_delta_y * dt * 0.5;
    }
    if(input->move_right.held)
    {
	    game->cube_position = vec3_add(game->cube_position, vec3_scale(vec3_new(input->mouse_delta_x, input->mouse_delta_y, 0), dt * 0.5));
    }
    //glm_rotate(game->cube_orientation, dt * radians(30), game->cube_rotation_per_frame.data);

    mat4 cube_transform;
    glm_mat4_identity(cube_transform);
    glm_translate(cube_transform, game->cube_position.data);
    glm_mat4_mul(cube_transform, game->cube_orientation, cube_transform);

	render_list->clear_color     = vec3_new(0.01, 0.008, 0.02);
	render_list->camera_position = vec3_new(0, 0, 0);
	render_list->camera_target   = vec3_new(0, 0, CUBEZ);
	memcpy(render_list->cube_transform, cube_transform, sizeof(mat4));
}
