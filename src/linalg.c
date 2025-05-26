#include <math.h>
#include "clamp.c"

#include <cglm/cglm.h>

#define FLOAT_EPSILON 0.0000001

typedef struct
{
	union 
	{
    	float data[2];
    	struct 
    	{
        	float x;
        	float y;
    	};
	};
} Vec2;

typedef struct
{
	union 
	{
    	float data[3];
    	struct 
    	{
        	float x;
        	float y;
        	float z;
    	};
    	struct
    	{
        	float r;
        	float g;
        	float b;
    	};
	};
} Vec3;

typedef struct
{
	union 
	{
    	float data[4];
    	struct 
    	{
        	float x;
        	float y;
        	float z;
        	float w;
    	};
    	struct
    	{
        	float r;
        	float g;
        	float b;
        	float a;
    	};
	};
} Vec4;

typedef struct
{
	union 
	{
		float data  [16];
		float data2d[4][4];
		struct 
		{
			Vec4 a;
			Vec4 b;
			Vec4 c;
			Vec4 d;
		};
	};
} Mat4;

typedef struct
{
    Vec3 a;
    Vec3 b;
} Vec3Line;

Vec2 vec2_new(float x, float y)
{
	return (Vec2){{{x, y}}};
}

Vec2 vec2_zero()
{
	return (Vec2){{{0, 0}}};
}

Vec2 vec2_add(Vec2 a, Vec2 b)
{
	return vec2_new(a.x + b.x, a.y + b.y);
}

Vec2 vec2_sub(Vec2 a, Vec2 b)
{
	return vec2_new(a.x - b.x, a.y - b.y);
}

Vec2 vec2_mul(Vec2 a, Vec2 b)
{
	return vec2_new(a.x * b.x, a.y * b.y);
}

Vec2 vec2_div(Vec2 a, Vec2 b)
{
	return vec2_new(a.x / b.x, a.y / b.y);
}

Vec2 vec2_scale(Vec2 v, float s)
{
    return vec2_new(
        v.data[0] * s,
        v.data[1] * s);
}

float vec2_dot(Vec2 a, Vec2 b)
{
	return (a.x * b.x) + (a.y * b.y);
}

float vec2_magnitude(Vec2 v)
{
    return sqrt(v.x * v.x + v.y * v.y);
}

Vec2 vec2_normalize(Vec2 v)
{
	float magnitude = vec2_magnitude(v);

	if(magnitude < FLOAT_EPSILON) {
    	return vec2_zero();
	}

	return vec2_new(
    	v.x / magnitude,
    	v.y / magnitude);
}

float vec2_distance(Vec2 a, Vec2 b)
{
	float result = 0;
	for(uint8_t i = 0; i < 2; i++)
	{
		float t1 = a.data[i] - b.data[i];
		result += t1 * t1;
	}

	return sqrt(result);
}

Vec2 vec2_lerp(Vec2 a, Vec2 b, float t)
{
	float clamped_t = float_clamp(t, 0.0f, 1.0f);

	Vec2 s = vec2_new(clamped_t, clamped_t);
	Vec2 v = vec2_sub(a, b);
	v = vec2_mul(s, v);

	return vec2_add(a, v);
}

Vec3 vec3_new(float x, float y, float z)
{
    return (Vec3){{{x, y, z}}};
}

Vec3 vec3_zero()
{
    return vec3_new(0.0f, 0.0f, 0.0f);
}

Vec3 vec3_add(Vec3 a, Vec3 b)
{
    return vec3_new(
    	a.data[0] + b.data[0],
    	a.data[1] + b.data[1],
    	a.data[2] + b.data[2]);
}

Vec3 vec3_sub(Vec3 a, Vec3 b)
{
    return vec3_new(
    	a.data[0] - b.data[0],
    	a.data[1] - b.data[1],
    	a.data[2] - b.data[2]);
}

Vec3 vec3_scale(Vec3 v, float s)
{
    return vec3_new(
        v.data[0] * s,
        v.data[1] * s,
        v.data[2] * s);
}

float vec3_dot(Vec3 a, Vec3 b)
{
	return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

float vec3_magnitude(Vec3 v)
{
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 vec3_normalize(Vec3 v)
{
	float magnitude = vec3_magnitude(v);

	// TODO - is this reasonable?
	if(magnitude < FLOAT_EPSILON) {
    	return vec3_zero();
	}

	return vec3_new(
    	v.x / magnitude,
    	v.y / magnitude,
    	v.z / magnitude);
}

Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    return vec3_new(
        a.data[1] * b.data[2] - a.data[2] * b.data[1],
        a.data[2] * b.data[0] - a.data[0] * b.data[2],
        a.data[0] * b.data[1] - a.data[1] * b.data[0]);
}

// TODO - Get rid of cglm.
// These two functions are our unneccesary glue for now.
void mat4_glm_to_internal(Mat4* dst, mat4 glm)
{
	//memcpy(dst, glm, sizeof(Mat4));
	for(uint8_t i = 0; i < 16; i++)
	{
		dst->data[i] = ((float*)glm)[i];
	}
}

void mat4_internal_to_glm(mat4 dst, Mat4* internal)
{
	//memcpy(dst, internal, sizeof(Mat4));
	for(uint8_t i = 0; i < 16; i++)
	{
		((float*)dst)[i] = internal->data[i];
	}
}

void mat4_identity(Mat4* dst)
{
	mat4 glm_m;
	glm_mat4_identity(glm_m);
	mat4_glm_to_internal(dst, glm_m);
}

void mat4_translate(Mat4* dst, Mat4* m, Vec3 v)
{
	mat4 glm_m;
	mat4_internal_to_glm(glm_m, m);

	glm_translate(glm_m, v.data);

	mat4_glm_to_internal(dst, glm_m);
}

void mat4_rotate(Mat4* dst, float angle, Vec3 axis)
{
	mat4 glm_dst;
	mat4_internal_to_glm(glm_dst, dst);

	glm_rotate(glm_dst, angle, axis.data);

	mat4_glm_to_internal(dst, glm_dst);
}

void mat4_multiply(Mat4* dst, Mat4* a, Mat4* b)
{
	float a00 = a->data2d[0][0], a01 = a->data2d[0][1], a02 = a->data2d[0][2], a03 = a->data2d[0][3],
	    a10 = a->data2d[1][0], a11 = a->data2d[1][1], a12 = a->data2d[1][2], a13 = a->data2d[1][3],
	    a20 = a->data2d[2][0], a21 = a->data2d[2][1], a22 = a->data2d[2][2], a23 = a->data2d[2][3],
	    a30 = a->data2d[3][0], a31 = a->data2d[3][1], a32 = a->data2d[3][2], a33 = a->data2d[3][3],

	    b00 = b->data2d[0][0], b01 = b->data2d[0][1], b02 = b->data2d[0][2], b03 = b->data2d[0][3],
	    b10 = b->data2d[1][0], b11 = b->data2d[1][1], b12 = b->data2d[1][2], b13 = b->data2d[1][3],
	    b20 = b->data2d[2][0], b21 = b->data2d[2][1], b22 = b->data2d[2][2], b23 = b->data2d[2][3],
	    b30 = b->data2d[3][0], b31 = b->data2d[3][1], b32 = b->data2d[3][2], b33 = b->data2d[3][3];

	dst->data2d[0][0] = a00 * b00 + a10 * b01 + a20 * b02 + a30 * b03;
	dst->data2d[0][1] = a01 * b00 + a11 * b01 + a21 * b02 + a31 * b03;
	dst->data2d[0][2] = a02 * b00 + a12 * b01 + a22 * b02 + a32 * b03;
	dst->data2d[0][3] = a03 * b00 + a13 * b01 + a23 * b02 + a33 * b03;
	dst->data2d[1][0] = a00 * b10 + a10 * b11 + a20 * b12 + a30 * b13;
	dst->data2d[1][1] = a01 * b10 + a11 * b11 + a21 * b12 + a31 * b13;
	dst->data2d[1][2] = a02 * b10 + a12 * b11 + a22 * b12 + a32 * b13;
	dst->data2d[1][3] = a03 * b10 + a13 * b11 + a23 * b12 + a33 * b13;
	dst->data2d[2][0] = a00 * b20 + a10 * b21 + a20 * b22 + a30 * b23;
	dst->data2d[2][1] = a01 * b20 + a11 * b21 + a21 * b22 + a31 * b23;
	dst->data2d[2][2] = a02 * b20 + a12 * b21 + a22 * b22 + a32 * b23;
	dst->data2d[2][3] = a03 * b20 + a13 * b21 + a23 * b22 + a33 * b23;
	dst->data2d[3][0] = a00 * b30 + a10 * b31 + a20 * b32 + a30 * b33;
	dst->data2d[3][1] = a01 * b30 + a11 * b31 + a21 * b32 + a31 * b33;
	dst->data2d[3][2] = a02 * b30 + a12 * b31 + a22 * b32 + a32 * b33;
	dst->data2d[3][3] = a03 * b30 + a13 * b31 + a23 * b32 + a33 * b33;
}

void mat4_lookat(Mat4* dst, Vec3 eye, Vec3 center, Vec3 up)
{
	mat4 glm_m;
	glm_lookat(eye.data, center.data, up.data, glm_m);

	mat4_glm_to_internal(dst, glm_m);
}

void mat4_perspective(Mat4* dst, float fov_y, float aspect_ratio, float clip_near, float clip_far)
{
	mat4 glm_dst;
	mat4_internal_to_glm(glm_dst, dst);

	glm_perspective(fov_y, aspect_ratio, clip_near, clip_far, glm_dst);

	mat4_glm_to_internal(dst, glm_dst);
}

float radians(float degrees)
{
	return glm_rad(degrees);
}
