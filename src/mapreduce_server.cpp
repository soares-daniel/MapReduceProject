#include "in_memory_state_mgr.hxx"
#include "logger_wrapper.hxx"

#include "nuraft.hxx"

#include "test_common.h"


#include "mr_state_machine.cpp"

#include <iostream>
#include <sstream>

#include <stdio.h>

using namespace nuraft;

namespace mapreduce_server {

static raft_params::return_method_type CALL_TYPE
    = raft_params::blocking;
//  = raft_params::async_handler;

static bool ASYNC_SNAPSHOT_CREATION = false;

#include "example_common.hxx"

mr_state_machine* get_sm() {
    return static_cast<mr_state_machine*>( stuff.sm_.get() );
}

void handle_result(ptr<TestSuite::Timer> timer,
                   raft_result& result,
                   ptr<std::exception>& err)
{
    if (result.get_result_code() != cmd_result_code::OK) {
        // Something went wrong.
        std::cout << "failed: " << result.get_result_code() << ", "
                  << TestSuite::usToString( timer->getTimeUs() )
                  << std::endl;
        return;
    }

    ptr<buffer> buf = result.get();
    uint64_t log_idx = buf->get_ulong(); // Log index
    bool has_map_reduce_results = buf->get_byte() != 0; // Deserialize flag

    std::cout << "succeeded, log index: " << log_idx << std::endl;
    if (has_map_reduce_results) {
        std::map<std::string, int> mapReduceResults = get_sm()->get_map_reduce_results(log_idx);
        std::cout << "MapReduce results:" << std::endl;
        for (const auto& kv : mapReduceResults) {
            std::cout << kv.first << ": " << kv.second << std::endl;
        }
    }
}

void append_log(mr_state_machine::op_payload& payload) {
    // Rest of the previous append_log
    ptr<buffer> new_log = mr_state_machine::enc_log(payload);

    // To measure the elapsed time.
    ptr<TestSuite::Timer> timer = cs_new<TestSuite::Timer>();

    // Do append.
    ptr<raft_result> ret = stuff.raft_instance_->append_entries( {new_log} );

    if (!ret->get_accepted()) {
        // Log append rejected, usually because this node is not a leader.
        std::cout << "failed to replicate: "
                  << ret->get_result_code() << ", "
                  << TestSuite::usToString( timer->getTimeUs() )
                  << std::endl;
        return;
    }
    // Log append accepted, but that doesn't mean the log is committed.
    // Commit result can be obtained below.

    if (CALL_TYPE == raft_params::blocking) {
        // Blocking mode:
        //   `append_entries` returns after getting a consensus,buffer_serializer
        //   so that `ret` already has the result from state machine.
        ptr<std::exception> err(nullptr);
        handle_result(timer, *ret, err);

    } else if (CALL_TYPE == raft_params::async_handler) {
        // Async mode:
        //   `append_entries` returns immediately.
        //   `handle_result` will be invoked asynchronously,
        //   after getting a consensus.
        ret->when_ready( std::bind( handle_result,
                                    timer,
                                    std::placeholders::_1,
                                    std::placeholders::_2 ) );

    } else {
        assert(0);
    }
}

void handle_kv_command(const std::string& cmd,
                const std::vector<std::string>& tokens)
{
    if (tokens.size() < 2) {
        std::cerr << "Error: Invalid command format for " << cmd << std::endl;
        return;
    }

    const std::string& key = tokens[1];
    mr_state_machine::op_type op;
    int value = 0; // Default value

    if (cmd == "+") {
        if (tokens.size() != 3) {
            std::cerr << "Error: Invalid command format for addition. Usage: + <key> <value>" << std::endl;
            return;
        }
        value = std::stoi(tokens[2]);
        op = mr_state_machine::INSERT_VALUE;

    } else if (cmd == "-") {
        if (tokens.size() == 3) {
            // Command to delete a specific value
            value = std::stoi(tokens[2]);
            op = mr_state_machine::DELETE_VALUE;
        } else {
            // Command to delete the entire key
            op = mr_state_machine::DELETE_KEY;
        }

    } else {
        std::cerr << "Error: Unknown command " << cmd << std::endl;
        return;
    }

    // Serialize and generate Raft log to append.
    mr_state_machine::op_payload payload = {op, key, value};
    mapreduce_server::append_log(payload);
}

void handle_map_reduce_command(const std::vector<std::string>& tokens) {
    if (tokens.size() < 7) {
        std::cerr << "Error: Invalid command format for mapReduce" << std::endl;
        return;
    }

    std::string mapFunc, reduceFunc;
    std::vector<std::string> keys;
    bool isKeyFlag = false;

    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token == "--m") {
            mapFunc = tokens[++i];
            isKeyFlag = false;
        } else if (token == "--r") {
            reduceFunc = tokens[++i];
            isKeyFlag = false;
        } else if (token == "--k") {
            isKeyFlag = true;
        } else if (isKeyFlag) {
            keys.push_back(token);
        }
    }

    if (mapFunc.empty() || reduceFunc.empty() || keys.empty()) {
        std::cerr << "Error: Invalid command format for mapReduce" << std::endl;
        return;
    }

    mr_state_machine::op_payload payload = {mr_state_machine::MAP_REDUCE, "NULL", 0, mapFunc, reduceFunc, keys};
    mapreduce_server::append_log(payload);
}

void print_kv_store() {
    auto kv_pairs = get_sm()->get_kv_store().getAll();
    for (const auto& kv : kv_pairs) {
        std::cout << kv.first << ": ";
        const auto& values = kv.second;
        for (size_t i = 0; i < values.size(); ++i) {
            std::cout << values[i];
            if (i < values.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << std::endl;
    }
}

void print_status(const std::string& cmd,
                  const std::vector<std::string>& tokens)
{
    ptr<log_store> ls = stuff.smgr_->load_log_store();

    std::cout
        << "my server id: " << stuff.server_id_ << std::endl
        << "leader id: " << stuff.raft_instance_->get_leader() << std::endl
        << "Raft log range: ";
    if (ls->start_index() >= ls->next_slot()) {
        // Start index can be the same as next slot when the log store is empty.
        std::cout << "(empty)" << std::endl;
    } else {
        std::cout << ls->start_index()
                  << " - " << (ls->next_slot() - 1) << std::endl;
    }
    std::cout
        << "last committed index: "
            << stuff.raft_instance_->get_committed_log_idx() << std::endl
        << "current term: "
            << stuff.raft_instance_->get_term() << std::endl
        << "last snapshot log index: "
            << (stuff.sm_->last_snapshot()
                ? stuff.sm_->last_snapshot()->get_last_log_idx() : 0) << std::endl
        << "last snapshot log term: "
            << (stuff.sm_->last_snapshot()
                ? stuff.sm_->last_snapshot()->get_last_log_term() : 0) << std::endl
        << "Key-Value Store Contents:" << std::endl;
        print_kv_store();
}

void help(const std::string& cmd,
          const std::vector<std::string>& tokens)
{
    std::cout
    << "KV Store:\n"
    << "  mapReduce --m <map_func> --r <reduce_func> --k <keys> - Apply MapReduce\n"
    << "  + <key> <value> - Add value to key\n"
    << "  - <key> - Remove key\n"
    << "  - <key> <value> - Remove value from key\n"
    << "  store - Display all key-value pairs\n"
    << "\n"
    << "add server: add <server id> <address>:<port>\n"
    << "    e.g.) add 2 127.0.0.1:20000\n"
    << "\n"
    << "get current server status: st (or stat)\n"
    << "\n"
    << "get the list of members: ls (or list)\n"
    << "\n"
    << "exit - Exit this program\n";
}


bool do_cmd(const std::vector<std::string>& tokens) {
    if (!tokens.size()) return true;

    const std::string& cmd = tokens[0];

    if (cmd == "q" || cmd == "exit") {
        stuff.launcher_.shutdown(5);
        stuff.reset();
        return false;

    } else if (cmd == "mapReduce") {
        handle_map_reduce_command(tokens);

    } else if (cmd == "+") {
        handle_kv_command(cmd, tokens);

    } else if (cmd == "-") {
        handle_kv_command(cmd, tokens);

    } else if (cmd == "store") {
            print_kv_store();

    } else if ( cmd == "add" ) {
        // e.g.) add 2 localhost:12345
        add_server(cmd, tokens);

    } else if ( cmd == "st" || cmd == "stat" ) {
        print_status(cmd, tokens);

    } else if ( cmd == "ls" || cmd == "list" ) {
        server_list(cmd, tokens);

    } else if ( cmd == "h" || cmd == "help" ) {
        help(cmd, tokens);
    }
    return true;
}

void check_additional_flags(int argc, char** argv) {
    for (int ii = 1; ii < argc; ++ii) {
        if (strcmp(argv[ii], "--async-handler") == 0) {
            CALL_TYPE = raft_params::async_handler;
        } else if (strcmp(argv[ii], "--async-snapshot-creation") == 0) {
            ASYNC_SNAPSHOT_CREATION = true;
        }
    }
}

void mr_server_usage(int argc, char** argv) {
    std::stringstream ss;
    ss << "Usage: \n";
    ss << "    " << argv[0] << " <server id> <IP address and port> [<options>]";
    ss << std::endl << std::endl;
    ss << "    options:" << std::endl;
    ss << "      --async-handler: use async type handler." << std::endl;
    ss << "      --async-snapshot-creation: create snapshots asynchronously."
       << std::endl << std::endl;

    std::cout << ss.str();
    exit(0);
}

};
using namespace mapreduce_server;

int main(int argc, char** argv) {
    if (argc < 3) mr_server_usage(argc, argv);

    set_server_info(argc, argv);
    check_additional_flags(argc, argv);

    std::cout << "    -- Replicated MapReduce with Raft --" << std::endl;
    std::cout << "    Version 0.1.0" << std::endl;
    std::cout << "    Server ID:    " << stuff.server_id_ << std::endl;
    std::cout << "    Endpoint:     " << stuff.endpoint_ << std::endl;
    if (CALL_TYPE == raft_params::async_handler) {
        std::cout << "    async handler is enabled" << std::endl;
    }
    if (ASYNC_SNAPSHOT_CREATION) {
        std::cout << "    snapshots are created asynchronously" << std::endl;
    }
    init_raft( cs_new<mr_state_machine>(ASYNC_SNAPSHOT_CREATION) );
    loop();

    return 0;
}
