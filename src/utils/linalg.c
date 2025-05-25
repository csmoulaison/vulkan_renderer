#include <math.h>
#include "clamp.c"

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
	union {
		float data[16];
		struct {
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

	// TODO - is this reasonable?
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

// TODO - our own m4 struct
void mat4_lookat(Vec3 eye, Vec3 center, Vec3 up, Mat4 dst)
{
	// TODO - Get this working
	//glm_lookat(eye.data, center.data, up.data, dst);
}

float radians(float degrees)
{
	// TODO - Get this working
	return 0; // glm_rad(degrees);
}
