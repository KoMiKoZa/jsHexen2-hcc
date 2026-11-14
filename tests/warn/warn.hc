// Plain HexenC with one vector condition and one expression with two calls.
// Without -fte both get a warning; with -fte neither does (tests/test_fte.py).

float random(void) : 7;

vector dir;

float() two_calls =
{
	return random() + random();
};

void() vector_test =
{
	if (dir)
		dir = '0 0 0';
};
