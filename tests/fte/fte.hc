// Code written for FTEQCC. Compiles only with -fte; tests/test_fte.py checks the output.

void sprint(entity e, string s) : 24;
void centerprint(entity client, string s) : 73;
string ftos(float f) : 26;
float random(void) : 7;

float g_inc;								// a global with a low offset, for the -ot check in later()
entity self;
entity world;
.float count;
.float flags;
.vector velocity;
.string target;
float other_count;

string GREETING = "Hello";				// a string constant with a value
float LIMIT = 10;
float LIMIT = 10;						// the same constant again
float TABLE[3] = { 1, 2, 3, };			// a trailing comma
nosave float NOT_SAVED;
float big = 10000000000;
float NEG = -.5;
float NEG2 = - 1;
float NEGARR[2] = { -.5, - 1 };
float NEGZERO = - 0;

void() helper;							// prototype
void() helper { };						// body without "=", empty block
void helper2(float a) = [0, helper] { }	// "=" before the frame and body
void() framed [1, helper] { };			// frames without "="

// Under -ot, x++ must not turn an unrelated earlier field read into an address,
// even when a temp's index equals the global's offset
float() later =
{
	local float n;
	n = 1;
	n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1;
	n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1;
	n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1;
	n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1; n = n + 1;
	n = self.count + g_inc++;
	return n;
};

float(float a, float b) pick =
{
	local float n = a - b;				// local initialized by code
	float other_count = n * 2;			// a local that hides a global
	if !(n)								// FTEQCC's "if not"
		return b;
	for (float i = 0; i < 3; i++)		// for with a declaration
		n += i;
	++n;
	n--;
	n = -n + +a;						// unary minus and plus
	n = n-1;							// "a-1" is a subtraction
	self.velocity = self.velocity / 2;	// vector divided by a float
	self.velocity /= 4;
	self.velocity = 0;					// a float assigned to a vector
	self.velocity = [n, 0, a];			// vector constructor
	if (self && self.count)				// && on an entity
		n = n + (1, 2);					// comma operator
	n = 1, other_count = 2;				// comma between statements
	other_count = 1;
	n = other_count (+) 4;				// the new value, 5
	centerprint(self, "A", ftos(a), "/", ftos(b));	// extra arguments to a builtin
	return n;
}
