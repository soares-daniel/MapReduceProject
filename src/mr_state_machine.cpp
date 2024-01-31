#pragma once

#include "nuraft.hxx"

#include "MapReduce.h"
#include "KeyValueStore.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
#include <memory>
#include <sstream>

#include <string.h>

using namespace nuraft;

namespace mapreduce_server {

class mr_state_machine : public state_machine {
public:
    mr_state_machine(bool async_snapshot = false)
        : kv_store_(), last_committed_idx_(0), async_snapshot_(async_snapshot) {}

    ~mr_state_machine() {}

    enum op_type : int {
        INSERT_VALUE = 0x0,
        DELETE_VALUE = 0x1,
        DELETE_KEY = 0x2,
        MAP_REDUCE = 0x3
    };

    struct op_payload {
        op_type type_;
        std::string key_;
        int value_;  // For INSERT_KEY
        std::string map_op_;            // For MAP_REDUCE
        std::string reduce_op_;         // For MAP_REDUCE
        std::vector<std::string> keys_; // For MAP_REDUCE
    };

    static ptr<buffer> enc_log(const op_payload& payload) {
        // Encode from payload to Raft log.
        ptr<buffer> ret = buffer::alloc(sizeof(op_payload));
        buffer_serializer bs(ret);

        // WARNING: We don't consider endian-safety in this example.
        bs.put_raw(&payload, sizeof(op_payload));

        return ret;
    }

    static void dec_log(buffer& log, op_payload& payload_out) {
        // Decode from Raft log to payload pair.
        assert(log.size() == sizeof(op_payload));

        buffer_serializer bs(log);
        memcpy(&payload_out, bs.get_raw(log.size()), sizeof(op_payload));
    }

    ptr<buffer> pre_commit(const ulong log_idx, buffer& data) {
        // Nothing to do with pre-commit in this example.
        return nullptr;
    }

    ptr<buffer> commit(const ulong log_idx, buffer& data) {
        op_payload payload;
        dec_log(data, payload);

        std::unique_ptr<MapReduce> mr = nullptr;
        ptr<buffer> ret;

        bool has_map_reduce_results = false;
        std::string results_str;

        switch (payload.type_) {
            case INSERT_VALUE:
                kv_store_.insert(payload.key_, payload.value_);
                break;

            case DELETE_VALUE:
                kv_store_.removeValue(payload.key_, payload.value_);
                break;

            case DELETE_KEY:
                kv_store_.removeKey(payload.key_);
                break;

            case MAP_REDUCE: {
                has_map_reduce_results = true;
                mr = std::unique_ptr<MapReduce>(new MapReduce(kv_store_));
                auto mapReduceResults = mr->performMapReduce(payload.map_op_, payload.reduce_op_, payload.keys_);
                add_map_reduce_result(log_idx, mapReduceResults);
                break;
            }

            default:
                // Handle unknown operation
                break;
        }

        ret = buffer::alloc( sizeof(log_idx) + sizeof(bool) );
        buffer_serializer bs(ret);
        bs.put_u64(log_idx);
        bs.put_u8(static_cast<uint8_t>(has_map_reduce_results)); // Serialize flag indicating presence of results

        last_committed_idx_ = log_idx;
        return ret;
    }

    void commit_config(const ulong log_idx, ptr<cluster_config>& new_conf) {
        // Nothing to do with configuration change. Just update committed index.
        last_committed_idx_ = log_idx;
    }

    void rollback(const ulong log_idx, buffer& data) {
        // Nothing to do with rollback,
        // as this example doesn't do anything on pre-commit.
    }

    int read_logical_snp_obj(snapshot& s,
                         void*& user_snp_ctx,
                         ulong obj_id,
                         ptr<buffer>& data_out,
                         bool& is_last_obj)
    {
        ptr<snapshot_ctx> ctx = nullptr;
        {
            std::lock_guard<std::mutex> ll(snapshots_lock_);
            auto entry = snapshots_.find(s.get_last_log_idx());
            if (entry == snapshots_.end()) {
                data_out = nullptr;
                is_last_obj = true;
                return 0;
            }
            ctx = entry->second;
        }

        // Serialize the whole KeyValueStore into a string
        // (Consider splitting it if too large)
        std::string kv_store_serialized = serialize_kv_store(ctx->kv_store_); // Implement this method

        if (obj_id == 0) {
            // First object, return the serialized key-value store.
            data_out = buffer::alloc(kv_store_serialized.size());
            buffer_serializer bs(data_out);
            bs.put_raw(kv_store_serialized.data(), kv_store_serialized.size());
            is_last_obj = true;
        }
        return 0;
    }

    void save_logical_snp_obj(snapshot& s,
                          ulong& obj_id,
                          buffer& data,
                          bool is_first_obj,
                          bool is_last_obj)
{
    if (obj_id == 0) {
        // Object ID == 0: contains the serialized key-value store.
        buffer_serializer bs(data);
        std::string kv_store_serialized = bs.get_str(); // Deserialize the string

        std::lock_guard<std::mutex> ll(snapshots_lock_);
        auto entry = snapshots_.find(s.get_last_log_idx());

        entry->second->kv_store_ = deserialize_kv_store(kv_store_serialized); // Implement this method
    }
}

    bool apply_snapshot(snapshot& s) {
        std::lock_guard<std::mutex> ll(snapshots_lock_);
        auto entry = snapshots_.find(s.get_last_log_idx());
        if (entry == snapshots_.end()) return false;

        ptr<snapshot_ctx> ctx = entry->second;
        kv_store_ = ctx->kv_store_; // Restore the key-value store from the snapshot context.
        return true;
    }

    void free_user_snp_ctx(void*& user_snp_ctx) {
        // In this example, `read_logical_snp_obj` doesn't create
        // `user_snp_ctx`. Nothing to do in this function.
    }

    ptr<snapshot> last_snapshot() {
        // Just return the latest snapshot.
        std::lock_guard<std::mutex> ll(snapshots_lock_);
        auto entry = snapshots_.rbegin();
        if (entry == snapshots_.rend()) return nullptr;

        ptr<snapshot_ctx> ctx = entry->second;
        return ctx->snapshot_;
    }

    ulong last_commit_index() {
        return last_committed_idx_;
    }

    void create_snapshot(snapshot& s,
                         async_result<bool>::handler_type& when_done)
    {
        if (!async_snapshot_) {
            // Create a snapshot in a synchronous way (blocking the thread).
            create_snapshot_sync(s, when_done);
        } else {
            // Create a snapshot in an asynchronous way (in a different thread).
            create_snapshot_async(s, when_done);
        }
    }

    KeyValueStore get_kv_store() const { return kv_store_; }

    std::map<std::string, int> get_map_reduce_results(const ulong log_idx) {
        std::lock_guard<std::mutex> ll(snapshots_lock_);
        auto entry = map_reduce_results_.find(log_idx);
        if (entry == map_reduce_results_.end()) {
            return std::map<std::string, int>();
        }
        return entry->second;
    }

private:
    struct snapshot_ctx {
        snapshot_ctx(ptr<snapshot>& s, const KeyValueStore& kv_store)
            : snapshot_(s), kv_store_(kv_store) {}

        ptr<snapshot> snapshot_;
        KeyValueStore kv_store_;
    };

    std::string serialize_kv_store(const KeyValueStore& kv_store) {
        std::stringstream ss;
        for (const auto& kv : kv_store.getAll()) {
            ss << kv.first << ":";
            for (const auto& val : kv.second) {
                ss << val << ",";
            }
            ss << ";";  // End of the key-value pair
        }
        return ss.str();
    }

    KeyValueStore deserialize_kv_store(std::string& data) {
        KeyValueStore new_kv_store;
        std::istringstream ss(data);
        std::string key_value_pair;
        while (std::getline(ss, key_value_pair, ';')) {
            std::istringstream kv_stream(key_value_pair);
            std::string key;
            if (!std::getline(kv_stream, key, ':')) {
                continue; // No key found, skip
            }

            std::vector<int> values;
            std::string value_str;
            while (std::getline(kv_stream, value_str, ',')) {
                if (value_str.empty()) {
                    continue; // Skip empty strings
                }
                try {
                    int value = std::stoi(value_str);
                    values.push_back(value);
                } catch (const std::invalid_argument& e) {
                    // Handle or log the error
                    continue;
                }
            }
            // Store the key and values
            new_kv_store.insertMany(key, values);
        }
        return new_kv_store;
    }

    void create_snapshot_internal(ptr<snapshot> ss) {
        std::lock_guard<std::mutex> ll(snapshots_lock_);

        ptr<snapshot_ctx> ctx = cs_new<snapshot_ctx>(ss, kv_store_);
        snapshots_[ss->get_last_log_idx()] = ctx;

        // Maintain last 3 snapshots only.
        const int MAX_SNAPSHOTS = 3;
        int num = snapshots_.size();
        auto entry = snapshots_.begin();
        for (int ii = 0; ii < num - MAX_SNAPSHOTS; ++ii) {
            if (entry == snapshots_.end()) break;
            entry = snapshots_.erase(entry);
        }
    }

    void create_snapshot_sync(snapshot& s,
                              async_result<bool>::handler_type& when_done)
    {
        // Clone snapshot from `s`.
        ptr<buffer> snp_buf = s.serialize();
        ptr<snapshot> ss = snapshot::deserialize(*snp_buf);
        create_snapshot_internal(ss);

        ptr<std::exception> except(nullptr);
        bool ret = true;
        when_done(ret, except);

        std::cout << "snapshot (" << ss->get_last_log_term() << ", "
                  << ss->get_last_log_idx() << ") has been created synchronously"
                  << std::endl;
    }

    void create_snapshot_async(snapshot& s,
                               async_result<bool>::handler_type& when_done)
    {
        // Clone snapshot from `s`.
        ptr<buffer> snp_buf = s.serialize();
        ptr<snapshot> ss = snapshot::deserialize(*snp_buf);

        // Note that this is a very naive and inefficient example
        // that creates a new thread for each snapshot creation.
        std::thread t_hdl([this, ss, when_done]{
            create_snapshot_internal(ss);

            ptr<std::exception> except(nullptr);
            bool ret = true;
            when_done(ret, except);

            std::cout << "snapshot (" << ss->get_last_log_term() << ", "
                      << ss->get_last_log_idx() << ") has been created asynchronously"
                      << std::endl;
        });
        t_hdl.detach();
    }

    void add_map_reduce_result(const ulong log_idx, std::map<std::string, int>& results) {
        std::lock_guard<std::mutex> ll(snapshots_lock_);
        map_reduce_results_[log_idx] = results;
    }

    // Key-value store.
    KeyValueStore kv_store_;

    // MapReduce results (log_index : results, where result -> key : value)
    std::map< ulong, std::map<std::string, int> > map_reduce_results_;

    // Last committed Raft log number.
    std::atomic<uint64_t> last_committed_idx_;

    // Keeps the last 3 snapshots, by their Raft log numbers.
    std::map< uint64_t, ptr<snapshot_ctx> > snapshots_;

    // Mutex for `snapshots_`.
    std::mutex snapshots_lock_;

    // If `true`, snapshot will be created asynchronously.
    bool async_snapshot_;
};

}; // namespace mapreduce_server

