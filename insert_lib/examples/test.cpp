#include <insert_lib.h>

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

using namespace std;
using namespace rocksdb;

// 全局统计信息
atomic<int> total_memtables_created(0);
atomic<int> total_entries_inserted(0);

// 存储每个线程的运行时间
struct ThreadResult {
    int thread_id;
    int memtables_created;
    int entries_inserted;
    long long duration_ms;
};
vector<ThreadResult> thread_results;
mutex results_mutex;

// 工作线程函数
void worker_thread(DBWrapper& db, 
                  ColumnFamilyData* cfd, 
                  int thread_id, 
                  int entries_per_memtable, 
                  int num_insertions,
                  bool random_key,
                  bool disable_actual_write) {
    
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
    
    // 线程本地统计
    int local_memtables_created = 0;
    int local_entries_inserted = 0;

    unsigned int seed = thread_id;
    
    for (int i = 0; i < num_insertions; i++) {
        // 创建新的memtable
        auto mem = db.newTable(cfd);
        if (!mem) {
            cout << "线程 " << thread_id << " 创建memtable失败" << endl;
            continue;
        }
        
        local_memtables_created++;
        total_memtables_created++;
        
        // 向memtable插入数据
        for (int j = 0; j < entries_per_memtable; j++) {
            int rand_prefix = rand_r(&seed);
            string key = "Prefix" + to_string(random_key ? rand_prefix : 0) + "_Thread" + to_string(thread_id) + "_Mem" + to_string(i) + "_Key" + to_string(j);
            string value = "Value" + to_string(j);
            
            auto status = mem->Add(j + 1, kTypeValue, key, Slice(value), nullptr, false);
            if (status.ok()) {
                local_entries_inserted++;
                total_entries_inserted++;
            } else {
                cout << "线程 " << thread_id << " 插入键值对失败: " << key << " -> " << value << endl;
            }
        }
        
        auto status = Status::OK();
        // 将memtable插入到数据库
        if (!disable_actual_write)
            status = db.insertMemTable(cfd, mem, 0);
        else
            delete mem;
        if (!status.ok()) {
            cout << "线程 " << thread_id << " 插入memtable失败: " << status.ToString() << endl;
        }
        
        // 输出进度
        if ((i + 1) % 10 == 0) {
            cout << "线程 " << thread_id << " 完成 " << (i + 1) << "/" << num_insertions << " 个memtable" << endl;
        }
    }
    
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
    cout << "用法: " << program_name << " <线程数> <每个memtable的entry数> <插入次数> <key分布是否随机> <是否禁用实际写入>" << endl;
    cout << "示例: " << program_name << " 4 1000 100" << endl;
    cout << "参数说明:" << endl;
    cout << "  线程数: 并发工作的线程数量" << endl;
    cout << "  每个memtable的entry数: 每个memtable中插入的键值对数量" << endl;
    cout << "  插入次数: 每个线程创建和插入memtable的次数" << endl;
    cout << "  是否禁用实际写入: 若禁用，则仅进行memtable构造" << endl;
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        print_usage(argv[0]);
        return 1;
    }
    
    // 解析命令行参数
    int num_threads = atoi(argv[1]);
    int entries_per_memtable = atoi(argv[2]);
    int num_insertions = atoi(argv[3]);
    bool random_key = atoi(argv[4]) != 0;
    bool disable_actual_write = atoi(argv[5]) != 0;
    
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
    
    cout << "=== 多线程Memtable构建测试 ===" << endl;
    cout << "线程数: " << num_threads << endl;
    cout << "每个memtable的entry数: " << entries_per_memtable << endl;
    cout << "插入次数: " << num_insertions << endl;
    cout << "总memtable数: " << (num_threads * num_insertions) << endl;
    cout << "总entry数: " << (num_threads * num_insertions * entries_per_memtable) << endl;
    cout << "================================" << endl;
    
    // 创建数据库
    string dpath = "/tmp/testdb_multithread";
    
    // 删除旧的数据库目录
    try {
        if (filesystem::exists(dpath)) {
            filesystem::remove_all(dpath);
            cout << "已删除旧的数据库目录: " << dpath << endl;
        }
    } catch (const filesystem::filesystem_error& e) {
        cout << "删除旧数据库目录失败: " << e.what() << endl;
        return 1;
    }
    
    rocksdb::Options options;
    options.create_if_missing = true;
    
    rocksdb::DB* db;
    rocksdb::Status status = rocksdb::DB::Open(options, dpath, &db);
        
    if (!status.ok()) {
        cout << "错误: 无法打开数据库: " << status.ToString() << endl;
        return 1;
    }
    
    // 创建列族
    rocksdb::ColumnFamilyHandle *cf_handle;
    auto s = db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), "test_cf", &cf_handle);
    if (!s.ok() && !s.IsInvalidArgument()) {
        cout << "错误: 无法创建列族: " << s.ToString() << endl;
        return 1;
    }

    assert(cf_handle != nullptr);
    
    // 创建DBWrapper
    DBWrapper db_wrapper(db);
    auto cfd = db_wrapper.getColumnFamilyData(GetColumnFamilyID(cf_handle));
    if (!cfd) {
        cout << "错误: 无法获取列族数据" << endl;
        return 1;
    }
    
    cout << "开始多线程测试..." << endl;
    auto start_time = chrono::high_resolution_clock::now();
    
    // 创建并启动工作线程
    vector<thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker_thread, ref(db_wrapper), cfd, i, entries_per_memtable, num_insertions, random_key, disable_actual_write);
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
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
    
    // 清理
    delete cf_handle;
    delete db;
    
    // 删除数据库目录
    try {
        if (filesystem::exists(dpath)) {
            filesystem::remove_all(dpath);
            cout << "数据库目录已删除: " << dpath << endl;
        }
    } catch (const filesystem::filesystem_error& e) {
        cout << "删除数据库目录失败: " << e.what() << endl;
    }
    
    return 0;
} 