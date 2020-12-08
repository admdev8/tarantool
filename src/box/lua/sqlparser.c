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
#include "box/sql_stmt_cache.h"

#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int CTID_STRUCT_SQL_PARSED_AST = 0;

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

	if (rc != 0 || db->mallocFailed) {
		sqlVdbeFinalize(sParse.pVdbe);
		assert(!(*ppStmt));
		goto exit_cleanup;
	}
	// we have either AST or VDBE, but not both
	assert(SQL_PARSE_VALID_VDBE(&sParse) != SQL_PARSE_VALID_AST(&sParse));
	if (SQL_PARSE_VALID_VDBE(&sParse)) {
		if (db->init.busy == 0) {
			Vdbe *pVdbe = sParse.pVdbe;
			sqlVdbeSetSql(pVdbe, zSql, (int)(sParse.zTail - zSql));
		}
		*ppStmt = (sql_stmt *) sParse.pVdbe;

		/* Delete any TriggerPrg structures allocated while parsing this statement. */
		while (sParse.pTriggerPrg) {
			TriggerPrg *pT = sParse.pTriggerPrg;
			sParse.pTriggerPrg = pT->pNext;
			sqlDbFree(db, pT);
		}
	} else {	// AST constructed
		assert(SQL_PARSE_VALID_AST(&sParse));
		*ast = sParse.parsed_ast;
		assert(ast->keep_ast == true);
		sql_ast_set_sql(ast, zSql, (int)(sParse.zTail - zSql));
	}

exit_cleanup:
	sql_parser_destroy(&sParse);
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
			goto error;
		if (sql_stmt_cache_insert(stmt, ast) != 0) {
			sql_stmt_finalize(stmt);
			goto error;
		}
	} else {
#if 0
		if (sql_stmt_schema_version(stmt) != box_schema_version() &&
		    !sql_stmt_busy(stmt)) {
			; //if (sql_reprepare(&stmt) != 0)
			//	goto error;
		}
#endif
	}
	assert(ast != NULL);
	/* Add id to the list of available statements in session. */
	if (!session_check_stmt_id(current_session(), stmt_id))
		session_add_stmt_id(current_session(), stmt_id);

#if 0 
	struct sql_parsed_ast** ppast =
		luaL_pushcdata(L, CTID_STRUCT_SQL_PARSED_AST);
	*ppast = ast;
#else
	lua_pushinteger(L, (lua_Integer)stmt_id);

#endif

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

static struct sql_stmt*
sql_ast_generate_vdbe(struct lua_State *L, struct stmt_cache_entry *entry)
{
	(void)L;
	struct sql_parsed_ast * ast = entry->ast;
	// nothing to generate yet - this kind of statement is 
	// not (yet) supported. Eventually will be removed.
	if (!AST_VALID(entry->ast))
		return entry->stmt;

	// assumption is that we have not yet completed
	// bytecode generation for parsed AST
	struct sql_stmt *stmt = entry->stmt;
	assert(stmt == NULL);
	struct sql *db = sql_get();

	Parse sParse = {0};
	sql_parser_create(&sParse, db, current_session()->sql_flags);
	sParse.parse_only = false;

	struct Vdbe *v = sqlGetVdbe(&sParse);
	if (v == NULL) {
		sql_parser_destroy(&sParse);
		diag_set(OutOfMemory, sizeof(struct Vdbe), "sqlGetVdbe",
			 "sqlparser");
		return NULL;
	}

	// we already parsed AST, thus not calling sqlRunParser

	switch (ast->ast_type) {
		case AST_TYPE_SELECT: 	// SELECT
		{
			Select *p = ast->select;
			SelectDest dest = {SRT_Output, NULL, 0, 0, 0, 0, NULL};

			sqlSelect(&sParse, p, &dest);
			sql_select_delete(sParse.db, p);
			break;
		}

		default:		// FIXME
		{
			assert(0);
		}
	}
	sql_finish_coding(&sParse);
	sql_parser_destroy(&sParse);

	stmt = (struct sql_stmt*)sParse.pVdbe;
	return stmt;
}

static int
lbox_sqlparser_execute(struct lua_State *L)
{
	int top = lua_gettop(L);
#if 0
	struct sql_bind *bind = NULL;
	int bind_count = 0;
	size_t length;
	struct port port;

	if (top == 2) {
		if (! lua_istable(L, 2))
			return luaL_error(L, "Second argument must be a table");
		bind_count = lua_sql_bind_list_decode(L, &bind, 2);
		if (bind_count < 0)
			return luaT_push_nil_and_error(L);
	}

#endif
	assert(top == 1);
	// FIXME - assuming we are receiving a single 
	// argument of a prepared AST handle
	assert(lua_type(L, 1) == LUA_TNUMBER);
	lua_Integer query_id = lua_tointeger(L, 1);
#if 0
	if (!session_check_stmt_id(current_session(), stmt_id)) {
		diag_set(ClientError, ER_WRONG_QUERY_ID, stmt_id);
		return -1;
	}
#endif

	struct stmt_cache_entry *entry = stmt_cache_find_entry(query_id);
	assert(entry != NULL);

	// 2. generate 
	struct sql_stmt *stmt = NULL;
	struct port port;
	struct region *region = &fiber()->gc;

	if ((stmt = sql_ast_generate_vdbe(L, entry))) {
		enum sql_serialization_format format = 
			sql_column_count(stmt) > 0 ? DQL_EXECUTE : DML_EXECUTE;

		port_sql_create(&port, stmt, format, true);
		if (sql_execute(stmt, &port, region) != 0) {
			port_destroy(&port);
			sql_stmt_reset(stmt);
			return luaT_push_nil_and_error(L);
		}
	}
	sql_stmt_reset(stmt);
	port_dump_lua(&port, L, false);
	port_destroy(&port);

	return 1;
};

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

extern char sql_ast_ffi_defs_lua[];

void
box_lua_sqlparser_init(struct lua_State *L)
{
#if 0
	luaL_cdef(L, "struct sql_parsed_ast;");
#else
	luaL_cdef(L, sql_ast_ffi_defs_lua);
#endif
	CTID_STRUCT_SQL_PARSED_AST = luaL_ctypeid(L, "struct sql_parsed_ast&");
	assert(CTID_STRUCT_SQL_PARSED_AST != 0);

	static const struct luaL_Reg meta[] = {
		{ "parse", lbox_sqlparser_parse },
		{ "unparse", lbox_sqlparser_unparse },
		{ "serialize", lbox_sqlparser_serialize },
		{ "deserialize", lbox_sqlparser_deserialize },
		{ "execute", lbox_sqlparser_execute },
		{ NULL, NULL },
	};
	luaL_register_module(L, "sqlparser", meta);
	lua_pop(L, 1);

	return;
}
