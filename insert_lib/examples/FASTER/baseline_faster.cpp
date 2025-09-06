
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sched.h>
#include <unistd.h>

#include "core/faster.h"
#include "test_types.h"
#include "context_type.h"


using namespace FASTER::core;

// Define key and value types
typedef FASTER::test::FixedSizeKey<size_t> Key;
typedef FASTER::test::SimpleAtomicValue<int> Value;

typedef FASTER::environment::QueueIoHandler handler_t;
typedef FASTER::device::FileSystemDisk<handler_t, 1073741824ull> disk_t;

using namespace std;

// 全局统计信息
atomic<int> total_memtables_created(0);
atomic<int> total_entries_inserted(0);
atomic<int> seq(0);

// Callback for async operations
auto callback = [](IAsyncContext* ctxt, Status result) {
    if (result != Status::Ok) {
        std::cerr << "Error in async operation!" << std::endl;
    }
};

// 存储每个线程的运行时间
struct ThreadResult {
    int thread_id;
    int memtables_created;
    int entries_inserted;
    long long duration_ms;
};
vector<ThreadResult> thread_results;
mutex results_mutex;

// Create FasterKv instance
// 工作线程函数
void worker_thread(FasterKv<Key, Value, disk_t>& store,
                  int thread_id, 
                  int entries_per_memtable, 
                  int num_insertions) {

    // 将线程绑定到指定的CPU核心
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_id, &cpuset);
    
    int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (ret != 0) {
        cout << "警告: 线程 " << thread_id << " 绑定到CPU核心 " << thread_id << " 失败" << endl;
    } else {
        cout << "线程 " << thread_id << " 已绑定到CPU核心 " << thread_id << endl;
    }

    // 记录线程开始时间
    auto thread_start_time = chrono::high_resolution_clock::now();
    auto session_id = store.StartSession();
    
    // 线程本地统计
    int local_memtables_created = 0;
    int local_entries_inserted = 0;
    
    for (int i = 0; i < num_insertions; i++) {        
        local_memtables_created++;
        total_memtables_created++;
        
        // 向memtable插入数据
        for (int j = 0; j < entries_per_memtable; j++) {
            //string key = "Thread" + to_string(thread_id) + "_Mem" + to_string(i) + "_Key" + to_string(j);
            //string value = "Value" + to_string(j);
            
            auto seq_n = seq.fetch_add(1, memory_order_release);
            //UpsertContext upsert_context{ Key{thread_id * seq_n}, Value{thread_id * seq_n} };
            UpsertContext upsert_context{ Key{0}, Value{thread_id * seq_n} };
            auto status = store.Upsert(upsert_context, callback, seq_n);
            if (status != Status::Ok) {
                cout << "线程 " << thread_id << " 插入KV失败: " << StatusStr(status) << endl;
            } 
            
            total_entries_inserted++;
            //store.CompletePending(true);
        }
        
        
        // 输出进度
        if ((i + 1) % 10 == 0) {
            cout << "线程 " << thread_id << " 完成 " << (i + 1) << "/" << num_insertions << " 个iter" << endl;
        }
    }

    store.CompletePending(true);
    store.StopSession();
    
    // 计算线程运行时间
    auto thread_end_time = chrono::high_resolution_clock::now();
    auto thread_duration = chrono::duration_cast<chrono::milliseconds>(thread_end_time - thread_start_time);
    
    // 保存线程结果
    ThreadResult result;
    result.thread_id = thread_id;
    result.memtables_created = local_memtables_created;
    result.entries_inserted = local_entries_inserted;
    result.duration_ms = thread_duration.count();
    
    {
        lock_guard<mutex> lock(results_mutex);
        thread_results.push_back(result);
    }
    
    cout << "线程 " << thread_id << " 完成所有工作" << endl;
}

void print_usage(const char* program_name) {
    cout << "用法: " << program_name << " <线程数> <每个iter的entry数> <插入次数>" << endl;
    cout << "示例: " << program_name << " 4 1000 100" << endl;
    cout << "参数说明:" << endl;
    cout << "  线程数: 并发工作的线程数量" << endl;
    cout << "  每个iter的entry数: 每个iter中插入的键值对数量" << endl;
    cout << "  插入次数: 每个线程创建和插入iter的次数" << endl;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }
    
    // 解析命令行参数
    int num_threads = atoi(argv[1]);
    int entries_per_memtable = atoi(argv[2]);
    int num_insertions = atoi(argv[3]);
    
    if (num_threads <= 0 || entries_per_memtable <= 0 || num_insertions <= 0) {
        cout << "错误: 所有参数必须为正整数" << endl;
        print_usage(argv[0]);
        return 1;
    }
    
    // 检查线程数是否超过可用CPU核心数
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    cout << "总可用GPU数：" << num_cpus << "\n";
    if (num_threads > num_cpus) {
        cout << "警告: 线程数(" << num_threads << ") 超过可用CPU核心数(" << num_cpus << ")" << endl;
        cout << "建议将线程数设置为 " << num_cpus << " 或更少以获得最佳性能" << endl;
    }

    cout << "=== 多线程Memtable构建测试(baseline) ===" << endl;
    cout << "线程数: " << num_threads << endl;
    cout << "每个memtable的entry数: " << entries_per_memtable << endl;
    cout << "插入次数: " << num_insertions << endl;
    cout << "总memtable数: " << (num_threads * num_insertions) << endl;
    cout << "总entry数: " << (num_threads * num_insertions * entries_per_memtable) << endl;
    cout << "================================" << endl;
    
    // Initialize FasterKv setting
    uint64_t hash_table_size = 1 << 20;    // 1 M hash table entries
    uint64_t log_size = 4 * ((size_t)1 << 30);     // 4 GiB log size
    const char* log_path = "/dev/shm/foo_faster";
    double mutable_fraction = 0.9;         // 90% mutable region

    
    // 删除旧的数据库目录
    try {
        if (filesystem::exists(log_path)) {
            filesystem::remove_all(log_path);
            cout << "已删除旧的数据库目录: " << log_path << endl;
        }
    } catch (const filesystem::filesystem_error& e) {
        cout << "删除旧数据库目录失败: " << e.what() << endl;
        return 1;
    }
    
    
    // 创建DB
    FasterKv<Key, Value, disk_t> store{
        hash_table_size, log_size, log_path, mutable_fraction
    };
    
    cout << "开始多线程测试..." << endl;
    auto start_time = chrono::high_resolution_clock::now();
    auto session_id = store.StartSession();
    
    // 创建并启动工作线程
    vector<thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker_thread, ref(store), i, entries_per_memtable, num_insertions);
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    store.StopSession();

    
    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    
    cout << "\n=== 测试完成 ===" << endl;
    cout << "总耗时: " << duration.count() << " 毫秒" << endl;
    cout << "实际创建的memtable数: " << total_memtables_created.load() << endl;
    cout << "实际插入的entry数: " << total_entries_inserted.load() << endl;
    cout << "预期memtable数: " << (num_threads * num_insertions) << endl;
    cout << "预期entry数: " << (num_threads * num_insertions * entries_per_memtable) << endl;
    
    // 输出脚本可处理的数据行
    cout << "DATA:";
    cout << " " << num_threads;
    cout <<" " << entries_per_memtable;
    cout <<" " << total_entries_inserted.load();
    for (const auto& result : thread_results) {
        cout << " " << result.duration_ms;
    }
    cout << endl;
    
    
    // 删除数据库目录
    try {
        if (filesystem::exists(log_path)) {
            filesystem::remove_all(log_path);
            cout << "数据库目录已删除: " << log_path << endl;
        }
    } catch (const filesystem::filesystem_error& e) {
        cout << "删除数据库目录失败: " << e.what() << endl;
    }
    
    return 0;
} 