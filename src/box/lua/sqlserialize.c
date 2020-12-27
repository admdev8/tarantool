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

#include <assert.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern uint32_t CTID_STRUCT_SQL_PARSED_AST;

struct OutputWalker {
	struct Walker base;
	size_t accum;
	struct ibuf *ibuf;
	//const char *title;
};

static int
sql_walk_select(struct Walker *, struct Select *, const char *);
static int
sql_walk_expr_list(struct Walker * base, struct ExprList * p, const char *title);
static int
sql_walk_select_expr(Walker * walker, Select * p, bool dryrun, const char *title);
static int
sql_walk_select_from(Walker * walker, Select * p, bool dryrun, const char *title);

// a set of msgpack helpers to serialize data to ibuf

// we have to be careful enough to manually select _int or _uint
// variants
#define mp_sizeof_Xint(v) \
	(v < 0 ? mp_sizeof_int(v) : mp_sizeof_uint(v))
#define mp_encode_Xint(data, v) \
	(v < 0 ? mp_encode_int(data, v) : mp_encode_uint(data, v))

// output string literal
#define OUT_S(ibuf, s) \
	data = ibuf_alloc(ibuf, mp_sizeof_str(strlen(s))); \
	assert(data != NULL); \
	data = mp_encode_str(data, s, strlen(s)); \

// output field name and their value 
#define OUT_V(ibuf, p, f, type) \
	OUT_S(ibuf, #f) \
	data = ibuf_alloc(ibuf, mp_sizeof_##type(p->f)); \
	assert(data != NULL); \
	data = mp_encode_##type(data, p->f);

// output field name and it's string value
#define OUT_VS(ibuf, p, f) \
	OUT_S(ibuf, #f) \
	data = ibuf_alloc(ibuf, mp_sizeof_str(strlen(p->f))); \
	assert(data != NULL); \
	data = mp_encode_str(data, p->f, strlen(p->f));

// outpu title of tuple, expecting map to follow
#define OUT_TUPLE_TITLE(ibuf, title) \
	data = ibuf_alloc(ibuf, mp_sizeof_map(1)); \
	assert(data != NULL); \
	data = mp_encode_map(data, 1); \
	assert(data != NULL); \
	OUT_S(ibuf, title);
	/* then 1 map expected */

// output array of n elements
#define OUT_ARRAY_N(ibuf, n) \
	data = ibuf_alloc(ibuf, mp_sizeof_array(n)); \
	assert(data != NULL); \
	data = mp_encode_array(data, n);

// output map of n keys/values
#define OUT_MAP_N(ibuf, n) \
	data = ibuf_alloc(ibuf, mp_sizeof_map(n)); \
	assert(data != NULL); \
	data = mp_encode_map(data, n);


/*
 * Walk an expression tree.  Invoke the callback once for each node
 * of the expression, while descending.  (In other words, the callback
 * is invoked before visiting children.)
 *
 * The return value from the callback should be one of the WRC_*
 * constants to specify how to proceed with the walk.
 *
 *    WRC_Continue      Continue descending down the tree.
 *
 *    WRC_Prune         Do not descend into child nodes.  But allow
 *                      the walk to continue with sibling nodes.
 *
 *    WRC_Abort         Do no more callbacks.  Unwind the stack and
 *                      return the top-level walk call.
 *
 * The return value from this routine is WRC_Abort to abandon the tree walk
 * and WRC_Continue to continue.
 */

static int
sql_walk_expr(struct Walker * base, struct Expr * expr, const char *title)
{
	if (expr == NULL)
		return WRC_Continue;

	struct OutputWalker * walker = (struct OutputWalker*)base;
	struct ibuf * ibuf = walker->ibuf;
	char * data = ibuf->wpos;
	OUT_TUPLE_TITLE(walker->ibuf, title);

	// first we need to estimate number of elements in map
	size_t extra = (SQL_MAX_EXPR_DEPTH > 0);
	extra += (expr->pLeft != NULL) + (expr->pRight != NULL) +
		(!!ExprHasProperty(expr, EP_xIsSelect) || expr->x.pList != NULL);

	data = mp_encode_map(data, 9 + 
			     !ExprHasProperty(expr, (EP_TokenOnly | EP_Leaf)) *
			     extra);

	OUT_V(ibuf, expr, op, uint);
	OUT_V(ibuf, expr, type, uint);
	OUT_V(ibuf, expr, flags, uint);
	OUT_V(ibuf, expr, u.iValue, Xint); // FIXME
#if SQL_MAX_EXPR_DEPTH > 0
	OUT_V(ibuf, expr, nHeight, Xint);
#endif
	OUT_V(ibuf, expr, iTable, Xint);
	OUT_V(ibuf, expr, iColumn, Xint);

	OUT_V(ibuf, expr, iAgg, Xint);
	OUT_V(ibuf, expr, iRightJoinTable, Xint);
	OUT_V(ibuf, expr, op2, uint);

	if (ExprHasProperty(expr, (EP_TokenOnly | EP_Leaf)))
		return WRC_Abort;

	if (expr->pLeft && sql_walk_expr(base, expr->pLeft, "left"))
		return WRC_Abort;
	if (expr->pRight && sql_walk_expr(base, expr->pRight, "right"))
		return WRC_Abort;

	if (ExprHasProperty(expr, EP_xIsSelect)) {
		if (sql_walk_select(base, expr->x.pSelect, "subselect"))
			return WRC_Abort;
	} else if (expr->x.pList) {
		if (sql_walk_expr_list(base, expr->x.pList, "inexpr"))
			return WRC_Abort;
	}

	return WRC_Continue;
}

/*
 * Call sql_walk_expr() for every expression in list p or until
 * an abort request is seen.
 */
static int
sql_walk_expr_list(struct Walker * base, struct ExprList * p, const char *title)
{
	if (p == NULL)
		return WRC_Continue;

	struct OutputWalker * walker = (struct OutputWalker*)base;
	struct ibuf * ibuf = walker->ibuf;
	char * data = ibuf->wpos;
	OUT_TUPLE_TITLE(walker->ibuf, title);
	struct ExprList_item *pItem;
	int i;
	int n_elems = p->nExpr;
	OUT_ARRAY_N(ibuf, n_elems);
	for (i = p->nExpr, pItem = p->a; i > 0; i--, pItem++) {
		if (sql_walk_expr(base, pItem->pExpr, "FCK"))
			return WRC_Abort;
	}
	assert(n_elems == (p->nExpr - i));
	return WRC_Continue;
}

/*
 * Walk all expressions associated with SELECT statement p.  Do
 * not invoke the SELECT callback on p, but do (of course) invoke
 * any expr callbacks and SELECT callbacks that come from subqueries.
 * Return WRC_Abort or WRC_Continue.
 */
static int
sql_walk_select_expr(Walker * walker, Select * p, bool dryrun,
		    const char *title)
{
	(void)title;
	int rc = 0;
	if (dryrun != 0) {
		rc += (p->pEList != NULL) + (p->pWhere != NULL) +
			(p->pGroupBy != NULL) + (p->pHaving != NULL) +
			(p->pOrderBy != NULL) + (p->pLimit != NULL) +
			(p->pOffset != NULL);
		return rc;
	}
	if (sql_walk_expr_list(walker, p->pEList, "results"))
		return WRC_Abort;
	if (sql_walk_expr(walker, p->pWhere, "where"))
		return WRC_Abort;
	if (sql_walk_expr_list(walker, p->pGroupBy, "groupby"))
		return WRC_Abort;
	if (sql_walk_expr(walker, p->pHaving, "having"))
		return WRC_Abort;
	if (sql_walk_expr_list(walker, p->pOrderBy, "orderby"))
		return WRC_Abort;
	if (sql_walk_expr(walker, p->pLimit, "limit"))
		return WRC_Abort;
	if (sql_walk_expr(walker, p->pOffset, "offset"))
		return WRC_Abort;
	return WRC_Continue;
}

/*
 * Walk the parse trees associated with all subqueries in the
 * FROM clause of SELECT statement p.  Do not invoke the select
 * callback on p, but do invoke it on each FROM clause subquery
 * and on any subqueries further down in the tree.  Return
 * WRC_Abort or WRC_Continue;
 */
static int
sql_walk_select_from(Walker * base, Select * p, bool dryrun, const char *title)
{
	SrcList *pSrc = p->pSrc;
	if (dryrun != 0)
		return (pSrc != NULL);
	if (pSrc == NULL)
		return WRC_Continue;

	struct OutputWalker * walker = (struct OutputWalker*)base;
	struct ibuf * ibuf = walker->ibuf;
	char * data = ibuf->wpos;
	OUT_TUPLE_TITLE(walker->ibuf, title);
	int n_elems = pSrc->nSrc;
	OUT_ARRAY_N(ibuf, n_elems);
	int i;
	struct SrcList_item *pItem;

	for (i = pSrc->nSrc, pItem = pSrc->a; i > 0; i--, pItem++) {
		size_t items = (pItem->pSelect != NULL) +
				(pItem->fg.isTabFunc &&
				pItem->u1.pFuncArg != NULL);
		
		OUT_MAP_N(ibuf, items);
		if (sql_walk_select(base, pItem->pSelect, "select"))
			return WRC_Abort;

		if (pItem->fg.isTabFunc &&
		    sql_walk_expr_list(base, pItem->u1.pFuncArg, "list"))
			return WRC_Abort;
	}
	assert(n_elems == (pSrc->nSrc - i));
	return WRC_Continue;
}

/*
 * Call sql_walk_expr() for every expression in Select statement p.
 * Invoke sqlWalkSelect() for subqueries in the FROM clause and
 * on the compound select chain, p->pPrior.
 *
 * If it is not NULL, the xSelectCallback() callback is invoked before
 * the walk of the expressions and FROM clause. The xSelectCallback2()
 * method, if it is not NULL, is invoked following the walk of the
 * expressions and FROM clause.
 *
 * Return WRC_Continue under normal conditions.  Return WRC_Abort if
 * there is an abort request.
 *
 * If the Walker does not have an xSelectCallback() then this routine
 * is a no-op returning WRC_Continue.
 */
static int
sql_walk_select(struct Walker *base, struct Select * p,
		const char *title)
{
	if (p == NULL)
		return WRC_Continue;

	struct OutputWalker * walker = (struct OutputWalker*)base;
	struct ibuf * ibuf = walker->ibuf;
	char * data = ibuf->wpos;
	int rc = WRC_Continue;
	base->walkerDepth++;

	OUT_TUPLE_TITLE(walker->ibuf, title);

	// count number of selects in chain
	size_t n_selects = 0;
	struct Select * pp = p;
	while (pp) {
		n_selects++;
		pp = pp->pPrior;
	}
	OUT_ARRAY_N(ibuf, n_selects);
	while (p) {
		// estimate extra elements in map
		size_t extra = sql_walk_select_expr(base, p, true, NULL) +
			       sql_walk_select_from(base, p, true, NULL);
		
		// "select":{}
		OUT_TUPLE_TITLE(walker->ibuf, title);
		data = ibuf_alloc(ibuf, mp_sizeof_map(8 + extra));
		data = mp_encode_map(data, 8 + extra);

		OUT_V(ibuf, p, op, uint);
		OUT_V(ibuf, p, nSelectRow, Xint);
		OUT_V(ibuf, p, selFlags, uint);
		OUT_V(ibuf, p, iLimit, Xint);
		OUT_V(ibuf, p, iOffset, Xint);
		OUT_VS(ibuf, p, zSelName);
		OUT_V(ibuf, p, addrOpenEphm[0], Xint);
		OUT_V(ibuf, p, addrOpenEphm[1], Xint);
		if ((rc = sql_walk_select_expr(base, p, false, "expr")))
			goto return_error;
		if ((rc = sql_walk_select_from(base, p, false, "from")))
			goto return_error;
		
		p = p->pPrior;
	}
return_error:
	base->walkerDepth--;
	return rc & WRC_Abort;
}

static void
sqlparser_generate_msgpack_walker(struct Parse *parser,
				  struct ibuf *ibuf,
				  struct Select *p) 
{
	struct OutputWalker wlkr = {
		.base = {
			.xExprCallback = NULL, // outputExprStep,
			.xSelectCallback = NULL, //outputSelectStep,
			.pParse = parser,
			.u = { .pNC = NULL },
		},
		.accum = 0,
		.ibuf = ibuf,
		//.title = "select"
	};
	//struct region *region = &fiber()->gc;

	sql_walk_select(&wlkr.base, p, "select");

}

int
lbox_sqlparser_serialize(struct lua_State *L)
{
	int top = lua_gettop(L);
	assert(top == 1);
	(void)top;

	struct sql_parsed_ast *ast = luaT_check_sql_parsed_ast(L, 1);

	if (AST_VALID(ast)) {
		assert(ast->ast_type == AST_TYPE_SELECT);

		struct ibuf ibuf;
		ibuf_create(&ibuf, &cord()->slabc, 1024); // FIXME - precise estimate
		ibuf_reset(&ibuf);

		struct Parse parser;
		sql_parser_create(&parser, parser.db, default_flags);
		sqlparser_generate_msgpack_walker(&parser, &ibuf, ast->select);

		lua_pushlstring(L, ibuf.rpos, ibuf_used(&ibuf));
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
