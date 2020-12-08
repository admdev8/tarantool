#!/usr/bin/env tarantool

local common = require('process_timeout')
local tap = require('tap')
local fio = require('fio')
local ffi = require('ffi')

--
-- gh-4983: tarantool -e 'assert(false)' hangs
--

local TARANTOOL_PATH = arg[-1]
local output_file = fio.abspath('out.txt')
local line = ('%s -e "assert(false)" > %s 2>&1 & echo $!'):
        format(TARANTOOL_PATH, output_file)

-- Like a default timeout for `cond_wait` in test-run
local process_waiting_timeout = 60.0
local file_read_timeout = 60.0
local file_open_timeout = 60.0
local file_read_interval = 0.01

local res = tap.test('gh-4983-tnt-e-assert-false-hangs', function(test)
    test:plan(2)

    local pid = tonumber(io.popen(line):read('*line'))
    assert(pid, 'pid of proccess can\'t be recieved')

    local process_completed = common.wait_process_completion(pid,
            process_waiting_timeout)

    local details
    pcall(function()
        details = {
            cmdline = fio.open(('/proc/%d/cmdline'):format(pid),
                               {'O_RDONLY'}):read(1000000),
            status = fio.open(('/proc/%d/status'):format(pid),
                              {'O_RDONLY'}):read(1000000),
        }
    end)
    test:ok(process_completed,
            ('tarantool process with pid = %d completed'):format(pid),
            details)

    -- Kill process if hangs.
    if not process_completed then ffi.C.kill(pid, 9) end

    local fh = common.open_with_timeout(output_file, file_open_timeout)
    assert(fh, 'error while opening ' .. output_file)

    local data = common.read_with_timeout(fh, file_read_timeout, file_read_interval)
    test:like(data, 'assertion failed', 'assertion failure is displayed')

    fh:close()
end)

os.exit(res and 0 or 1)
