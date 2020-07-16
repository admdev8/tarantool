std = "luajit"
globals = {"box", "_TARANTOOL", "tonumber64", "utf8"}
ignore = {
    -- Accessing an undefined field of a global variable <debug>.
    "143/debug",
    -- Accessing an undefined field of a global variable <os>.
    "143/os",
    -- Accessing an undefined field of a global variable <string>.
    "143/string",
    -- Accessing an undefined field of a global variable <table>.
    "143/table",
    -- Unused argument <self>.
    "212/self",
    -- Redefining a local variable.
    "411",
    -- Redefining an argument.
    "412",
    -- Shadowing a local variable.
    "421",
    -- Shadowing an upvalue.
    "431",
    -- Shadowing an upvalue argument.
    "432",
}

include_files = {
    "**/*.lua",
    "extra/dist/tarantoolctl.in",
}

exclude_files = {
    "build/**/*.lua",
    "test-run/**/*.lua",
    "test/app/*.test.lua",
    "test/box/*.test.lua",
    -- Unused source file, to be dropped (gh-5169).
    "test/box/lua/require_init.lua",
    -- Unused source file, to be dropped (gh-5169).
    "test/box/lua/require_mod.lua",
    -- Unused source file, to be dropped (gh-5169).
    "test/box/lua/test_init.lua",
    "test/engine/*.test.lua",
    "test/engine_long/*.test.lua",
    "test/long_run-py/**/*.lua",
    "test/luajit-tap/**/*.lua",
    "test/replication/**/*.lua",
    "test/replication-py/**/*.lua",
    "test/sql/**/*.lua",
    "test/swim/**/*.lua",
    "test/var/**/*.lua",
    "test/vinyl/**/*.lua",
    "test/wal_off/**/*.lua",
    "test/xlog/**/*.lua",
    "test/xlog-py/**/*.lua",
    "third_party/**/*.lua",
    ".rocks/**/*.lua",
    ".git/**/*.lua",
}

files["test/sql-tap/**/*.lua"] = {
    ignore = {
        -- Accessing an undefined global variable.
        "113",
        -- Unused local variable.
        "211",
        -- Unused argument.
        "212",
        -- Unused loop variable.
        "213",
        -- Local variable is set but never accessed.
        "231",
        -- "Value assigned to a local variable is unused."
        "311",
        -- Unreachable code.
        "511",
        -- Loop can be executed at most once.
        "512",
        -- An empty if branch.
        "542",
        -- A line consists of nothing but whitespace.
        "611",
        -- A line contains trailing whitespace.
        "612",
        -- Trailing whitespace in a string.
        "613",
        -- Trailing whitespace in a comment.
        "614",
        -- Inconsistent indentation (SPACE followed by TAB).
        "621",
        -- Line is too long.
        "631"
    }
}

files["src/lua/help.lua"] = {
    -- Globals defined for interactive mode.
    globals = {"help", "tutorial"},
}
files["src/lua/init.lua"] = {
    -- Miscellaneous global function definition.
    globals = {"dostring"},
}
files["src/box/lua/console.lua"] = {
    ignore = {
        -- https://github.com/tarantool/tarantool/issues/5032
        "212",
    }
}
files["test/box/box.lua"] = {
    globals = {
        "cfg_filter",
        "sorted",
        "iproto_request",
    }
}
files["test/box/lua/push.lua"] = {
    globals = {"push_collection"}
}
files["test/box/lua/index_random_test.lua"] = {
    globals = {"index_random_test"}
}
files["test/box/lua/utils.lua"] = {
    globals = {
        "arithmetic",
        "check_space",
        "create_iterator",
        "iterate",
        "setmap",
        "sort",
        "space_field_types",
        "space_bsize",
        "table_generate",
        "table_shuffle",
        "tuple_to_string",
    }
}
files["test/box/lua/bitset.lua"] = {
    globals = {
        "clear",
        "create_space",
        "delete",
        "drop_space",
        "dump",
        "fill",
        "test_insert_delete",
    }
}
files["test/box/lua/fifo.lua"] = {
    globals = {
        "fifomax",
        "fifo_push",
        "fifo_top",
        "find_or_create_fifo",
    }
}
files["test/box/lua/identifier.lua"] = {
     globals = {"run_test"}
}
files["test/box-tap/session.test.lua"] = {
    globals = {
        "active_connections",
        "session",
        "space",
        "f1",
        "f2",
    },
}
files["test/box-tap/extended_error.test.lua"] = {
    globals = {
        "error_access_denied",
        "error_new",
        "error_new_stacked",
        "error_throw",
        "error_throw_stacked",
        "error_throw_access_denied",
        "forbidden_function",
    }
}
files["test/engine/conflict.lua"] = {
    globals = {
        "test_conflict",
    }
}
files["test/engine_long/suite.lua"] = {
    globals = {
        "delete_replace_update",
        "delete_insert",
    }
}
