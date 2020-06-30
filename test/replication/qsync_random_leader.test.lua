env = require('test_run')
math = require('math')
fiber = require('fiber')
test_run = env.new()
engine = test_run:get_cfg('engine')

MAX_REPLICAS = 3
NUM_INSTANCES = MAX_REPLICAS + 1
BROKEN_QUORUM = NUM_INSTANCES + 1

SERVERS = {}
test_run:cmd("setopt delimiter ';'")
for i=1,MAX_REPLICAS do
    SERVERS[i] = 'qsync' .. i
end;
test_run:cmd("setopt delimiter ''");
SERVERS -- print instance names

test_run:create_cluster(SERVERS, "replication", {args="0.1"})
test_run:wait_fullmesh(SERVERS)
box.schema.user.grant('guest', 'replication')
test_run:switch('default')
box.cfg{replication_synchro_quorum=NUM_INSTANCES, replication_synchro_timeout=0.1}
_ = box.schema.space.create('sync', {is_sync=true, engine=engine})
_ = box.space.sync:create_index('pk')

-- Testcase body.
current_leader = 'default'
for i=1,100 do                                               \
    new_leader_id = math.random(1, #SERVERS)                 \
    new_leader = SERVERS[new_leader_id]                      \
    test_run:switch(new_leader)                              \
    box.cfg{read_only=false}                                 \
    fiber = require('fiber')                                 \
    f = fiber.create(function() box.space.sync:delete{} end) \
    f = fiber.create(function() for i=1,10 do box.space.sync:insert{i} end end) \
    f.status()                                               \
    test_run:switch('default')                               \
    test_run:switch(current_leader)                          \
    box.cfg{read_only=true}                                  \
    test_run:switch('default')                               \
    current_leader = new_leader                              \
    fiber.sleep(0.1)                                         \
end

-- Teardown.
test_run:switch(current_leader)
box.cfg{read_only=true} -- demote master to replica
test_run:switch('default')
box.cfg{read_only=false} -- promote replica to master
box.space.sync:drop()
test_run:drop_cluster(SERVERS)
