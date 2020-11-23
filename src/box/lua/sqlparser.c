#include "diag.h"
#include "execute.h"
#include "lua/utils.h"
#include "sqlInt.h"
#include "../sql/vdbe.h"	// FIXME
#include "../execute.h"		// FIXME
#include "../schema.h"		// FIXME
#include "../session.h"		// FIXME
#include "../box.h"		// FIXME
#include "box/sql_stmt_cache.h"

#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * Remember the SQL string for a prepared statement.
 * Looks same as sqlVdbeSetSql but for AST, not VDBE
 */
static void
sql_ast_set_sql(struct sql_parsed_ast *ast, const char *ps, int sz)
{
	if (ast == NULL)
		return;
	assert(ast->sql_query == NULL);
	ast->sql_query = sqlDbStrNDup(sql_get(), ps, sz);
}

int
sql_stmt_parse(const char *zSql, sql_stmt **ppStmt, struct sql_parsed_ast *ast)
{
	struct sql *db = sql_get();
	int rc = 0;	/* Result code */
	Parse sParse;
	sql_parser_create(&sParse, db, current_session()->sql_flags);

	sParse.parse_only = true;	// Parse and build AST only
	sParse.parsed_ast.keep_ast = true;

	*ppStmt = NULL;
	/* assert( !db->mallocFailed ); // not true with SQL_USE_ALLOCA */

	sqlRunParser(&sParse, zSql);
	assert(0 == sParse.nQueryLoop);

	if (sParse.is_aborted)
		rc = -1;

	assert(sParse.pVdbe == NULL); // FIXME
	if (sParse.pVdbe != NULL && (rc != 0 || db->mallocFailed)) {
		sqlVdbeFinalize(sParse.pVdbe);
		assert(!(*ppStmt));
	} else {
		*ppStmt = (sql_stmt *) sParse.pVdbe;
	}
	*ast = sParse.parsed_ast;
	assert(ast->keep_ast == true);
	//if (db->init.busy == 0)
	sql_ast_set_sql(ast, zSql, (int)(sParse.zTail - zSql));

#if 0 // FIXME
	/* Delete any TriggerPrg structures allocated while parsing this statement. */
	while (sParse.pTriggerPrg) {
		TriggerPrg *pT = sParse.pTriggerPrg;
		sParse.pTriggerPrg = pT->pNext;
		sqlDbFree(db, pT);
	}
#endif

	sql_parser_destroy(&sParse); // FIXME
	return rc;
}

/**
 * Parse SQL to AST, return it as cdata
 * FIXME - split to the Lua and SQL parts..
 */
static int
lbox_sqlparser_parse(struct lua_State *L)
{
	if (!box_is_configured())
		luaL_error(L, "Please call box.cfg{} first");
	size_t length;
	int top = lua_gettop(L);

	if (top != 1 || !lua_isstring(L, 1))
		return luaL_error(L, "Usage: sqlparser.parse(sqlstring)");

	const char *sql = lua_tolstring(L, 1, &length);

	uint32_t stmt_id = sql_stmt_calculate_id(sql, length);
	struct sql_stmt *stmt = sql_stmt_cache_find(stmt_id);
	struct sql_parsed_ast *ast = sql_ast_alloc();

	if (stmt == NULL) {
		if (sql_stmt_parse(sql, &stmt, ast) != 0)
			return -1;
		if (sql_stmt_cache_insert(stmt, ast) != 0) {
			sql_stmt_finalize(stmt);
			goto error;
		}
	} else {
		if (sql_stmt_schema_version(stmt) != box_schema_version() &&
		    !sql_stmt_busy(stmt)) {
			; //if (sql_reprepare(&stmt) != 0)
			//	goto error;
		}
	}
	assert(ast != NULL);
	/* Add id to the list of available statements in session. */
	if (!session_check_stmt_id(current_session(), stmt_id))
		session_add_stmt_id(current_session(), stmt_id);

	return 1;
error:
	return luaT_push_nil_and_error(L);
}

static int
lbox_sqlparser_unparse(struct lua_State *L)
{
	int top = lua_gettop(L);

	if (top != 1 || !lua_isnumber(L, 1)) {
		return luaL_error(L, "Usage: sqlparser.unparse(stmt_id)");
	}
	lua_Integer stmt_id = lua_tonumber(L, 1);

	if (stmt_id < 0)
		return luaL_error(L, "Statement id can't be negative");
	if (sql_unprepare((uint32_t) stmt_id) != 0)
		return luaT_push_nil_and_error(L);
	return 0;
}

static int
lbox_sqlparser_serialize(struct lua_State *L)
{
	lua_pushliteral(L, "sqlparser.serialize");
	return 1;
}

static int
lbox_sqlparser_deserialize(struct lua_State *L)
{
	lua_pushliteral(L, "sqlparser.deserialize");
	return 1;
}

void
box_lua_sqlparser_init(struct lua_State *L)
{
	static const struct luaL_Reg meta[] = {
		{ "parse", lbox_sqlparser_parse },
		{ "unparse", lbox_sqlparser_unparse },
		{ "serialize", lbox_sqlparser_serialize },
		{ "deserialize", lbox_sqlparser_deserialize },
		{ NULL, NULL },
	};
	luaL_register_module(L, "sqlparser", meta);
	lua_pop(L, 1);

	return;
}
