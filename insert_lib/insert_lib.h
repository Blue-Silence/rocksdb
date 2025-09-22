#pragma once

#ifndef ROCKSDB_PLATFORM_POSIX
#define ROCKSDB_PLATFORM_POSIX
#endif

#include <db/db_impl/db_impl.h>
#include <db/merge_context.h>
#include <db/version_set.h>
#include <monitoring/instrumented_mutex.h>
#include <rocksdb/db.h>
// #include <rocksdb/column_family>
#include <pthread.h>
#include <rocksdb/memtablerep.h>

#include <iostream>
#include <string>

using rocksdb::GetColumnFamilyID;

// uint32_t GetColumnFamilyID(rocksdb::ColumnFamilyHandle* column_family);

class DBWrapper {
 public:
  DBWrapper(rocksdb::DB* db) { dbimpl = static_cast<rocksdb::DBImpl*>(db); };

  rocksdb::DB * getDB() {return static_cast<rocksdb::DB*>(dbimpl);}

  rocksdb::ColumnFamilySet* getColumnFamilySet() {
    return dbimpl->GetVersionSet()->GetColumnFamilySet();
  };

  rocksdb::ColumnFamilyData* getColumnFamilyData(uint32_t id) {
    return getColumnFamilySet()->GetColumnFamily(id);
  };

  rocksdb::MemTable* newTable(rocksdb::ColumnFamilyData* cfd) {
    //rocksdb::ColumnFamilyData* cfd = getColumnFamilyData(cfd_id);
    dbimpl->lock_db();
    auto new_mem =
        cfd->ConstructNewMemtable(cfd->GetLatestMutableCFOptions(), 0);
    dbimpl->unlock_db();
    return new_mem;
  };

  rocksdb::Status insertMemTable(rocksdb::ColumnFamilyData* cfd,
                                 rocksdb::MemTable* new_mem,
                                 rocksdb::SequenceNumber last_seq = 0) {
    auto s = dbimpl->InsertIMM2(cfd, new_mem, 0);
    if (last_seq !=0) {
        dbimpl->GetVersionSetNoInline()->SetLastSequence(last_seq);
    }
    return s;
  }

  void setLastSequence(rocksdb::SequenceNumber last_seq) {
    dbimpl->GetVersionSetNoInline()->SetLastSequence(last_seq);
  }

 private:
  rocksdb::DBImpl* dbimpl;
};
