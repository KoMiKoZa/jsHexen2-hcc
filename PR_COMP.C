
//**************************************************************************
//**
//** comp.c
//**
//** $Header: /H2 Mission Pack/Tools/hcc/PR_COMP.C 2     2/12/98 3:19p Jmonroe $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "hcc.h"

// MACROS ------------------------------------------------------------------

#define STRCMP(s1,s2) (((*s1)!=(*s2)) || strcmp(s1,s2))
#define STRNCMP(s1,s2,n) (((*s1)!=(*s2)) || strncmp(s1,s2,n))
#define MAX_ARRAY_ELEMENTS 1024
#define SPECIAL_ASSIGN_OP 500
#define TEMPREF_A 1
#define TEMPREF_B 2
#define TEMPREF_C 4

// TYPES -------------------------------------------------------------------

struct hash_element
{
	struct hash_element *next;
	def_t *def;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void InitHashTable(void);
static void ParseUnion(void);
static type_t *ParseUnionType(void);
static void ParseFunctionDef(def_t *def, type_t *type);
static void ParseCStyleFunctionDef(char *funcName, type_t *type);
static def_t *GetArrayDef(char *name, type_t *type, int *elementCount);
static def_t *GetFieldArrayDef(char *name, type_t *type,
	int *elementCount);
static int ParseArray(char *name, type_t *type);
static def_t *NewVarDef(char *name, type_t *type);
static function_t *ParseImmediateStatements(type_t *type);
static void ParseState(void);
static void NoteTempRef(int statement, int bit, def_t *d);
static void NoteReturnUse(int statement, int bit, def_t *d);
static int GlobalOperands(int op);
static int NewTemp(def_t *d, int statement, int size);
static void TempBeginFunction(void);
static void TempEndFunction(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int FrameIndex;
static struct hash_element *HashTable[MS_HASH_TABLE_SIZE];
static unsigned char TempRefs[MAX_STATEMENTS]; // operands holding a temp index
static int LastReturnWrite = -1; // last statement that wrote OFS_RETURN

pr_info_t pr;
def_t *pr_global_defs[MAX_REGS_ALLOC];	// to find def for a global variable

def_t *pr_scope; // the function being parsed, or NULL

string_t s_file; // filename for function definition

int locals_end; // for tracking local variables vs temps

jmp_buf pr_parse_abort; // longjump with this on parse error

opcode_t pr_opcodes[] =
{
	{"<DONE>", "DONE", -1, false, &def_entity, &def_field, &def_void, 0},

	{"*", "MUL_F", 2, false, &def_float, &def_float, &def_float, 1},
	{"*", "MUL_V", 2, false, &def_vector, &def_vector, &def_float, 1},
	{"*", "MUL_FV", 2, false, &def_float, &def_vector, &def_vector, 1},
	{"*", "MUL_VF", 2, false, &def_vector, &def_float, &def_vector, 1},

	{"/", "DIV", 2, false, &def_float, &def_float, &def_float, 2},

	{"+", "ADD_F", 3, false, &def_float, &def_float, &def_float, 3},
	{"+", "ADD_V", 3, false, &def_vector, &def_vector, &def_vector, 3},

	{"-", "SUB_F", 3, false, &def_float, &def_float, &def_float, 4},
	{"-", "SUB_V", 3, false, &def_vector, &def_vector, &def_vector, 4},

	{"==", "EQ_F", 4, false, &def_float, &def_float, &def_float, 5},
	{"==", "EQ_V", 4, false, &def_vector, &def_vector, &def_float, 5},
	{"==", "EQ_S", 4, false, &def_string, &def_string, &def_float, 5},
	{"==", "EQ_E", 4, false, &def_entity, &def_entity, &def_float, 5},
	{"==", "EQ_FNC", 4, false, &def_function, &def_function, &def_float, 5},

	{"!=", "NE_F", 4, false, &def_float, &def_float, &def_float, 6},
	{"!=", "NE_V", 4, false, &def_vector, &def_vector, &def_float, 6},
	{"!=", "NE_S", 4, false, &def_string, &def_string, &def_float, 6},
	{"!=", "NE_E", 4, false, &def_entity, &def_entity, &def_float, 6},
	{"!=", "NE_FNC", 4, false, &def_function, &def_function, &def_float, 6},

	{"<=", "LE", 4, false, &def_float, &def_float, &def_float, 7},

	{">=", "GE", 4, false, &def_float, &def_float, &def_float, 8},

	{"<", "LT", 4, false, &def_float, &def_float, &def_float, 9},

	{">", "GT", 4, false, &def_float, &def_float, &def_float, 10},

	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_float, 11},
	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_vector, 11},
	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_string, 11},
	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_entity, 11},
	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_field, 11},
	{".", "INDIRECT", 1, false, &def_entity, &def_field, &def_function, 11},

	{".", "ADDRESS", 1, false, &def_entity, &def_field, &def_pointer, 11},

	{"=", "STORE_F", 5, true, &def_float, &def_float, &def_float, 12},
	{"=", "STORE_V", 5, true, &def_vector, &def_vector, &def_vector, 12},
	{"=", "STORE_S", 5, true, &def_string, &def_string, &def_string, 12},
	{"=", "STORE_ENT", 5, true, &def_entity, &def_entity, &def_entity, 12},
	{"=", "STORE_FLD", 5, true, &def_field, &def_field, &def_field, 12},
	{"=", "STORE_FNC", 5, true, &def_function, &def_function, &def_function, 12},

	{"=", "STOREP_F", 5, true, &def_pointer, &def_float, &def_float, 12},
	{"=", "STOREP_V", 5, true, &def_pointer, &def_vector, &def_vector, 12},
	{"=", "STOREP_S", 5, true, &def_pointer, &def_string, &def_string, 12},
	{"=", "STOREP_ENT", 5, true, &def_pointer, &def_entity, &def_entity, 12},
	{"=", "STOREP_FLD", 5, true, &def_pointer, &def_field, &def_field, 12},
	{"=", "STOREP_FNC", 5, true, &def_pointer, &def_function, &def_function, 12},

	{"<RETURN>", "RETURN", -1, false, &def_void, &def_void, &def_void, 13},

	{"!", "NOT_F", -1, false, &def_float, &def_void, &def_float, 14},
	{"!", "NOT_V", -1, false, &def_vector, &def_void, &def_float, 14},
	{"!", "NOT_S", -1, false, &def_vector, &def_void, &def_float, 14},
	{"!", "NOT_ENT", -1, false, &def_entity, &def_void, &def_float, 14},
	{"!", "NOT_FNC", -1, false, &def_function, &def_void, &def_float, 14},

	{"<IF>", "IF", -1, false, &def_float, &def_float, &def_void, 15},

	{"<IFNOT>", "IFNOT", -1, false, &def_float, &def_float, &def_void, 16},

	{"<CALL0>", "CALL0", -1, false, &def_function, &def_void, &def_void, 17},
	{"<CALL1>", "CALL1", -1, false, &def_function, &def_void, &def_void, 18},
	{"<CALL2>", "CALL2", -1, false, &def_function, &def_void, &def_void, 19}, 
	{"<CALL3>", "CALL3", -1, false, &def_function, &def_void, &def_void, 20}, 
	{"<CALL4>", "CALL4", -1, false, &def_function, &def_void, &def_void, 21},
	{"<CALL5>", "CALL5", -1, false, &def_function, &def_void, &def_void, 22},
	{"<CALL6>", "CALL6", -1, false, &def_function, &def_void, &def_void, 23},
	{"<CALL7>", "CALL7", -1, false, &def_function, &def_void, &def_void, 24},
	{"<CALL8>", "CALL8", -1, false, &def_function, &def_void, &def_void, 25},

	{"<STATE>", "STATE", -1, false, &def_float, &def_float, &def_void, 26},

	{"<GOTO>", "GOTO", -1, false, &def_float, &def_void, &def_void, 27},

	{"&&", "AND", 6, false, &def_float, &def_float, &def_float, 28},

	{"||", "OR", 6, false, &def_float, &def_float, &def_float, 29},

	{"&", "BITAND", 2, false, &def_float, &def_float, &def_float, 30},

	{"|", "BITOR", 2, false, &def_float, &def_float, &def_float, 31},

	{"*=", "MULSTORE_F", 5, true,
		&def_float, &def_float, &def_float, 32+SPECIAL_ASSIGN_OP},
	{"*=", "MULSTORE_V", 5, true,
		&def_vector, &def_float, &def_vector, 32+SPECIAL_ASSIGN_OP},
	{"*=", "MULSTOREP_F", 5, true,
		&def_pointer, &def_float, &def_float, 32+SPECIAL_ASSIGN_OP},
	{"*=", "MULSTOREP_V", 5, true,
		&def_pointer, &def_float, &def_vector, 32+SPECIAL_ASSIGN_OP},

	{"/=", "DIVSTORE_F", 5, true,
		&def_float, &def_float, &def_float, 33+SPECIAL_ASSIGN_OP},
	{"/=", "DIVSTOREP_F", 5, true,
		&def_pointer, &def_float, &def_float, 33+SPECIAL_ASSIGN_OP},

	{"+=", "ADDSTORE_F", 5, true,
		&def_float, &def_float, &def_float, 34+SPECIAL_ASSIGN_OP},
	{"+=", "ADDSTORE_V", 5, true,
		&def_vector, &def_vector, &def_vector, 34+SPECIAL_ASSIGN_OP},
	{"+=", "ADDSTOREP_F", 5, true,
		&def_pointer, &def_float, &def_float, 34+SPECIAL_ASSIGN_OP},
	{"+=", "ADDSTOREP_V", 5, true,
		&def_pointer, &def_vector, &def_vector, 34+SPECIAL_ASSIGN_OP},

	{"-=", "SUBSTORE_F", 5, true,
		&def_float, &def_float, &def_float, 35+SPECIAL_ASSIGN_OP},
	{"-=", "SUBSTORE_V", 5, true,
		&def_vector, &def_vector, &def_vector, 35+SPECIAL_ASSIGN_OP},
	{"-=", "SUBSTOREP_F", 5, true,
		&def_pointer, &def_float, &def_float, 35+SPECIAL_ASSIGN_OP},
	{"-=", "SUBSTOREP_V", 5, true,
		&def_pointer, &def_vector, &def_vector, 35+SPECIAL_ASSIGN_OP},

	{"<FETCH_GBL_F>", "FETCH_GBL_F",
		-1, false, &def_float, &def_float, &def_float, 36},
	{"<FETCH_GBL_V>", "FETCH_GBL_V",
		-1, false, &def_vector, &def_float, &def_vector, 36},
	{"<FETCH_GBL_S>", "FETCH_GBL_S",
		-1, false, &def_string, &def_float, &def_string, 36},
	{"<FETCH_GBL_E>", "FETCH_GBL_E",
		-1, false, &def_entity, &def_float, &def_entity, 36},
	{"<FETCH_GBL_FNC>", "FETCH_GBL_FNC",
		-1, false, &def_function, &def_float, &def_function, 36},

	{"<CSTATE>", "CSTATE", -1, false,
		&def_float, &def_float, &def_void, 37},

	{"<CWSTATE>", "CWSTATE", -1, false,
		&def_float, &def_float, &def_void, 38},

	{"<THINKTIME>", "THINKTIME", -1, false,
		&def_entity, &def_float, &def_void, 39},

	{"(+)", "BITSET", 5, true, &def_float, &def_float, &def_float, 40},
	{"(+)", "BITSETP", 5, true, &def_pointer, &def_float, &def_float, 40},
	{"(-)", "BITCLR", 5, true, &def_float, &def_float, &def_float, 41},
	{"(-)", "BITCLRP", 5, true, &def_pointer, &def_float, &def_float, 41},

	{"<RAND0>", "RAND0", -1, false,
		&def_void, &def_void, &def_void, 42},
	{"<RAND1>", "RAND1", -1, false,
		&def_float, &def_void, &def_void, 43},
	{"<RAND2>", "RAND2", -1, false,
		&def_float, &def_float, &def_void, 44},
	{"<RANDV0>", "RANDV0", -1, false,
		&def_void, &def_void, &def_void, 42},
	{"<RANDV1>", "RANDV1", -1, false,
		&def_vector, &def_void, &def_void, 43},
	{"<RANDV2>", "RANDV2", -1, false,
		&def_vector, &def_vector, &def_void, 44},

	{"<SWITCH_F>", "SWITCH_F", -1, false,
		&def_void, &def_void, &def_void, 45},
	{"<SWITCH_V>", "SWITCH_V", -1, false,
		&def_void, &def_void, &def_void, 46},
	{"<SWITCH_S>", "SWITCH_S", -1, false,
		&def_void, &def_void, &def_void, 47},
	{"<SWITCH_E>", "SWITCH_E", -1, false,
		&def_void, &def_void, &def_void, 48},
	{"<SWITCH_FNC>", "SWITCH_FNC", -1, false,
		&def_void, &def_void, &def_void, 49},

	{"<CASE>", "CASE", -1, false,
		&def_void, &def_void, &def_void, 50},
	{"<CASERANGE>", "CASERANGE", -1, false,
		&def_void, &def_void, &def_void, 51},

/*
	{"<FETCH_FLD_F>", "FETCH_FLD_F",
		-1, false, &def_float, &def_float, &def_float, 36},
	{"<FETCH_FLD_V>", "FETCH_FLD_V",
		-1, false, &def_vector, &def_float, &def_vector, 36},
	{"<FETCH_FLD_S>", "FETCH_FLD_S",
		-1, false, &def_string, &def_float, &def_string, 36},
	{"<FETCH_FLD_E>", "FETCH_FLD_E",
		-1, false, &def_entity, &def_float, &def_entity, 36},
	{"<FETCH_FLD_FNC>", "FETCH_FLD_FNC",
		-1, false, &def_function, &def_float, &def_function, 36},
*/

	{NULL}
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// CO_Init
//
//==========================================================================

void CO_Init(void)
{
	InitHashTable();
}

//==========================================================================
//
// InitHashTable
//
//==========================================================================

static void InitHashTable(void)
{
	memset(HashTable, 0, sizeof(HashTable));
}

//==========================================================================
//
// CO_CompileFile
//
//==========================================================================

qboolean CO_CompileFile(char *fileText, char *fileName)
{
	qboolean inProgress;

	FrameIndex = -1;
	inProgress = false;
	s_file = CopyString(fileName);

	// Ugly hack to prevent longjmp failure from within
	// LX_NewSourceFile().
	if(setjmp(pr_parse_abort))
	{
		LX_ErrorRecovery();
		if(pr_token_type == tt_eof)
		{
			return false;
		}
		pr_error_count++;
		inProgress = true;
	}

	if(inProgress == false)
	{
		LX_NewSourceFile(fileText);
	}

	while(pr_token_type != tt_eof)
	{
		if(setjmp(pr_parse_abort))
		{
			if(++pr_error_count >= MAX_ERRORS)
			{
				printf("stopped at %d errors\n", pr_error_count);
				return false;
			}
			LX_ErrorRecovery();
			if(pr_token_type == tt_eof)
			{
				return false;
			}
		}
		pr_scope = NULL;
		CO_ParseDefs();
	}
	return (pr_error_count == 0);
}

//==========================================================================
//
// CO_GenCode
//
// Emits a primitive statement, returning the def it places its value in.
//
//==========================================================================

def_t *CO_GenCode(opcode_t *op, def_t *var_a, def_t *var_b)
{
	dstatement_t *statement;
	def_t *var_c;

	statement = &statements[numstatements];
	numstatements++;

	statement_linenums[statement-statements] = lx_SourceLine;
	statement->op = op - pr_opcodes;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	if(hcc_OptimizeTemps)
	{
		NoteTempRef(statement-statements, TEMPREF_A, var_a);
		NoteTempRef(statement-statements, TEMPREF_B, var_b);
	}
	NoteReturnUse(statement-statements, TEMPREF_A, var_a);
	NoteReturnUse(statement-statements, TEMPREF_B, var_b);
	if(statement->op >= OP_RAND0 && statement->op <= OP_RANDV2)
	{
		LastReturnWrite = statement-statements;
	}

	if(op->type_c == &def_void)
	{
		statement->c = 0;
		return NULL;
	}

	if(op->right_associative)
	{
		if(op->tag < SPECIAL_ASSIGN_OP)
		{ // Return right side operand for normal assigments.
			statement->c = 0;
			return var_a;
		}
		if(op->type_a != &def_pointer)
		{
			// Return left side operand for non-pointer special
			// assignments.  Special assignments to pointers need to
			// allocate a register because the result is not accessible
			// if only stored in an entity.
			statement->c = 0;
			return var_b;
		}
	}

	// Allocate the result register.
	var_c = malloc(sizeof(def_t));
	memset(var_c, 0, sizeof(def_t));
	var_c->type = op->type_c->type;

	if(hcc_OptimizeTemps)
	{ // [2026-11-12 v0.9.8] Komi - the register is assigned once the temp's lifetime is known
		var_c->ofs = NewTemp(var_c, statement-statements,
			type_size[op->type_c->type->type]);
		statement->c = var_c->ofs;
		TempRefs[statement-statements] |= TEMPREF_C;
		return var_c;
	}

	var_c->ofs = numpr_globals;
	statement->c = numpr_globals;
	numpr_globals += type_size[op->type_c->type->type];
	CO_CheckRegisters();

	return var_c;
}

//==========================================================================
//
// CO_GenCodeDirect
//
//==========================================================================

void CO_GenCodeDirect(opcode_t *op, def_t *var_a, def_t *var_b,
	def_t *var_c)
{
	dstatement_t *statement;

	statement = &statements[numstatements];
	numstatements++;

	statement_linenums[statement-statements] = lx_SourceLine;
	statement->op = op - pr_opcodes;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	statement->c = var_c ? var_c->ofs : 0;
	if(hcc_OptimizeTemps)
	{
		NoteTempRef(statement-statements, TEMPREF_A, var_a);
		NoteTempRef(statement-statements, TEMPREF_B, var_b);
		NoteTempRef(statement-statements, TEMPREF_C, var_c);
	}
	NoteReturnUse(statement-statements, TEMPREF_A, var_a);
	NoteReturnUse(statement-statements, TEMPREF_B, var_b);
	NoteReturnUse(statement-statements, TEMPREF_C, var_c);
	if(statement->op >= OP_CALL0 && statement->op <= OP_CALL8)
	{
		LastReturnWrite = statement-statements;
	}
}

//==========================================================================
//
// Overwritten return values
//
// [2026-11-12 v0.9.8] Komi - Every call and every random() leaves its
// result in OFS_RETURN, and HCC used it from there. When a second call ran
// before the first result was used, as in f() + g(), the first result was
// lost and the second was read in its place. Now each result is tracked
// until a statement uses it; just before a call or random() would overwrite
// one that is still waiting, it is copied to a temp of its own, with a
// warning. Code where this never happens compiles exactly as before.
// -noretfix keeps HCC's original behavior.
//
//==========================================================================

int hcc_KeepReturnBug;

static def_t **Pending; // results still waiting in OFS_RETURN, oldest first
static int NumPending;
static int MaxPending;

static char *ReturnName(def_t *d)
{
	static char buf[MAX_NAME+16];

	if(d->name == NULL)
	{
		return "a call";
	}
	sprintf(buf, "%s()", d->name);
	return buf;
}

def_t *CO_NewReturn(type_t *type, char *name)
{
	def_t *d;

	if(hcc_KeepReturnBug)
	{
		def_ret.type = type;
		return &def_ret;
	}
	d = malloc(sizeof(def_t));
	memset(d, 0, sizeof(def_t));
	d->type = type;
	d->ofs = OFS_RETURN;
	d->name = name;
	d->retStatement = numstatements;
	if(type->type != ev_void)
	{
		if(NumPending == MaxPending)
		{
			MaxPending = MaxPending ? MaxPending*2 : 16;
			Pending = realloc(Pending, MaxPending*sizeof(def_t *));
		}
		Pending[NumPending++] = d;
	}
	return d;
}

static void NoteReturnUse(int statement, int bit, def_t *d)
{
	int i;

	if(d == NULL || d->retStatement == 0
		|| !(GlobalOperands(statements[statement].op) & bit))
	{
		return;
	}
	for(i = 0; i < NumPending; i++)
	{
		if(Pending[i] == d)
		{
			for(NumPending--; i < NumPending; i++)
			{
				Pending[i] = Pending[i+1];
			}
			return;
		}
	}
	// Used before (an assignment's value) and used again after another call
	if(LastReturnWrite != d->retStatement-1)
	{
		MS_ParseWarning("the result of %s is used after a later call "
			"overwrote it (not fixed)", ReturnName(d));
	}
}

static void KeepReturn(def_t *d)
{
	int op;
	int size;

	switch(d->type->type)
	{
	case ev_vector:
		op = OP_STORE_V;
		break;
	case ev_string:
		op = OP_STORE_S;
		break;
	case ev_entity:
		op = OP_STORE_ENT;
		break;
	case ev_field:
		op = OP_STORE_FLD;
		break;
	case ev_function:
		op = OP_STORE_FNC;
		break;
	default:
		op = OP_STORE_F;
		break;
	}
	MS_ParseWarning("kept the result of %s in a temp: a later call "
		"in this expression would overwrite it", ReturnName(d));
	size = type_size[d->type->type];
	d->retStatement = 0;
	if(hcc_OptimizeTemps)
	{
		d->ofs = NewTemp(d, numstatements, size);
	}
	else
	{
		d->ofs = numpr_globals;
		numpr_globals += size;
		CO_CheckRegisters();
	}
	CO_GenCode(&pr_opcodes[op], &def_ret, d);
}

// Called just before a statement that writes OFS_RETURN; a, b and c are
// its operands, which it reads before the write.
void CO_KeepReturns(def_t *a, def_t *b, def_t *c)
{
	int i, n;
	def_t *d;

	n = 0;
	for(i = 0; i < NumPending; i++)
	{
		d = Pending[i];
		if(d == a || d == b || d == c)
		{
			Pending[n++] = d;
			continue;
		}
		KeepReturn(d);
	}
	NumPending = n;
}

// A statement is complete: a result nobody used is discarded.
void CO_ClearReturns(void)
{
	NumPending = 0;
}

//==========================================================================
//
// CO_CheckRegisters
//
// [2026-11-12 v0.9.8] Komi - Statement operands are 16 bits, so a progs
// with more globals than that cannot be addressed. Fail instead of writing
// a progs.dat that no engine can run.
//
//==========================================================================

void CO_CheckRegisters(void)
{
	if(numpr_globals > MAX_REGS)
	{
		MS_Error("too many globals: %d, the progs format allows %d",
			numpr_globals, MAX_REGS);
	}
}

//==========================================================================
//
// Temporary register allocation (-ot)
//
// [2026-11-12 v0.9.8] Komi - HCC gives every expression result its own
// global and never reuses it. With -ot the code is generated exactly as
// before, and only the registers of the temps change:
//
// - A temp that is not live across a CALL, and whose lifetime holds no
//   jump, can share a cell of one program-wide pool. No QC code other than
//   its own straight-line statements runs while it is live.
// - Every other temp keeps a register of its own, allocated where HCC
//   would have put it relative to the function's locals: temps from before
//   the function's last local declaration are inside the range the engine
//   saves on entry and restores on exit, later ones are not. Recursion
//   behaves as before.
// - Case values, and OP_MULSTOREP_V results (the engine writes only the
//   first of their three cells), keep a register of their own too.
//
// Until a temp's register is known, statements hold its index in Temps,
// flagged in TempRefs. Pool cells are flagged in PoolRefs and get their
// final register once the pool's size is known, after the last function.
//
//==========================================================================

typedef struct
{
	def_t *def;
	int defStatement;
	int lastUse;
	int size;
	qboolean pinned;
	int ofs; // own register, or -1 for a pool cell
	int cell;
} tempinfo_t;

typedef struct
{
	int first;
	int last;
} tempspan_t;

int hcc_OptimizeTemps;

static tempinfo_t *Temps;
static int NumTemps;
static int MaxTemps;
static int SegmentStart;
static int FunctionStart;
static unsigned char PoolRefs[MAX_STATEMENTS];
static int *PoolBusy; // last statement using each cell, current segment
static int PoolSize;
static int MaxPoolCells;
static tempspan_t *Spans; // pooled lifetimes of the current function
static int NumSpans;
static int MaxSpans;
static int TotalOwnTemps;
static int TotalPooledTemps;

// [2026-11-12 v0.9.8] Komi - operands of each opcode that hold a global (the
// rest are jump offsets)
static int GlobalOperands(int op)
{
	switch(op)
	{
	case OP_GOTO:
		return 0;
	case OP_IF:
	case OP_IFNOT:
	case OP_SWITCH_F:
	case OP_SWITCH_V:
	case OP_SWITCH_S:
	case OP_SWITCH_E:
	case OP_SWITCH_FNC:
	case OP_CASE:
		return TEMPREF_A;
	case OP_CASERANGE:
		return TEMPREF_A|TEMPREF_B;
	default:
		return TEMPREF_A|TEMPREF_B|TEMPREF_C;
	}
}

static void NoteTempRef(int statement, int bit, def_t *d)
{
	if(d == NULL || !(GlobalOperands(statements[statement].op) & bit))
	{
		return;
	}
	if(d->tempIndex < 0)
	{
		MS_Error("internal error: temp used after its lifetime");
	}
	if(d->tempIndex == 0)
	{
		return;
	}
	TempRefs[statement] |= bit;
	Temps[d->tempIndex-1].lastUse = statement;
}

static int NewTemp(def_t *d, int statement, int size)
{
	tempinfo_t *t;

	if(NumTemps == 32767)
	{
		MS_Error("internal error: too many temps between declarations");
	}
	if(NumTemps == MaxTemps)
	{
		MaxTemps = MaxTemps ? MaxTemps*2 : 1024;
		Temps = realloc(Temps, MaxTemps*sizeof(tempinfo_t));
	}
	t = &Temps[NumTemps];
	memset(t, 0, sizeof(*t));
	t->def = d;
	t->defStatement = statement;
	t->lastUse = statement;
	t->size = size;
	d->tempIndex = NumTemps+1;
	return NumTemps++;
}

void CO_PinTemp(def_t *d)
{
	if(d != NULL && d->tempIndex > 0)
	{
		Temps[d->tempIndex-1].pinned = true;
	}
}

static int FindPoolCells(int size, int first)
{
	int i, j;

	for(i = 0; ; i++)
	{
		for(j = 0; j < size; j++)
		{
			if(i+j < PoolSize && PoolBusy[i+j] >= first)
			{
				break;
			}
		}
		if(j == size)
		{
			break;
		}
	}
	while(i+size > PoolSize)
	{
		if(PoolSize == MaxPoolCells)
		{
			MaxPoolCells = MaxPoolCells ? MaxPoolCells*2 : 256;
			PoolBusy = realloc(PoolBusy, MaxPoolCells*sizeof(int));
		}
		PoolBusy[PoolSize++] = -1;
	}
	return i;
}

static void PatchOperand(short *operand, int s, int bit)
{
	tempinfo_t *t;

	if(!(TempRefs[s] & bit))
	{
		return;
	}
	t = &Temps[(unsigned short)*operand];
	if(t->ofs >= 0)
	{
		*operand = t->ofs;
	}
	else
	{
		*operand = t->cell;
		PoolRefs[s] |= bit;
	}
}

//==========================================================================
//
// CO_TempFlush
//
// [2026-11-12 v0.9.8] Komi - Assigns registers to the temps made since the
// last flush. Called before each local declaration (no temp is live there)
// and at function end.
//
//==========================================================================

void CO_TempFlush(void)
{
	int i, k, s, op;
	int last;
	qboolean own;
	tempinfo_t *t;

	if(!hcc_OptimizeTemps)
	{
		return;
	}
	for(i = 0; i < PoolSize; i++)
	{
		PoolBusy[i] = -1;
	}
	for(i = 0; i < NumTemps; i++)
	{
		t = &Temps[i];
		last = t->lastUse;
		own = t->pinned
			|| statements[t->defStatement].op == OP_MULSTOREP_V;
		for(s = t->defStatement+1; !own && s < last; s++)
		{
			op = statements[s].op;
			if((op >= OP_CALL0 && op <= OP_CALL8)
				|| GlobalOperands(op) != (TEMPREF_A|TEMPREF_B|TEMPREF_C))
			{
				own = true;
			}
		}
		if(own)
		{
			t->ofs = numpr_globals;
			numpr_globals += t->size;
			CO_CheckRegisters();
			TotalOwnTemps++;
			continue;
		}
		t->ofs = -1;
		t->cell = FindPoolCells(t->size, t->defStatement);
		for(k = 0; k < t->size; k++)
		{
			PoolBusy[t->cell+k] = last;
		}
		if(NumSpans == MaxSpans)
		{
			MaxSpans = MaxSpans ? MaxSpans*2 : 256;
			Spans = realloc(Spans, MaxSpans*sizeof(tempspan_t));
		}
		Spans[NumSpans].first = t->defStatement;
		Spans[NumSpans].last = last;
		NumSpans++;
		TotalPooledTemps++;
	}
	for(s = SegmentStart; s < numstatements; s++)
	{
		if(TempRefs[s])
		{
			PatchOperand(&statements[s].a, s, TEMPREF_A);
			PatchOperand(&statements[s].b, s, TEMPREF_B);
			PatchOperand(&statements[s].c, s, TEMPREF_C);
			TempRefs[s] = 0;
		}
	}
	for(i = 0; i < NumTemps; i++)
	{
		t = &Temps[i];
		if(t->ofs >= 0)
		{ // [2026-11-12 v0.9.8] Komi - case values are still referenced by their def
			t->def->ofs = t->ofs;
			t->def->tempIndex = 0;
		}
		else
		{
			t->def->ofs = 0;
			t->def->tempIndex = -1;
		}
	}
	NumTemps = 0;
	SegmentStart = numstatements;
}

static void TempBeginFunction(void)
{
	NumTemps = 0;
	NumSpans = 0;
	SegmentStart = FunctionStart = numstatements;
}

// [2026-11-12 v0.9.8] Komi - A jump into a pooled lifetime would read a cell
// its def never wrote. HCC's grammar can't produce one; check anyway, once
// all jumps are patched.
static void TempEndFunction(void)
{
	int i, s, target;
	dstatement_t *st;

	if(!hcc_OptimizeTemps)
	{
		return;
	}
	CO_TempFlush();
	for(s = FunctionStart; s < numstatements; s++)
	{
		st = &statements[s];
		switch(st->op)
		{
		case OP_GOTO:
			target = s+st->a;
			break;
		case OP_IF:
		case OP_IFNOT:
		case OP_SWITCH_F:
		case OP_SWITCH_V:
		case OP_SWITCH_S:
		case OP_SWITCH_E:
		case OP_SWITCH_FNC:
		case OP_CASE:
			target = s+st->b;
			break;
		case OP_CASERANGE:
			target = s+st->c;
			break;
		default:
			continue;
		}
		for(i = 0; i < NumSpans; i++)
		{
			if(target > Spans[i].first && target <= Spans[i].last)
			{
				MS_Error("internal error: jump into a temp's lifetime"
					" in %s", pr_scope ? pr_scope->name : "?");
			}
		}
	}
}

//==========================================================================
//
// CO_TempEndProgram
//
// [2026-11-12 v0.9.8] Komi - Places the pool after everything else, outside
// every function's locals. Two spare registers follow it: the engine copies
// call arguments and return values three cells wide, so a float in the top
// pool cell would otherwise read past the end of the globals.
//
//==========================================================================

void CO_TempEndProgram(qboolean quiet)
{
	int s;
	int base;

	if(!hcc_OptimizeTemps)
	{
		return;
	}
	base = numpr_globals;
	numpr_globals += PoolSize+2;
	CO_CheckRegisters();
	for(s = 0; s < numstatements; s++)
	{
		if(PoolRefs[s] & TEMPREF_A)
		{
			statements[s].a = base+statements[s].a;
		}
		if(PoolRefs[s] & TEMPREF_B)
		{
			statements[s].b = base+statements[s].b;
		}
		if(PoolRefs[s] & TEMPREF_C)
		{
			statements[s].c = base+statements[s].c;
		}
	}
	if(!quiet)
	{
		printf("          temps: %d pooled in %d registers, %d own\n",
			TotalPooledTemps, PoolSize, TotalOwnTemps);
	}
}

//==========================================================================
//
// CO_ParseImmediate
//
// Looks for a preexisting constant.
//
//==========================================================================

def_t *CO_ParseImmediate(void)
{
	def_t *cn = NULL;
	int index = 0;
	struct hash_element *cell = NULL;

	if(pr_immediate_type == &type_string)
	{
		index = MS_Hash(pr_immediate_string);
	}
	else if(pr_immediate_type == &type_float)
	{
		char tmpchar[40];
		sprintf(tmpchar, "%.4f", pr_immediate._float);
		index = MS_Hash(tmpchar);
	}
	else if(pr_immediate_type == &type_vector)
	{
		char tmpchar[80];
		sprintf(tmpchar, "%.4f,%.4f,%.4f",
			pr_immediate.vector[0], pr_immediate.vector[1],
			pr_immediate.vector[2]);
		index = MS_Hash(tmpchar);
	}
	else
	{
		MS_ParseError("weird immediate type");
	}

	for(cell = HashTable[index]; cell != NULL; cell = cell->next)
	{
		cn = cell->def;
		if(!cn->initialized || (cn->type != pr_immediate_type))
		{
			continue;
		}
		if(pr_immediate_type == &type_string)
		{
			if(!STRCMP(G_STRING(cn->ofs), pr_immediate_string))
			{
				return cn;
			}
		}
		else if(pr_immediate_type == &type_float)
		{
			if(G_FLOAT(cn->ofs) == pr_immediate._float)
			{
				return cn;
			}
		}
		else if(pr_immediate_type == &type_vector)
		{
			if((G_FLOAT(cn->ofs) == pr_immediate.vector[0])
				&& (G_FLOAT(cn->ofs+1) == pr_immediate.vector[1])
				&& (G_FLOAT(cn->ofs+2) == pr_immediate.vector[2]))
			{
				return cn;
			}
		}
		else
		{
			MS_ParseError("weird immediate type");
		}
	}

	if(hcc_OptimizeImmediates)
	{ // Check for a constant with the same value
		for(cn = pr.def_head.next; cn; cn = cn->next)
		{
			if(!cn->initialized || (cn->type != pr_immediate_type))
			{
				continue;
			}
			if(pr_immediate_type == &type_string)
			{
				if(!STRCMP(G_STRING(cn->ofs), pr_immediate_string))
				{
					cell = malloc(sizeof(struct hash_element));
					cell->next = HashTable[index];
					cell->def = cn;
					HashTable[index] = cell;
					return cn;
				}
			}
			else if(pr_immediate_type == &type_float)
			{
				if(G_FLOAT(cn->ofs) == pr_immediate._float)
				{
					cell = malloc(sizeof(struct hash_element));
					cell->next = HashTable[index];
					cell->def = cn;
					HashTable[index] = cell;
					return cn;
				}
			}
			else if(pr_immediate_type == &type_vector)
			{
				if((G_FLOAT(cn->ofs) == pr_immediate.vector[0])
					&& ( G_FLOAT(cn->ofs+1) == pr_immediate.vector[1])
					&& ( G_FLOAT(cn->ofs+2) == pr_immediate.vector[2]))
				{
					cell = malloc(sizeof(struct hash_element));
					cell->next = HashTable[index];
					cell->def = cn;
					HashTable[index] = cell;
					return cn;
				}
			}
			else
			{
				MS_ParseError("weird immediate type");
			}
		}
	}

	// Allocate a new one
	cn = malloc(sizeof(def_t));
	memset(cn, 0, sizeof(*cn));
	cn->next = NULL;
	pr.def_tail->next = cn;
	pr.def_tail = cn;

	cell = malloc(sizeof(struct hash_element));
	cell->next = HashTable[index];
	cell->def = cn;
	HashTable[index] = cell;

	cn->type = pr_immediate_type;
	//cn->name = "IMMEDIATE";
	cn->name = "I+";
	cn->initialized = 1;
	cn->scope = NULL; // Always share immediates

	// Copy the immediate to the global area
	cn->ofs = numpr_globals;
	pr_global_defs[cn->ofs] = cn;
	numpr_globals += type_size[pr_immediate_type->type];
	CO_CheckRegisters();
	if(pr_immediate_type == &type_string)
	{
		pr_immediate.string = CopyString(pr_immediate_string);
	}
	memcpy(pr_globals+cn->ofs, &pr_immediate,
		4*type_size[pr_immediate_type->type]);

	return cn;
}

//==========================================================================
//
// ParseState
//
// [function]
// [$frame function]
// [++ $frame1..$frame2]
// [-- $frame2..$frame1]
// [++(w) $frame1..$frame2]
// [--(w) $frame2..$frame1]
//
//==========================================================================

static void ParseState(void)
{
	char *name;
	def_t *def;
	def_t *s1, *s2;
	int frame1, frame2;
	int direction;
	qboolean weapon;

	if(pr_token_type == tt_name)
	{
		FrameIndex++;

		// Stuff a float for CO_ParseImmediate()
		pr_token_type = tt_immediate;
		pr_immediate_type = &type_float;
		pr_immediate._float = (float)FrameIndex;
		s1 = CO_ParseImmediate();

		pr_token_type = tt_name;
		name = PR_ParseName();
		def = PR_GetDef(&type_function, name, 0, true);
		LX_Require("]");
		CO_GenCode(&pr_opcodes[OP_STATE], s1, def);
		return;
	}
	else if(pr_tokenclass == TK_INC || pr_tokenclass == TK_DEC)
	{
		direction = pr_tokenclass;
		weapon = false;
		LX_Fetch();
		if(TK_CHECK(TK_LPAREN))
		{
			if(LX_Check("w"))
			{
				LX_Fetch();
			}
			else
			{
				LX_Require("W");
			}
			LX_Require(")");
			weapon = true;
		}
		if(pr_token_type != tt_immediate
			|| pr_immediate_type != &type_float)
		{
			MS_ParseError("state frame must be a number");
		}
		frame1 = (int)pr_immediate._float;
		s1 = CO_ParseImmediate();
		LX_Fetch();
		LX_Require("..");
		if(pr_token_type != tt_immediate
			|| pr_immediate_type != &type_float)
		{
			MS_ParseError("state frame must be a number");
		}
		frame2 = (int)pr_immediate._float;
		s2 = CO_ParseImmediate();
		LX_Fetch();
		if(direction == TK_INC)
		{
			if(frame1 > frame2)
			{
				MS_ParseError("bad frame order in state cycle");
			}
		}
		else if(frame1 < frame2)
		{
			MS_ParseError("bad frame order in state cycle");
		}
		LX_Require("]");
		if(weapon == true)
		{
			CO_GenCode(&pr_opcodes[OP_CWSTATE], s1, s2);
		}
		else
		{
			CO_GenCode(&pr_opcodes[OP_CSTATE], s1, s2);
		}
		return;
	}

	if(pr_token_type != tt_immediate
		|| pr_immediate_type != &type_float)
	{
		MS_ParseError("state frame must be a number");
	}
	FrameIndex = (int)pr_immediate._float;
	s1 = CO_ParseImmediate();
	LX_Fetch();
	if(pr_tokenclass == TK_COMMA)
	{
		LX_Fetch();
	}
	name = PR_ParseName();
	def = PR_GetDef(&type_function, name, 0, true);

	LX_Require("]");
	CO_GenCode(&pr_opcodes[OP_STATE], s1, def);
}

//==========================================================================
//
// ParseImmediateStatements
//
//==========================================================================

static function_t *ParseImmediateStatements(type_t *type)
{
	int i;
	function_t *f;
	def_t *defs[MAX_PARMS];
	def_t *scopeDef;
	def_t *searchDef;

	f = malloc(sizeof(function_t));

	// Check for builtin function definition
	if(TK_CHECK(TK_COLON))
	{
		if(pr_token_type != tt_immediate
			|| pr_immediate_type != &type_float
			|| pr_immediate._float != (int)pr_immediate._float)
		{
			MS_ParseError("bad builtin immediate");
		}
		f->builtin = (int)pr_immediate._float;
		LX_Fetch();
		return f;
	}

	f->builtin = 0;

	// Define the parms
	for(i = 0; i < type->num_parms; i++)
	{
		defs[i] = PR_GetDef(type->parm_types[i], pr_parm_names[i],
			pr_scope, true);
		f->parm_ofs[i] = defs[i]->ofs;
		if(i > 0 && f->parm_ofs[i] < f->parm_ofs[i-1])
		{
			MS_ParseError("bad parm order");
		}
	}

	f->code = numstatements;

	// Check for a state opcode
	if(TK_CHECK(TK_LBRACKET))
	{
		ParseState();
	}

	// Check for regular statements
	st_ReturnType = type->aux_type;
	st_ReturnParsed = false;
	if(TK_CHECK(TK_LBRACE))
	{
		scopeDef = pr_scope;
		searchDef = pr.def_tail;

		while(pr_tokenclass != TK_RBRACE)
		{
			ST_ParseStatement();
		}

		while((searchDef = searchDef->next) != NULL)
		{
			if(searchDef->scope == scopeDef
				&& searchDef->referenceCount == 0
				&& searchDef->parentVector == NULL)
			{
				MS_ParseWarning("unreferenced local variable '%s'",
					searchDef->name);
			}
		}
		if(type->aux_type->type != ev_void && st_ReturnParsed == false)
		{
			MS_ParseError("missing return");
		}

		LX_Fetch();
	}
	else if(type->aux_type->type != ev_void && st_ReturnParsed == false)
	{
		MS_ParseError("missing return");
	}

	// Emit an end of statements opcode
	CO_GenCode(pr_opcodes, 0, 0);

	return f;
}

//==========================================================================
//
// PR_GetDef
//
// If type is NULL, it will match any type.  If allocate is true, a new
// def will be allocated if it can't be found.
//
//==========================================================================

def_t *PR_GetDef(type_t *type, char *name, def_t *scope,
	qboolean allocate)
{
	def_t *def;
	def_t *elementDef;
	char element[MAX_NAME];

	char key[100];
	int index = 0;
	struct hash_element *cell = NULL;

	if(scope != NULL)
	{
		sprintf(key, "%s:%s", name, scope->name);
		index = MS_Hash(key);
	}
	else
	{
		index = MS_Hash(name);
	}

	for(cell = HashTable[index]; cell != NULL; cell = cell->next)
	{
		def = cell->def;
		if(!STRCMP(def->name, name))
		{ // Found a match
			if(def->scope && def->scope != scope)
			{ // In a different function
				continue;
			}
			if(type && def->type != type)
			{ // Type mismatch
				MS_ParseError("type mismatch on redeclaration of %s", name);
			}
			return def;
		}
	}

	if(scope != NULL)
	{
		index = MS_Hash(name);
		for(cell = HashTable[index]; cell != NULL; cell = cell->next)
		{
			def = cell->def;
			if(!STRCMP(def->name, name))
			{ // Found a match
				if(def->scope && def->scope != scope)
				{ // In a different function
					continue;
				}
				if(type && def->type != type)
				{ // Type mismatch
					MS_ParseError("type mismatch on redeclaration of %s",
						name);
				}
				return def;
			}
		}
	}

	if(allocate == false)
	{
		return NULL;
	}

	// Allocate a new def
	def = malloc(sizeof(def_t));
	memset(def, 0, sizeof(*def));
	def->next = NULL;
	pr.def_tail->next = def;
	pr.def_tail = def;

	// Add to hash table
	cell = malloc(sizeof(struct hash_element));
	cell->next = HashTable[index];
	cell->def = def;
	HashTable[index] = cell;

	def->name = malloc(strlen(name)+1);
	strcpy(def->name, name);
	def->type = type;

	def->scope = scope;

	def->ofs = numpr_globals;
	pr_global_defs[numpr_globals] = def;

	if(type->type == ev_vector)
	{ // Create vector element defs
		sprintf(element, "%s_x", name);
		elementDef = PR_GetDef(&type_float, element, scope, true);
		elementDef->parentVector = def;
		sprintf(element, "%s_y", name);
		elementDef = PR_GetDef(&type_float, element, scope, true);
		elementDef->parentVector = def;
		sprintf(element, "%s_z", name);
		elementDef = PR_GetDef(&type_float, element, scope, true);
		elementDef->parentVector = def;
	}
	else
	{
		numpr_globals += type_size[type->type];
		CO_CheckRegisters();
	}

	// [2026-11-13 v0.9.8] Komi - A field-typed parameter or local (".float fld") holds a field
	// reference passed in or assigned; only a global declaration makes a field.
	// Raven's HCC gave each one entity space and a field def of its own.
	if(type->type == ev_field && scope == NULL)
	{
		*(int *)&pr_globals[def->ofs] = pr.size_fields;
		if(type->aux_type->type == ev_vector)
		{ // Create vector element defs
			sprintf(element, "%s_x", name);
			PR_GetDef(&type_floatfield, element, scope, true);
			sprintf(element, "%s_y", name);
			PR_GetDef(&type_floatfield, element, scope, true);
			sprintf(element, "%s_z", name);
			PR_GetDef(&type_floatfield, element, scope, true);
		}
		else
		{
			pr.size_fields += type_size[type->aux_type->type];
		}
	}

	return def;
}

//==========================================================================
//
// CO_ParseDefs
//
//==========================================================================

void CO_ParseDefs(void)
{
	int i;
	char *name;
	def_t *def;
	type_t *type;
	int elementCount;
	gofs_t offset;

	type = PR_ParseType();

	if(type == &type_union)
	{
		if(pr_scope)
		{
			MS_ParseError("unions must be global");
		}
		ParseUnion();
		return;
	}

	if(pr_scope && (type->type == ev_field || type->type == ev_function))
	{
		MS_ParseError("fields and functions must be global");
	}

	do
	{
		name = PR_ParseName();
		if(TK_CHECK(TK_LPAREN))
		{
			ParseCStyleFunctionDef(name, type);
			return;
		}

		elementCount = 0;
		if(TK_CHECK(TK_LBRACKET))
		{
			def = GetArrayDef(name, type, &elementCount);
		}
		else
		{
			def = PR_GetDef(type, name, pr_scope, true);
		}

		// Check for initialization
		if(TK_CHECK(TK_ASSIGN))
		{
			if(def->initialized)
			{
				MS_ParseError("'%s' : redefinition", name);
			}
			if(type->type == ev_field)
			{
				MS_ParseError("fields cannot be initialized");
			}

			if(elementCount != 0)
			{
				LX_Require("{");
				i = 0;
				offset = def->ofs;
				do
				{
					if(pr_token_type != tt_immediate)
					{
						MS_ParseError("immediate type required for %s",
							name);
					}
					if(pr_immediate_type->type != type->type)
					{
						MS_ParseError("wrong immediate type for %s", name);
					}
					memcpy(pr_globals+offset, &pr_immediate,
						4*type_size[pr_immediate_type->type]);
					offset += type_size[pr_immediate_type->type];
					i++;
					LX_Fetch();
				} while(TK_CHECK(TK_COMMA));
				LX_Require("}");
				if(i != elementCount)
				{
					MS_ParseError("element count mismatch in array"
						" initialization");
				}
				def->initialized = 1;
				continue;
			}
			else if(type->type == ev_function)
			{
				ParseFunctionDef(def, type);
				continue;
			}
			else if(pr_immediate_type != type)
			{
				MS_ParseError("wrong immediate type for %s", name);
			}
			def->initialized = 1;
			memcpy(pr_globals+def->ofs, &pr_immediate,
				4*type_size[pr_immediate_type->type]);
			LX_Fetch();
		}
		else if(elementCount != 0 && type->type != ev_field)
		{
			memset(pr_globals+def->ofs, 0,
				elementCount*4*type_size[type->type]);
			def->initialized = 1;
		}
	} while(TK_CHECK(TK_COMMA));
	LX_Require(";");
}

//==========================================================================
//
// GetArrayDef
//
//==========================================================================

static def_t *GetArrayDef(char *name, type_t *type, int *elementCount)
{
	def_t *def;
	int count;
	int regCount;

	if(type->type == ev_field)
	{
		return GetFieldArrayDef(name, type, elementCount);
	}

	count = ParseArray(name, type);

	def = NewVarDef(name, type);

	// Precede the array register storage with the element count
	// for run-time boundary checking.
	G_INT(numpr_globals) = count-1;
	numpr_globals++;

	def->ofs = numpr_globals;

	regCount = count*type_size[type->type];
	do
	{
		pr_global_defs[numpr_globals] = def;
		numpr_globals++;
	} while(--regCount);
	CO_CheckRegisters();

	*elementCount = count;
	return def;
}

//==========================================================================
//
// GetFieldArrayDef
//
//==========================================================================

static def_t *GetFieldArrayDef(char *name, type_t *type,
	int *elementCount)
{
	def_t *def;
	int count;

	count = ParseArray(name, type);

	def = NewVarDef(name, type);

	def->ofs = numpr_globals;
	pr_global_defs[numpr_globals] = def;
	G_INT(numpr_globals) = pr.size_fields;
	numpr_globals++;
	CO_CheckRegisters();
	pr.size_fields += type_size[type->aux_type->type]*count;

	*elementCount = count;
	return def;
}

//==========================================================================
//
// NewVarDef
//
//==========================================================================

static def_t *NewVarDef(char *name, type_t *type)
{
	def_t *def;
	int index;
	struct hash_element *cell;
	char key[100];

	// Allocate the array def
	def = malloc(sizeof(def_t));
	memset(def, 0, sizeof(*def));
	def->next = NULL;
	pr.def_tail->next = def;
	pr.def_tail = def;

	// Add it to the hash table
	if(pr_scope != NULL)
	{
		sprintf(key, "%s:%s", name, pr_scope->name);
		index = MS_Hash(key);
	}
	else
	{
		index = MS_Hash(name);
	}
	cell = malloc(sizeof(struct hash_element));
	cell->next = HashTable[index];
	cell->def = def;
	HashTable[index] = cell;

	def->name = malloc(strlen(name)+1);
	strcpy(def->name, name);

	def->type = type;
	def->scope = pr_scope;

	return def;
}

//==========================================================================
//
// ParseArray
//
//==========================================================================

static int ParseArray(char *name, type_t *type)
{
	int count;
	etype_t t;

	t = type->type;
	if(t == ev_field)
	{
		t = type->aux_type->type;
	}
	if(t != ev_float && t != ev_vector && t != ev_string
		&& t != ev_entity && t != ev_function)
	{
		MS_ParseError("bad array type");
	}
	if(PR_GetDef(type, name, pr_scope, false) != NULL)
	{
		MS_ParseError("array redefinition");
	}
	if(pr_token_type != tt_immediate
		|| pr_immediate_type != &type_float
		|| pr_immediate._float != (int)pr_immediate._float)
	{
		MS_ParseError("subscript is not integral");
	}
	count = (int)pr_immediate._float;
	if(count < 1 || count > MAX_ARRAY_ELEMENTS)
	{
		MS_ParseError("bad subscript: %d", count);
	}
	LX_Fetch();
	LX_Require("]");
	return count;
}

//==========================================================================
//
// ParseUnion
//
//==========================================================================

static void ParseUnion(void)
{
	char *name;
	type_t *type;
	int startFieldOffset;
	int highestFieldOffset;

	LX_Require("{");
	startFieldOffset = pr.size_fields;
	highestFieldOffset = startFieldOffset;
	do
	{
		if(LX_CheckFetch("struct"))
		{
			pr.size_fields = startFieldOffset;
			LX_Require("{");
			do
			{
				type = ParseUnionType();
				do
				{
					name = PR_ParseName();
					PR_GetDef(type, name, pr_scope, true);
				} while(TK_CHECK(TK_COMMA));
				LX_Require(";");
			} while(pr_tokenclass != TK_RBRACE);
			LX_Require("}");
			LX_Require(";");
			if(pr.size_fields > highestFieldOffset)
			{
				highestFieldOffset = pr.size_fields;
			}
		}
		else
		{
			type = ParseUnionType();
			do
			{
				name = PR_ParseName();
				pr.size_fields = startFieldOffset;
				PR_GetDef(type, name, pr_scope, true);
				if(pr.size_fields > highestFieldOffset)
				{
					highestFieldOffset = pr.size_fields;
				}
			} while(TK_CHECK(TK_COMMA));
			LX_Require(";");
		}
	} while(pr_tokenclass != TK_RBRACE);
	LX_Require("}");
	LX_Require(";");
	pr.size_fields = highestFieldOffset;
}

//==========================================================================
//
// ParseUnionType
//
//==========================================================================

static type_t *ParseUnionType(void)
{
	type_t *type;
	type_t newType;

	type = PR_ParseType();
	if(type->type == ev_field)
	{
		MS_ParseError("union field types are implicit");
	}
	memset(&newType, 0, sizeof(newType));
	newType.type = ev_field;
	newType.aux_type = type;
	return PR_FindType(&newType);
}

//==========================================================================
//
// ParseFunctionDef
//
//==========================================================================

static void ParseFunctionDef(def_t *def, type_t *type)
{
	int i;
	function_t *f;
	dfunction_t *df;
	int locals_start;

	locals_start = locals_end = numpr_globals;
	pr_scope = def;
	TempBeginFunction();
	f = ParseImmediateStatements(type);
	TempEndFunction();
	pr_scope = NULL;
	def->initialized = 1;
	G_FUNCTION(def->ofs) = numfunctions;
	f->def = def;

	// Fill in the dfunction
	df = &functions[numfunctions];
	numfunctions++;
	if(f->builtin)
	{
		df->first_statement = -f->builtin;
		def->referenceCount++;	
	}
	else
	{
		df->first_statement = f->code;
	}
	df->s_name = CopyString(f->def->name);
	df->s_file = s_file;
	df->numparms =  f->def->type->num_parms;
	df->locals = locals_end-locals_start;
	df->parm_start = locals_start;
	for(i = 0; i < df->numparms; i++)
	{
		df->parm_size[i] = type_size[f->def->type->parm_types[i]->type];
	}
}

//==========================================================================
//
// ParseCStyleFunctionDef
//
//==========================================================================

static void ParseCStyleFunctionDef(char *funcName, type_t *type)
{
	int i;
	char *name;
	type_t newType;
	type_t *funcType;
	def_t *def;
	function_t *f;
	dfunction_t *df;
	int locals_start;
	char funcIdent[MAX_NAME];
	int initClass;

	strcpy(funcIdent, funcName);
	memset(&newType, 0, sizeof(newType));
	newType.type = ev_function;
	newType.aux_type = type; // Return type
	newType.num_parms = 0;
	if(!TK_CHECK(TK_RPAREN))
	{
		if(TK_CHECK(TK_ELLIPSIS))
		{ // Variable args
			newType.num_parms = -1;
		}
		else if(!LX_CheckFetch("void"))
		{
			do
			{
				type = PR_ParseType();
				name = PR_ParseName();
				strcpy(pr_parm_names[newType.num_parms], name);
				newType.parm_types[newType.num_parms] = type;
				newType.num_parms++;
			} while(TK_CHECK(TK_COMMA));
		}
		LX_Require(")");
	}

	funcType = PR_FindType(&newType);
	def = PR_GetDef(funcType, funcIdent, pr_scope, true);

	if(def->initialized)
	{
		MS_ParseError("%s redeclared", funcName);
	}

	if(TK_TEST(TK_LBRACE)
		|| TK_TEST(TK_LBRACKET)
		|| TK_TEST(TK_COLON))
	{
		initClass = pr_tokenclass;
		if(def->initialized)
		{
			MS_ParseError("%s redeclared", name);
		}
		locals_start = locals_end = numpr_globals;
		pr_scope = def;
		TempBeginFunction();
		f = ParseImmediateStatements(funcType);
		TempEndFunction();
		pr_scope = NULL;
		def->initialized = 1;
		G_FUNCTION(def->ofs) = numfunctions;
		f->def = def;
		df = &functions[numfunctions];
		numfunctions++;
		if(f->builtin)
		{
			df->first_statement = -f->builtin;
			def->referenceCount++;
		}
		else
		{
			df->first_statement = f->code;
		}
		df->s_name = CopyString(f->def->name);
		df->s_file = s_file;
		df->numparms =  f->def->type->num_parms;
		df->locals = locals_end - locals_start;
		df->parm_start = locals_start;
		for(i = 0; i < df->numparms; i++)
		{
			df->parm_size[i] =
				type_size[f->def->type->parm_types[i]->type];
		}
		if(initClass == TK_COLON)
		{
			LX_Require(";");
		}
	}
	else
	{
		LX_Require(";");
	}
}
