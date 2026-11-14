
//**************************************************************************
//**
//** expr.c
//**
//** $Header: /H2 Mission Pack/Tools/hcc/expr.c 3     3/04/98 12:29a Jmonroe $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "hcc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static def_t *Term(void);
static def_t *ParseFunctionCall(def_t *func);
static def_t *ParseIntrinsicFunc(char *name);
static void PrecacheSound(def_t *e, int ch);
static void PrecacheModel(def_t *e, int ch);
static void PrecacheFile(def_t *e, int ch);
static def_t *MakeImmediate(type_t *type, float x, float y, float z);
static def_t *Truth(def_t *e);
static def_t *Reciprocal(def_t *e);
static opcode_t *FteCoerce(opcode_t *first, def_t **e, def_t **e2);
static def_t *AddressOf(def_t *e);
static def_t *Increment(def_t *e, int tokenclass, qboolean postfix);
static def_t *VectorConstructor(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int ex_FunctionCallCount;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int TokenToOp[] =
{
	OP_DONE,			// TK_NONE
	OP_DONE,			// TK_SEMICOLON
	OP_DONE,			// TK_LPAREN
	OP_DONE,			// TK_RPAREN
	OP_DONE,			// TK_COMMA
	OP_ADD_F,			// TK_PLUS
	OP_MUL_F,			// TK_ASTERISK
	OP_DIV_F,			// TK_SLASH
	OP_DONE,			// TK_LBRACE
	OP_DONE,			// TK_RBRACE
	OP_DONE,			// TK_LBRACKET
	OP_DONE,			// TK_RBRACKET
	OP_DONE,			// TK_NUMBERSIGN
	OP_EQ_F,			// TK_EQ
	OP_NE_F,			// TK_NE
	OP_LT,				// TK_LT
	OP_GT,				// TK_GT
	OP_LE,				// TK_LE
	OP_GE,				// TK_GE
	OP_STORE_F,			// TK_ASSIGN
	OP_AND,				// TK_AND
	OP_BITAND,			// TK_BITAND
	OP_OR,				// TK_OR
	OP_BITOR,			// TK_BITOR
	OP_DONE,			// TK_NOT
	OP_LOAD_F,			// TK_PERIOD
	OP_DONE,			// TK_ELLIPSIS
	OP_SUB_F,			// TK_MINUS
	OP_DONE,			// TK_INC
	OP_DONE,			// TK_DEC
	OP_MULSTORE_F,		// TK_MULASSIGN
	OP_DIVSTORE_F,		// TK_DIVASSIGN
	OP_ADDSTORE_F,		// TK_ADDASSIGN
	OP_SUBSTORE_F,		// TK_SUBASSIGN
	OP_DONE,			// TK_COLON
	OP_DONE,			// TK_RANGE
	OP_BITSET,			// TK_BITSET
	OP_BITCLR			// TK_BITCLR
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// EX_Init
//
//==========================================================================

void EX_Init(void)
{
	ex_FunctionCallCount = 0;
}

//==========================================================================
//
// EX_Expression
//
//==========================================================================

def_t *EX_Expression(int priority)
{
	opcode_t *op;
	opcode_t *first;
	def_t *e;
	def_t *e2;
	etype_t type_a;
	etype_t type_b;
	etype_t type_c;
	int tag;
	int opIndex;

	if(priority == 0)
	{
		return Term();
	}

	e = EX_Expression(priority-1);

	while(1)
	{
		if(priority == 1 && TK_CHECK(TK_LPAREN))
		{
			return ParseFunctionCall(e);
		}

		// [2026-11-14] Komi - -fte: "x++" and "x--"
		if(priority == 1 && hcc_Fte && (TK_TEST(TK_INC) || TK_TEST(TK_DEC)))
		{
			e = Increment(e, pr_tokenclass, true);
			LX_Fetch();
			continue;
		}

		opIndex = TokenToOp[pr_tokenclass];
		if(opIndex == OP_DONE)
		{
			return e;
		}
		for(op = &pr_opcodes[opIndex]; op->name; op++)
		{
			if(op->priority != priority)
			{
				return e;
			}
			if(!LX_CheckFetch(op->name))
			{
				return e;
			}
			if(op->right_associative)
			{
				if((unsigned)
					(statements[numstatements-1].op-OP_LOAD_F) < 6)
				{
					// The preceding statement was an indirect.  Change it to
					// an address of.
					statements[numstatements-1].op = OP_ADDRESS;
					def_pointer.type->aux_type = e->type;
					e->type = def_pointer.type;
				}
/*				else if((unsigned)
					(statements[numstatements-1].op-OP_FETCH_GBL_F) < 5)
				{
					// The preceding statement was an array lookup.  Assignment
					// is currently not allowed to arrays.
					MS_ParseError("assignment not allowed to arrays");
				}
*/
				e2 = EX_Expression(priority);
			}
			else
			{
				e2 = EX_Expression(priority-1);
			}

			// Set types a, b, and c
			type_a = e->type->type;
			type_b = e2->type->type;
			if(op->name[0] == '.')
			{ // Field access gets type from field
				if(e2->type->aux_type)
				{
					type_c = e2->type->aux_type->type;
				}
				else
				{ // Not a field
					type_c = -1;
				}
			}
			else
			{
				type_c = ev_void;
			}

			// Find the opcode that matches the types
			tag = op->tag;
			first = op;
retry:
			while(type_a != op->type_a->type->type
				|| type_b != op->type_b->type->type
				|| (type_c != ev_void && type_c != op->type_c->type->type))
			{
				op++;
				if(tag != op->tag)
				{
					op--;
					break;
				}
			}
			if(type_a != op->type_a->type->type
				|| type_b != op->type_b->type->type
				|| (type_c != ev_void && type_c != op->type_c->type->type)
				|| (type_a == ev_pointer && type_b != e->type->aux_type->type
					&& !(hcc_Fte && op == &pr_opcodes[OP_MULSTOREP_V])))
			{
				// [2026-11-14] Komi - -fte: retry with FTEQCC's conversion
				if(hcc_Fte && type_c == ev_void
					&& (first = FteCoerce(first, &e, &e2)) != NULL)
				{
					op = first;
					tag = op->tag;
					type_a = e->type->type;
					type_b = e2->type->type;
					goto retry;
				}
				MS_ParseError("type mismatch for %s", op->name);
			}

			// Emit the statement
			if(op->right_associative)
			{
				e = CO_GenCode(op, e2, e);
			}
			else
			{
				e = CO_GenCode(op, e, e2);
			}

			if(type_c != ev_void)
			{ // Field access gets type from field
				e->type = e2->type->aux_type;
			}

			break;
		}
		if(!op->name)
		{ // Next token isn't at this priority level
			break;
		}
	}

	return e;
}

//==========================================================================
//
// Term
//
//==========================================================================

static def_t *Term(void)
{
	def_t *d, *e, *e2;
	etype_t t;
	int tc;
	char *name;

	if(TK_CHECK(TK_NOT))
	{
		return EX_Not(EX_Expression(NOT_PRIORITY));
	}

	if(hcc_Fte)
	{ // [2026-11-14] Komi - -fte: FTEQCC's unary operators and vector constructor
		if(TK_CHECK(TK_MINUS))
		{
			e = EX_Expression(1);
			t = e->type->type;
			if(t == ev_float)
				return CO_GenCode(&pr_opcodes[OP_SUB_F],
					MakeImmediate(&type_float, 0, 0, 0), e);
			if(t == ev_vector)
				return CO_GenCode(&pr_opcodes[OP_SUB_V],
					MakeImmediate(&type_vector, 0, 0, 0), e);
			MS_ParseError("type mismatch for -");
		}
		if(TK_CHECK(TK_PLUS))
		{
			return EX_Expression(1);
		}
		if(TK_TEST(TK_INC) || TK_TEST(TK_DEC))
		{
			tc = pr_tokenclass;
			LX_Fetch();
			return Increment(EX_Expression(1), tc, false);
		}
		if(TK_CHECK(TK_LBRACKET))
		{
			return VectorConstructor();
		}
	}

	if(TK_CHECK(TK_LPAREN))
	{
		e = EX_Expression(TOP_PRIORITY);
		// [2026-11-14] Komi - -fte: "(a, b)" evaluates both and gives b, as in FTEQCC
		while(hcc_Fte && TK_CHECK(TK_COMMA))
		{
			e = EX_Expression(TOP_PRIORITY);
		}
		LX_Require(")");
		return e;
	}

	if(pr_token_type == tt_immediate)
	{
		d = CO_ParseImmediate();
		LX_Fetch();
		return d;
	}

	name = PR_ParseName();
	if((d = ParseIntrinsicFunc(name)) != NULL)
	{ // Found and parsed an intrinsic function
		d->referenceCount++;
		if(d->parentVector != NULL)
		{
			d->parentVector->referenceCount++;
		}
		return d;
	}

	d = PR_GetDef(NULL, name, pr_scope, false);
	if(!d)
	{
		MS_ParseError("unknown value \"%s\"", name);
	}

	d->referenceCount++;
	if(d->parentVector != NULL)
	{
		d->parentVector->referenceCount++;
	}

	if(TK_CHECK(TK_LBRACKET))
	{
		e = EX_Expression(TOP_PRIORITY);
		LX_Require("]");

		if(TK_TEST(TK_ASSIGN))
		{
			LX_Fetch();
			e2 = EX_Expression(TOP_PRIORITY);
			//MS_ParseError("assignment is not allowed to arrays");


			if(d->type->type != e2->type->type)
			{
				MS_ParseError("type mismatch for =");
			}
			return NULL;
		}

		switch(d->type->type)
		{
		case ev_float:
			e2 = CO_GenCode(&pr_opcodes[OP_FETCH_GBL_F], d, e);
			break;
		case ev_vector:
			e2 = CO_GenCode(&pr_opcodes[OP_FETCH_GBL_V], d, e);
			break;
		case ev_string:
			e2 = CO_GenCode(&pr_opcodes[OP_FETCH_GBL_S], d, e);
			break;
		case ev_entity:
			e2 = CO_GenCode(&pr_opcodes[OP_FETCH_GBL_E], d, e);
			break;
		case ev_function:
			e2 = CO_GenCode(&pr_opcodes[OP_FETCH_GBL_FNC], d, e);
			break;
		default:
			e2 = NULL; // Shut up compiler warning
			MS_ParseError("type mismatch for []");
			break;
		}
		return e2;
	}
	return d;
}

//==========================================================================
//
// EX_Not
//
// !e, with the NOT opcode of e's type.
//
//==========================================================================

def_t *EX_Not(def_t *e)
{
	etype_t t;
	def_t *e2;

	t = e->type->type;
	if(t == ev_float)
		e2 = CO_GenCode(&pr_opcodes[OP_NOT_F], e, 0);
	else if(t == ev_string)
		e2 = CO_GenCode(&pr_opcodes[OP_NOT_S], e, 0);
	else if(t == ev_entity)
		e2 = CO_GenCode(&pr_opcodes[OP_NOT_ENT], e, 0);
	else if(t == ev_vector)
		e2 = CO_GenCode(&pr_opcodes[OP_NOT_V], e, 0);
	else if(t == ev_function)
		e2 = CO_GenCode(&pr_opcodes[OP_NOT_FNC], e, 0);
	else
	{
		e2 = NULL; // Shut up compiler warning
		MS_ParseError("type mismatch for !");
	}
	return e2;
}

//==========================================================================
//
// ParseFunctionCall
//
//==========================================================================

static def_t *ParseFunctionCall(def_t *func)
{
	def_t *e;
	def_t *args[2];
	def_t *late[8];
	opcode_t *stores[8];
	int argCount;
	int i;
	qboolean extra;
	type_t *t;

	t = func->type;
	if(t->type != ev_function)
	{
		MS_ParseError("not a function");
	}

	func->referenceCount++;
	if(func->parentVector != NULL)
	{
		func->parentVector->referenceCount++;
	}

	argCount = 0;
	args[0] = NULL;
	args[1] = NULL;
	if(!TK_CHECK(TK_RPAREN))
	{
		do
		{
			if(argCount == 8)
			{
				MS_ParseError("more than eight parameters");
			}
			if(t->num_parms != -1 && argCount >= t->num_parms)
			{
				// [2026-11-14] Komi - -fte: a builtin takes extra arguments, as in FTEQCC
				if(!hcc_Fte || !func->initialized
					|| functions[G_FUNCTION(func->ofs)].first_statement >= 0)
				{
					MS_ParseError("too many parameters");
				}
				if(argCount == t->num_parms)
				{
					MS_ParseWarning("%s() is declared with %d parameters",
						func->name, t->num_parms);
				}
			}
			e = EX_Expression(TOP_PRIORITY);

			if(argCount == 0 && func->name)
			{ // Check for sound / model / file caching
				if(!strncmp(func->name, "precache_sound", 14))
				{
					PrecacheSound(e, func->name[14]);
				}
				else if(!strncmp(func->name, "precache_model", 14))
				{
					PrecacheModel(e, func->name[14]);
				}
				else if(!strncmp(func->name, "precache_file", 13))
				{
					PrecacheFile(e, func->name[13]);
				}
			}

			extra = t->num_parms != -1 && argCount >= t->num_parms;
			if(t->num_parms != -1 && !extra
				&& (e->type != t->parm_types[argCount]))
			{
				MS_ParseError("type mismatch on parm %i", argCount);
			}

			def_parms[argCount].type = extra ? e->type : t->parm_types[argCount];
			stores[argCount] = &pr_opcodes[extra
				|| t->parm_types[argCount] == NULL // Variable args
				|| t->parm_types[argCount]->type == ev_vector
				? OP_STORE_V : OP_STORE_F];
			if(argCount < 2)
			{
				args[argCount] = e;
			}
			else if(hcc_Fte)
			{ // [2026-11-14] Komi - -fte: stored after the last argument, as FTEQCC does, so a call in a later argument can't overwrite it
				late[argCount] = e;
			}
			else
			{
				CO_GenCode(stores[argCount], e, &def_parms[argCount]);
			}
			argCount++;
		} while(TK_CHECK(TK_COMMA));
		LX_Require(")");
		for(i = 2; hcc_Fte && i < argCount; i++)
		{
			CO_GenCode(stores[i], late[i], &def_parms[i]);
		}
	}

	if(t->num_parms != -1 && argCount < t->num_parms)
	{
		// [2026-11-14] Komi - -fte: the missing parameters are left undefined, as in FTEQCC
		if(!hcc_Fte)
		{
			MS_ParseError("too few parameters");
		}
		MS_ParseWarning("too few parameters to %s(): the rest are undefined",
			func->name);
	}

	CO_KeepReturns(func, args[0], args[1]);
	CO_GenCodeDirect(&pr_opcodes[OP_CALL0+argCount], func, args[0],
		args[1]);

	ex_FunctionCallCount++;

	return CO_NewReturn(t->aux_type, func->name);
}

//==========================================================================
//
// PrecacheSound
//
//==========================================================================

static void PrecacheSound(def_t *e, int ch)
{
	int i;
	char *n;

	if(!e->ofs || !e->initialized)
	{
		return;
	}
	n = G_STRING(e->ofs);
	for(i = 0; i < numsounds; i++)
	{
		if(!strcmp(n, precache_sounds[i]))
		{
			return;
		}
	}
	if(numsounds == MAX_SOUNDS)
	{
		MS_Error("PrecacheSound: numsounds == MAX_SOUNDS");
	}
	strcpy(precache_sounds[i], n);
	if(ch >= '1'  && ch <= '9')
	{
		precache_sounds_block[i] = ch - '0';
	}
	else
	{
		precache_sounds_block[i] = 1;
	}
	numsounds++;
}

//==========================================================================
//
// PrecacheModel
//
//==========================================================================

static void PrecacheModel(def_t *e, int ch)
{
	int i;
	char *n;

	if(!e->ofs || !e->initialized)
	{
		return;
	}
	n = G_STRING(e->ofs);
	for(i = 0; i < nummodels; i++)
	{
		if(!strcmp(n, precache_models[i]))
		{
			return;
		}
	}
	if(nummodels == MAX_MODELS)
	{
		MS_Error("PrecacheModels: nummodels == MAX_MODELS");
	}
	strcpy(precache_models[i], n);
	if(ch >= '1'  && ch <= '9')
	{
		precache_models_block[i] = ch - '0';
	}
	else
	{
		precache_models_block[i] = 1;
	}
	nummodels++;
}

//==========================================================================
//
// PrecacheFile
//
//==========================================================================
static void PrecacheFileName(char *n, int ch)
{
	int i;
	for(i = 0; i < numfiles; i++)
	{
		if(!strcmp(n, precache_files[i]))
		{
			return;
		}
	}
	if(numfiles == MAX_FILES)
	{
		MS_Error("PrecacheFile: numfiles == MAX_FILES");
	}
	strcpy(precache_files[i], n);
	if(ch >= '1'  && ch <= '9')
	{
		precache_files_block[i] = ch - '0';
	}
	else
	{
		precache_files_block[i] = 1;
	}
	numfiles++;
}

static void PrecacheFile(def_t *e, int ch)
{
	int i;
	char *n;

	if(!e->ofs || !e->initialized)
	{
		return;
	}
	n = G_STRING(e->ofs);
	for(i = 0; i < numfiles; i++)
	{
		if(!strcmp(n, precache_files[i]))
		{
			return;
		}
	}
	if(numfiles == MAX_FILES)
	{
		MS_Error("PrecacheFile: numfiles == MAX_FILES");
	}
	strcpy(precache_files[i], n);
	if(ch >= '1'  && ch <= '9')
	{
		precache_files_block[i] = ch - '0';
	}
	else
	{
		precache_files_block[i] = 1;
	}
	numfiles++;
}

//==========================================================================
//
// ParseIntrinsicFunc
//
//==========================================================================

static def_t *ParseIntrinsicFunc(char *name)
{
	def_t *expr1, *expr2;

	if(strcmp(name, "random") == 0)
	{
		LX_Require("(");
		if(TK_CHECK(TK_RPAREN))
		{
			CO_KeepReturns(NULL, NULL, NULL);
			CO_GenCode(&pr_opcodes[OP_RAND0], NULL, NULL);
		}
		else
		{
			expr1 = EX_Expression(TOP_PRIORITY);
			if(expr1->type->type != ev_float)
			{
				MS_ParseError("'random' : incompatible "
					"parameter type");
			}
			if(TK_CHECK(TK_COMMA))
			{
				expr2 = EX_Expression(TOP_PRIORITY);
				if(expr2->type->type != ev_float)
				{
					MS_ParseError("'random' : incompatible "
						"parameter type");
				}
				LX_Require(")");
				CO_KeepReturns(expr1, expr2, NULL);
				CO_GenCode(&pr_opcodes[OP_RAND2], expr1, expr2);
			}
			else
			{
				LX_Require(")");
				CO_KeepReturns(expr1, NULL, NULL);
				CO_GenCode(&pr_opcodes[OP_RAND1], expr1, NULL);
			}
		}
		return CO_NewReturn(&type_float, "random");
	}

	if(strcmp(name, "randomv") == 0)
	{
		LX_Require("(");
		if(TK_CHECK(TK_RPAREN))
		{
			CO_KeepReturns(NULL, NULL, NULL);
			CO_GenCode(&pr_opcodes[OP_RANDV0], NULL, NULL);
		}
		else
		{
			expr1 = EX_Expression(TOP_PRIORITY);
			if(expr1->type->type != ev_vector)
			{
				MS_ParseError("'randomv' : incompatible "
					"parameter type");
			}
			if(TK_CHECK(TK_COMMA))
			{
				expr2 = EX_Expression(TOP_PRIORITY);
				if(expr2->type->type != ev_vector)
				{
					MS_ParseError("'randomv' : incompatible "
						"parameter type");
				}
				LX_Require(")");
				CO_KeepReturns(expr1, expr2, NULL);
				CO_GenCode(&pr_opcodes[OP_RANDV2], expr1, expr2);
			}
			else
			{
				LX_Require(")");
				CO_KeepReturns(expr1, NULL, NULL);
				CO_GenCode(&pr_opcodes[OP_RANDV1], expr1, NULL);
			}
		}
		return CO_NewReturn(&type_vector, "randomv");
	}

	if(!strncmp(name, "precache_file", 13))	//i put this here to keep if from going in progs.dat
	{
		def_ret.type = &type_void;
		LX_Require("(");
		if(TK_CHECK(TK_RPAREN))
		{
			return &def_ret;	//it's empty for some reason.
		}
		//i should be getting an immediate string here...
		if(pr_token_type != tt_immediate)	//oops
			return (NULL);
/*			expr1  = CO_ParseImmediate();
		if(expr1->type->type != ev_string)
		{
			MS_ParseError("'precache_file' : parm not a string");
		}
*/
		if(pr_immediate_type != &type_string)
		{
			MS_ParseError("'precache_file' : parm not a string");
		}
		PrecacheFileName(pr_immediate_string, name[13]);
		LX_Fetch();				
		LX_Require(")");
		
		return &def_ret;
	}
	return NULL;
}

//==========================================================================
//
// FTEQCC compatibility (-fte)
//
// [2026-11-14] Komi - Helpers for the code FTEQCC accepts and Raven's
// HCC doesn't: conversions between operand types, increments and the vector
// constructor. Each produces what FTEQCC produces for the same line.
//
//==========================================================================

// An immediate made while the lexer may hold another one.
static def_t *MakeImmediate(type_t *type, float x, float y, float z)
{
	token_type_t tokenType;
	type_t *immediateType;
	eval_t immediate;
	def_t *d;

	tokenType = pr_token_type;
	immediateType = pr_immediate_type;
	immediate = pr_immediate;
	pr_token_type = tt_immediate;
	pr_immediate_type = type;
	memset(&pr_immediate, 0, sizeof(pr_immediate));
	pr_immediate.vector[0] = x;
	pr_immediate.vector[1] = y;
	pr_immediate.vector[2] = z;
	d = CO_ParseImmediate();
	pr_token_type = tokenType;
	pr_immediate_type = immediateType;
	pr_immediate = immediate;
	return d;
}

// A value for && and ||. FTEQCC uses a string, entity or function as it is,
// read as a float; a vector becomes 1 or 0.
static def_t *Truth(def_t *e)
{
	int op;
	def_t *d;

	if(e->type->type != ev_float && e->type->type != ev_vector
		&& e->retStatement == 0)
	{ // the same register, typed as a float
		d = malloc(sizeof(def_t));
		*d = *e;
		d->type = &type_float;
		return d;
	}
	switch(e->type->type)
	{
	case ev_float:
		return e;
	case ev_vector:
		op = OP_NOT_V;
		break;
	case ev_string:
		op = OP_NOT_S;
		break;
	case ev_entity:
		op = OP_NOT_ENT;
		break;
	case ev_function:
		op = OP_NOT_FNC;
		break;
	default:
		MS_ParseError("type mismatch for a condition");
		return e;
	}
	e = CO_GenCode(&pr_opcodes[op], e, 0);
	return CO_GenCode(&pr_opcodes[OP_NOT_F], e, 0);
}

// 1/e, folded when e is a constant.
static def_t *Reciprocal(def_t *e)
{
	if(e->initialized && e->type->type == ev_float && G_FLOAT(e->ofs) != 0)
	{
		return MakeImmediate(&type_float, 1/G_FLOAT(e->ofs), 0, 0);
	}
	return CO_GenCode(&pr_opcodes[OP_DIV_F],
		MakeImmediate(&type_float, 1, 0, 0), e);
}

// Returns the opcode family to match again after converting an operand, or
// NULL when FTEQCC has no conversion either. first is the operator's first
// opcode, e the left operand (the target of an assignment).
static opcode_t *FteCoerce(opcode_t *first, def_t **e, def_t **e2)
{
	etype_t a, b;

	a = (*e)->type->type;
	if(a == ev_pointer)
	{
		a = (*e)->type->aux_type->type;
	}
	b = (*e2)->type->type;
	if(first == &pr_opcodes[OP_AND] || first == &pr_opcodes[OP_OR])
	{
		if(a == ev_float && b == ev_float)
		{
			return NULL;
		}
		*e = Truth(*e);
		*e2 = Truth(*e2);
		return first;
	}
	if(a != ev_vector || b != ev_float)
	{
		return NULL;
	}
	if(first == &pr_opcodes[OP_DIV_F])
	{ // v / f is v * (1/f)
		*e2 = Reciprocal(*e2);
		return &pr_opcodes[OP_MUL_F];
	}
	if(first == &pr_opcodes[OP_MULSTORE_F])
	{ // ent.v *= f
		return &pr_opcodes[(*e)->type->type == ev_pointer
			? OP_MULSTOREP_V : OP_MULSTORE_V];
	}
	if(first == &pr_opcodes[OP_DIVSTORE_F])
	{ // v /= f is v *= 1/f
		*e2 = Reciprocal(*e2);
		return &pr_opcodes[(*e)->type->type == ev_pointer
			? OP_MULSTOREP_V : OP_MULSTORE_V];
	}
	if(first == &pr_opcodes[OP_STORE_F])
	{ // v = f sets all three components to f
		if((*e2)->initialized)
		{
			*e2 = MakeImmediate(&type_vector, G_FLOAT((*e2)->ofs),
				G_FLOAT((*e2)->ofs), G_FLOAT((*e2)->ofs));
		}
		else
		{
			*e2 = CO_GenCode(&pr_opcodes[OP_MUL_FV], *e2,
				MakeImmediate(&type_vector, 1, 1, 1));
		}
		return first;
	}
	return NULL;
}

// A field just read with LOAD becomes its address, for a store into it.
static def_t *AddressOf(def_t *e)
{
	dstatement_t *s;

	s = &statements[numstatements-1];
	if((unsigned)(s->op-OP_LOAD_F) < 6 && s->c == e->ofs)
	{
		s->op = OP_ADDRESS;
		def_pointer.type->aux_type = e->type;
		e->type = def_pointer.type;
	}
	return e;
}

// ++x and --x give the new value; x++ and x-- (postfix) the old one.
static def_t *Increment(def_t *e, int tokenclass, qboolean postfix)
{
	def_t *one;
	def_t *r;
	qboolean pointer;
	qboolean add;

	if(e == NULL || e->type->type != ev_float)
	{
		MS_ParseError("++ and -- need a float");
	}
	add = tokenclass == TK_INC;
	e = AddressOf(e);
	pointer = e->type->type == ev_pointer;
	one = MakeImmediate(&type_float, 1, 0, 0);
	if(pointer)
	{
		r = CO_GenCode(&pr_opcodes[add ? OP_ADDSTOREP_F : OP_SUBSTOREP_F],
			one, e);
	}
	else
	{
		r = CO_GenCode(&pr_opcodes[add ? OP_ADDSTORE_F : OP_SUBSTORE_F],
			one, e);
	}
	if(postfix)
	{
		r = CO_GenCode(&pr_opcodes[add ? OP_SUB_F : OP_ADD_F], r, one);
	}
	return r;
}

// [x, y, z] with any float expressions; constants make an immediate.
static def_t *VectorConstructor(void)
{
	static int count;
	def_t *c[3];
	def_t *v;
	def_t *component;
	char name[64];
	int i;

	for(i = 0; i < 3; i++)
	{
		if(i > 0)
		{
			LX_Require(",");
		}
		c[i] = EX_Expression(TOP_PRIORITY);
		if(c[i]->type->type != ev_float)
		{
			MS_ParseError("type mismatch in [x, y, z]");
		}
	}
	LX_Require("]");
	if(c[0]->initialized && c[1]->initialized && c[2]->initialized)
	{
		return MakeImmediate(&type_vector, G_FLOAT(c[0]->ofs),
			G_FLOAT(c[1]->ofs), G_FLOAT(c[2]->ofs));
	}
	if(pr_scope == NULL)
	{
		MS_ParseError("[x, y, z] with variables outside a function");
	}
	// A hidden local holds the vector
	sprintf(name, "vec%d$", ++count);
	v = PR_GetDef(&type_vector, name, pr_scope, true);
	v->referenceCount++;
	for(i = 0; i < 3; i++)
	{
		sprintf(name, "vec%d$_%c", count, "xyz"[i]);
		component = PR_GetDef(&type_float, name, pr_scope, false);
		CO_GenCode(&pr_opcodes[OP_STORE_F], c[i], component);
	}
	return v;
}
