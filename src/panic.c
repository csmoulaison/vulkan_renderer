#include <stdio.h>

#define panic() printf("PANIC at %s:%u\n", __FILE__, __LINE__);\
				exit(1);
