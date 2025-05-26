// VOLATILE - this must match the number of buttons defined in input_state.
#define INPUT_BUTTONS_LEN 4

typedef struct 
{
	uint32_t held;
	uint32_t pressed;
	uint32_t released;
} InputButton;

typedef struct
{
	int32_t mouse_delta_x;
	int32_t mouse_delta_y;
	int32_t mouse_x;
	int32_t mouse_y;

	union 
	{
    	InputButton buttons[INPUT_BUTTONS_LEN];
    	struct 
    	{
        	InputButton move_forward;
        	InputButton move_back;
        	InputButton move_left;
        	InputButton move_right;
    	};
	};
} InputContext;

void input_reset_buttons(InputContext* input)
{
    for(uint32_t i = 0; i < INPUT_BUTTONS_LEN; i++) {
        input->buttons[i].pressed = 0;
        input->buttons[i].released = 0;
    }
}

void input_button_press(InputButton* btn) 
{
    btn->pressed = 1;
    btn->held = 1;
}

void input_button_release(InputButton* btn) 
{
    btn->held = 0;
    btn->released = 1;
}
