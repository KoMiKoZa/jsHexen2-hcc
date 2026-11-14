// Plain HexenC with one vector condition and one expression with two calls.
// Without -fte both get a warning; with -fte neither does (tests/test_fte.py).

float random(void) : 7;

vector dir;
float sum;

void() both =
{
	if (dir)
		sum = random() + random();
};
