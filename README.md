Replicated Key-Value-Store with Map-Reduce
---------
Simple CLI-based KV-Store with Map Reduce with Raft replication.
* State machine: defined as KV-Store.
* Map-Reduce: Pre-defined operations for Map and Reduce
    * Map operations: `square`, `double` and `triple`.
    * Reduce operations: `sum` and `product`.

Files
-----
* [mapreduce_server.cpp](src/mapreduce_server.cpp)
    * Main server file. Initiate Raft server and handle CLI commands.
* [mr_state_machine.cpp](src/mr_state_machine.cpp):
    * State machine implementation (volatile).
* [KeyValueStore.cpp](src/KeyValueStore.cpp):
    * KV-Store implementation.
* [MapReduce.cpp](src/MapReduce.cpp):
    * Map-Reduce implementation
  
Installation
-----
1. Make sure to have NuRaft installed.
2. CMake expects NuRaft to be installed in `/usr/local/include`. 
   You might need to adjust [CMakeLists.txt](CMakeLists.txt) to include the right directory.
3. To build, perform the following command in your Terminal:
   ```
   mkdir build && cd build && cmake ../ && make
   ```

Consistency and Durability
-----
Note that everything is volatile; nothing will be written to disk, and server will lose data once its process terminates.

However, as long as quorum nodes are alive, committed data will not be lost in the entire Raft group's point of view. When a server exits and then re-starts, it will do catch-up with the current leader and recover all committed data.

How to Run
-----
Run server instances in different terminals.
```
build$ ./mapreduce_server 1 localhost:10001
```
```
build$ ./mapreduce_server 2 localhost:10002
```
```
build$ ./mapreduce_server 3 localhost:10003
```

Choose a server that will be the initial leader, and add the other two.
```
mapReduce 1> add 2 localhost:10002
async request is in progress (check with `list` command)
mapReduce 1> add 3 localhost:10003
async request is in progress (check with `list` command)
mapReduce 1>
```

Now 3 servers organize a Raft group.
```
mapReduce 1> list
server id 1: tcp://localhost:10001 (LEADER)
server id 2: tcp://localhost:10002
server id 3: tcp://localhost:10003
mapReduce 1>
```

You can find all commands with `help`.
```
mapReduce 1> help
KV Store:
  mapReduce --m <map_func> --r <reduce_func> --k <keys> - Apply MapReduce
  + <key> <value> - Add value to key
  - <key> - Remove key
  - <key> <value> - Remove value from key
  store - Display all key-value pairs

add server: add <server id> <address>:<port>
    e.g.) add 2 127.0.0.1:20000

get current server status: st (or stat)

get the list of members: ls (or list)

exit - Exit this program
```

Add/Remove from KV-Store.
```
mapReduce 1> + books 1
succeeded, log index: 4
mapReduce 1> + books 3
succeeded, log index: 5
mapReduce 1> store
books: 1, 3
mapReduce 1> - books 1
succeeded, log index: 6
mapReduce 1> store
books: 3
mapReduce 1> + example 4
succeeded, log index: 7
mapReduce 1> store
books: 3
example: 4
mapReduce 1> - books
succeeded, log index: 8
mapReduce 1> store
example: 4
```

Perform Map-Reduce.
```
mapReduce 1> store
books: 1, 3, 5, 6, 8
devices: 2, 5, 1
mapReduce 1> mapReduce --m double --r sum --k books devices   
succeeded, log index: 14
MapReduce results: 
books : 46
devices : 16
```

All servers should have the same state machine value.
```
mapReduce 2> st
my server id: 2
leader id: 1
Raft log range: 1 - 8
current term: 1
last snapshot log index: 5
last snapshot log term: 1
Key-Value Store Contents:
example: 4
mapReduce 2>
```

Server loses its data after termination. However, on server re-start, it will recover committed data by catch-up with the current leader.
```
mapReduce 3> exit

build$ ./mapreduce_server 3 localhost:10003
mapReduce 3> st
my server id: 3
Raft log range: 1 - 8
current term: 1
last snapshot log index: 5
last snapshot log term: 1
Key-Value Store Contents:
example: 4
mapReduce 3>
```

Contribution
-----

The implementation of the [KeyValueStore](src/KeyValueStore.cpp) as well as [MapReduce](src/MapReduce.cpp) was done by Thomas Rosa Da Silva (0180981748).

The implementation of the [Server](src/mapreduce_server.cpp) and the [State Machine](src/mr_state_machine.cpp) as done by Daniel Soares Vieira (0211348824).


Issues
-----

When performing Map-Reduce, there is a double free of an object which we were not able to fix.