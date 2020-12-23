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
#include "sqlparser.h"

#ifndef DISABLE_AST_CACHING
#include "box/sql_stmt_cache.h"
#endif

#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

uint32_t CTID_STRUCT_SQL_PARSED_AST = 0;
static uint32_t CTID_STRUCT_SQL_STMT = 0;

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

#ifndef DISABLE_AST_CACHING
static
void sql_ast_reset(struct sql_parsed_ast *ast)
{
	switch (ast->ast_type) {
		case AST_TYPE_SELECT: 	// SELECT
			sqlSelectReset(ast->select);
			break;
		default:
			assert(0);
	}
}
#endif

inline struct sql_parsed_ast *
luaT_check_sql_parsed_ast(struct lua_State *L, int idx)
{
	if (lua_type(L, idx) != LUA_TCDATA)
		return NULL;

	uint32_t cdata_type;
	struct sql_parsed_ast **sql_parsed_ast_ptr = luaL_checkcdata(L, idx, &cdata_type);
	if (sql_parsed_ast_ptr == NULL || cdata_type != CTID_STRUCT_SQL_PARSED_AST)
		return NULL;
	return *sql_parsed_ast_ptr;
}


static int
lbox_sql_parsed_ast_gc(struct lua_State *L)
{
	struct sql_parsed_ast *ast = luaT_check_sql_parsed_ast(L, 1);
	struct sql *db = sql_get();
	sql_parsed_ast_destroy(db, ast);

	return 0;
}

void
luaT_push_sql_parsed_ast(struct lua_State *L, struct sql_parsed_ast *ast)
{
	*(struct sql_parsed_ast **)
		luaL_pushcdata(L, CTID_STRUCT_SQL_PARSED_AST) = ast;
	lua_pushcfunction(L, lbox_sql_parsed_ast_gc);
	luaL_setcdatagc(L, -2);
}

struct sql_stmt *
luaT_check_sql_stmt(struct lua_State *L, int idx)
{
	if (lua_type(L, idx) != LUA_TCDATA)
		return NULL;

	uint32_t cdata_type;
	struct sql_stmt **sql_stmt_ptr = luaL_checkcdata(L, idx, &cdata_type);
	if (sql_stmt_ptr == NULL || cdata_type != CTID_STRUCT_SQL_STMT)
		return NULL;
	return *sql_stmt_ptr;
}


static int
lbox_sql_stmt_gc(struct lua_State *L)
{
#if 0
	struct sql_stmt *stmt = luaT_check_sql_stmt(L, 1);
	if (stmt)
		sqlVdbeDelete((Vdbe *)stmt);
#else
	(void)L;
#endif
	return 0;
}

void
luaT_push_sql_stmt(struct lua_State *L, struct sql_stmt *stmt)
{
	*(struct sql_stmt **)
		luaL_pushcdata(L, CTID_STRUCT_SQL_STMT) = stmt;
	lua_pushcfunction(L, lbox_sql_stmt_gc);
	luaL_setcdatagc(L, -2);
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

	struct sql_parsed_ast *ast = NULL;
#ifndef DISABLE_AST_CACHING
	uint32_t stmt_id = sql_stmt_calculate_id(sql, length);
	struct stmt_cache_entry *entry = stmt_cache_find_entry(stmt_id);

	if (entry == NULL) {
#endif
		struct sql_stmt *stmt = NULL;
		ast = sql_ast_alloc();

		if (sql_stmt_parse(sql, &stmt, ast) != 0)
			goto return_error;
#ifndef DISABLE_AST_CACHING
		if (sql_stmt_cache_insert(stmt, ast) != 0) {
			sql_stmt_finalize(stmt);
			goto return_error;
		}
	} else {
		ast = entry->ast;
		sql_ast_reset(ast);
		//goto return_error; // FIXME - some odd problems here
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

	lua_pushinteger(L, (lua_Integer)stmt_id);
#else
	if (AST_VALID(ast))
		luaT_push_sql_parsed_ast(L, ast);
	else
		luaT_push_sql_stmt(L, stmt);
#endif

	return 1;
return_error:
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

#ifndef DISABLE_AST_CACHING
static struct sql_stmt*
sql_ast_generate_vdbe(struct lua_State *L, struct stmt_cache_entry *entry)
{
	(void)L;
	struct sql_parsed_ast * ast = entry->ast;
	// nothing to generate yet - this kind of statement is
	// not (yet) supported. Eventually this limitation
	// will be lifted
	if (!AST_VALID(entry->ast))
		return entry->stmt;

	// assumption is that we have not yet completed
	// bytecode generation for parsed AST
	struct sql_stmt *stmt = entry->stmt;
	assert(stmt == NULL);
#else
static struct sql_stmt*
sql_ast_generate_vdbe(struct lua_State *L, struct sql_parsed_ast * ast)
{
	(void)L;

	// nothing to generate yet - this kind of statement is
	// not (yet) supported. Eventually this limitation
	// will be lifted
	if ( !AST_VALID(ast))
		return NULL;
#endif
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

			int rc = sqlSelect(&sParse, p, &dest);
			if (rc != 0)
				return NULL;
			break;
		}

		default:		// FIXME
		{
			assert(0);
		}
	}
	sql_finish_coding(&sParse);
	sql_parser_destroy(&sParse);

	return (struct sql_stmt*)sParse.pVdbe;
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
	(void)top;
#ifndef DISABLE_AST_CACHING
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
	struct sql_stmt *stmt = stmt = sql_ast_generate_vdbe(L, entry);
#else
	struct sql_parsed_ast *ast = luaT_check_sql_parsed_ast(L, 1);
	struct sql_stmt *stmt = NULL;
	if (ast == NULL)
		stmt = luaT_check_sql_stmt(L, 1);
	assert(ast != NULL || stmt != NULL); // FIXME - human readable error

	// 2. generate
	// 2a - supported case: SELECT
	if (AST_VALID(ast)) {
		stmt = sql_ast_generate_vdbe(L, ast);
	}
	// 2b - unsupported (yet) case - bail down to box.execute
	else {
		assert(stmt);
	}
#endif

	if (stmt == NULL)
		return luaT_push_nil_and_error(L);

	struct port port;
	struct region *region = &fiber()->gc;

	enum sql_serialization_format format =
		sql_column_count(stmt) > 0 ? DQL_EXECUTE : DML_EXECUTE;

	port_sql_create(&port, stmt, format, true);
	if (sql_execute(stmt, &port, region) != 0)
		goto return_error;

	sql_stmt_reset(stmt);
	port_dump_lua(&port, L, false);
	port_destroy(&port);

	return 1;

return_error:
	if (stmt != NULL)
		sql_stmt_reset(stmt);
	port_destroy(&port);
	return luaT_push_nil_and_error(L);
};


extern char sql_ast_cdef[];

void
box_lua_sqlparser_init(struct lua_State *L)
{
	int rc = luaL_cdef(L, sql_ast_cdef);
	if (rc != LUA_OK) {
		const char * error = lua_tostring(L, -1);
		panic("ffi cdef error: %s", error);
	}
	CTID_STRUCT_SQL_PARSED_AST = luaL_ctypeid(L, "struct sql_parsed_ast&");
	assert(CTID_STRUCT_SQL_PARSED_AST != 0);
	luaL_cdef(L, "struct sql_stmt;");
	CTID_STRUCT_SQL_STMT = luaL_ctypeid(L, "struct sql_stmt&");
	assert(CTID_STRUCT_SQL_STMT != 0);

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
