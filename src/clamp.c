float float_clamp(float v, float min, float max) {
	if(v < min)
	{
		return min;
	}
	else if(v > max)
	{
		return max;
	}
	return v;
}
