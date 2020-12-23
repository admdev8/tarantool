#include "diag.h"
#include "execute.h"
#include "lua/utils.h"
#include "sqlInt.h"
#include "../sql/vdbe.h"	// FIXME
#include "../sql/vdbeInt.h"	// FIXME
#include "../execute.h"		// FIXME
#include "../schema.h"		// FIXME
#include "../session.h"		// FIXME
#include "../box.h"		// FIXME
#include <small/ibuf.h>
#include <msgpuck/msgpuck.h>

#include "sqlparser.h"

#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern uint32_t CTID_STRUCT_SQL_PARSED_AST;

struct OutputWalker {
	struct Walker base;
	size_t accum;
	struct ibuf *ibuf;
};

#define INC_SZ(w, e, f) \
	w->accum += sizeof(e->f)

#define OUT_V(ibuf, p, f, type) \
	data = ibuf_alloc(ibuf, mp_sizeof_##type(p->f)); \
	data = mp_encode_str(data, #f, strlen(#f)); \
	data = mp_encode_##type(data, p->f);

int outputExprStep(struct Walker * base, struct Expr *expr) {
	struct OutputWalker *walker = (struct OutputWalker*)base;
	struct ibuf * ibuf = walker->ibuf;
	char * data = ibuf->wpos;

	data = mp_encode_map(data, 9 + (SQL_MAX_EXPR_DEPTH > 0));

	OUT_V(ibuf, expr, op, uint);
	OUT_V(ibuf, expr, type, uint);
	OUT_V(ibuf, expr, flags, uint);
	OUT_V(ibuf, expr, u.iValue, int); // FIXME
#if SQL_MAX_EXPR_DEPTH > 0
	OUT_V(ibuf, expr, nHeight, int);
#endif
	OUT_V(ibuf, expr, iTable, int);
	OUT_V(ibuf, expr, iColumn, int);

	OUT_V(ibuf, expr, iAgg, int);
	OUT_V(ibuf, expr, iRightJoinTable, int);
	OUT_V(ibuf, expr, op2, uint);
	// OUT_V(ibuf, expr, pAggInfo, int);

}
int outputSelectStep(Walker * base, Select * select) {
	struct OutputWalker *walker = (struct OutputWalker*)base;

}

int
lbox_sqlparser_serialize(struct lua_State *L)
{
	int top = lua_gettop(L);
	assert(top == 1);
	(void)top;

	struct sql_parsed_ast *ast = luaT_check_sql_parsed_ast(L, 1);

	if (!AST_VALID(ast)) {
		assert(ast->ast_type == AST_TYPE_SELECT);

		struct ibuf ibuf;
		ibuf_create(&ibuf, &cord()->slabc, 1024); // FIXME - precise estimate

		struct Parse parser;
		sql_parser_create(&parser, parser.db, default_flags);

		struct Select *p = ast->select;
		struct OutputWalker wlkr = {
			.base = {
				.xExprCallback = outputExprStep,
				.xSelectCallback = outputSelectStep,
				.pParse = &parser,
				.u = { .pNC = NULL },
			},
			.accum = 0,
			.ibuf = &ibuf,
		};
		struct region *region = &fiber()->gc;

		sqlWalkSelect(&wlkr, p);

		lua_pushnumber(L, 1);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int
lbox_sqlparser_deserialize(struct lua_State *L)
{
	lua_pushliteral(L, "sqlparser.deserialize");
	return 1;
}
