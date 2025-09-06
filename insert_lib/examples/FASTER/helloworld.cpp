#include <iostream>
#include <string>
#include "core/faster.h"
#include "test_types.h"
#include "context_type.h"


using namespace FASTER::core;

// Define key and value types
typedef FASTER::test::FixedSizeKey<size_t> Key;
typedef FASTER::test::SimpleAtomicValue<int> Value;

typedef FASTER::environment::QueueIoHandler handler_t;
typedef FASTER::device::FileSystemDisk<handler_t, 1073741824ull> disk_t;

// Initialize FasterKv setting
uint64_t hash_table_size = 1 << 20;    // 1 M hash table entries
uint64_t log_size = 4 * ((size_t)1 << 30);     // 4 GiB log size
const char* log_path = "/dev/shm/foo_faster";
double mutable_fraction = 0.9;         // 90% mutable region

// Create FasterKv instance
FasterKv<Key, Value, disk_t> store{
    hash_table_size, log_size, log_path, mutable_fraction
};

// Callback for async operations
auto callback = [](IAsyncContext* ctxt, Status result) {
    if (result != Status::Ok) {
        std::cerr << "Error in async operation!" << std::endl;
    }
};

int main() {

    // Start a session
    auto session_id = store.StartSession();

    // Perform operations
    {
        // Upsert
        UpsertContext upsert_context{ Key{1}, Value{100} };
        store.Upsert(upsert_context, callback, 1);

        // Read
        ReadContext<Key, Value> read_context{ Key{1} };
        Status result = store.Read(read_context, callback, 2);
        if (result == Status::Ok) {
            std::cout << "Read value: " << read_context.output.value << std::endl;
        }

        // RMW (increment by 50)
        RmwContext rmw_context{ Key{1}, Value{50} };
        store.Rmw(rmw_context, callback, 3);

        // Complete any pending operations
        store.CompletePending(true);

        // Read updated value
        ReadContext<Key, Value> read_context2{ Key{1} };
        result = store.Read(read_context2, callback, 4);
        if (result == Status::Ok) {
            std::cout << "Updated value: " << read_context2.output.value << std::endl;
        }
    }

    // Stop session
    store.StopSession();

    return 0;
}