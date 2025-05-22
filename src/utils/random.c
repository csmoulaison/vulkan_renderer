void rand_init()
{
	srand(time(NULL));
}

int32_t rand_int32(uint32_t max_exclusive)
{
	int32_t num;

	do 
	{
	    num = rand();
	} while (num >= (RAND_MAX - RAND_MAX % max_exclusive));

	return num % max_exclusive;
}

float rand_t()
{
	return (float)rand() / RAND_MAX;
}
