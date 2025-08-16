#include <insert_lib.h>

#include <iostream>
#include <string>

using namespace std;
using namespace rocksdb;


void imm_install_test(rocksdb::DB* db,
                      const std::vector<rocksdb::ColumnFamilyHandle*>& handles);

int main() {
  string dpath = "/tmp/testdb";
  cout << "Foo\n";

  rocksdb::Options options;
  options.create_if_missing = true;

  rocksdb::DB* db;
  std::vector<rocksdb::ColumnFamilyHandle*> handles;
  rocksdb::Status status = rocksdb::DB::Open(options, dpath, &db);
  if (!status.ok()) {
    std::vector<std::string> cf_names;
    rocksdb::Status s00000 =
        rocksdb::DB::ListColumnFamilies(rocksdb::DBOptions(), dpath, &cf_names);

    cout << "Foo1\n";

    std::vector<rocksdb::ColumnFamilyDescriptor> column_families;
    for (const auto& name : cf_names) {
      column_families.push_back(rocksdb::ColumnFamilyDescriptor(
          name, rocksdb::ColumnFamilyOptions()));
    }

    status = rocksdb::DB::Open(rocksdb::DBOptions(), dpath, column_families,
                               &handles, &db);
  }

  assert(status.ok());

  rocksdb::ColumnFamilyHandle *cl_1, *cl_2;
  auto s3 =
      db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), "cl_1", &cl_1);
  auto s4 =
      db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), "cl_2", &cl_2);
  cout << s3.ok() << s4.ok() << "\n";

  if (!cl_1)
    cl_1 = handles[1];
  else {
    handles.push_back(nullptr);
    handles.push_back(cl_1);
  }

  if (!cl_2) cl_2 = handles[2];
  rocksdb::DBImpl* a = static_cast<rocksdb::DBImpl*>(db);

  ////////////////////////////////////////////////////////////////////////////////////////////

  std::string value;
  string key1 = "123";
  string key2 = "456";

  /////////////////////////////////////////////////////////////////////
  imm_install_test(db, handles);

  return 0;
}

void imm_install_test(
    rocksdb::DB* db_p,
    const std::vector<rocksdb::ColumnFamilyHandle*>& handles) {
  cout << "\n\n\nIMM INSTALL TEST START\n\n\n";
  DBWrapper db(db_p);

  rocksdb::Options options;
  string key0 = "FFFKey0";
  string key1 = "FFFKey1";

  auto cfd = db.getColumnFamilyData(1);
  auto mem1 = cfd->ConstructNewMemtable(cfd->GetLatestMutableCFOptions(), 0);
  cout << "Our new mem table addr is: " << mem1 << "\n";
  // mem1->Ref();

  {
    string value0, value1;
    auto s0 = db.getDB()->Get(rocksdb::ReadOptions(), handles[1], key0, &value0);
    auto s1 = db.getDB()->Get(rocksdb::ReadOptions(), handles[1], key1, &value1);
    cout << "Get result (init): \n"
         << (int)s0.code() << " || " << (int)s1.code() << " || " << value0
         << " || " << value1 << "\n";
  }

  {
    Status s0, s1;
    s0 = mem1->Add(1, rocksdb::kTypeValue, key0, rocksdb::Slice("FFFVal0"),
                   nullptr, false);
    s1 = mem1->Add(2, rocksdb::kTypeValue, key1, rocksdb::Slice("FFFVal1"),
                   nullptr, false);
    cout << "Mem construct result: " << (int)s0.code() << " || "
         << (int)s1.code() << "\n";
  }

  {
    MergeContext merge_context;
    InternalKeyComparator ikey_cmp(options.comparator);
    SequenceNumber max_covering_tombstone_seq = 0;
    autovector<ReadOnlyMemTable*> to_delete;
    string value = "NO THINGS";
    Status s;
    SequenceNumber seq = 0;

    auto found = mem1->Get(
        LookupKey(key1, 1024000000), &value, /*columns*/ nullptr,
        /*timestamp*/ nullptr, &s, &merge_context, &max_covering_tombstone_seq,
        &seq, ReadOptions(), false /* immutable_memtable */);
    cout << "Check mem:" << found << "  ||  " << value << "  ||  " << seq
         << "\n";
  }

  //db->versions_->SetLastSequence(123450);

  {
    cout << "Check superversion (before): ";
    cout << cfd->GetSuperVersionNumber() << "\n";

    MergeContext merge_context;
    InternalKeyComparator ikey_cmp(options.comparator);
    SequenceNumber max_covering_tombstone_seq = 0;
    autovector<ReadOnlyMemTable*> to_delete;
    string value = "NO THINGS";
    Status s;
    SequenceNumber seq = 0;

    auto found = cfd->GetSuperVersion()->imm->Get(
        LookupKey(key1, 10241024000000), &value, /*columns*/ nullptr,
        /*timestamp*/ nullptr, &s, &merge_context, &max_covering_tombstone_seq,
        &seq, ReadOptions());
    cout << "imm:" << found << "  ||  " << value << "  ||  " << seq << "\n";
  }

  auto old_super = cfd->GetSuperVersion();
  db.getDB()->Put(rocksdb::WriteOptions(), key1, "Okk0000");
  {
    // auto s0 = db->InsertIMM2(cfd, mem1,2048000000);
    auto s0 = db.insertMemTable(cfd, mem1, 123450);
    cout << "IMM insert result: " << (int)s0.code() << "\n";
  }
  // db->Put(rocksdb::WriteOptions(), key1, "Okk0000");

  {
    cout << "Check superversion (after): ";
    cout << cfd->GetSuperVersionNumber() << "\n";

    MergeContext merge_context;
    InternalKeyComparator ikey_cmp(options.comparator);
    SequenceNumber max_covering_tombstone_seq = 0;
    autovector<ReadOnlyMemTable*> to_delete;
    string value = "NO THINGS";
    Status s;
    SequenceNumber seq = 0;

    auto found = cfd->GetSuperVersion()->imm->Get(
        LookupKey(key1, 10241024000000), &value, /*columns*/ nullptr,
        /*timestamp*/ nullptr, &s, &merge_context, &max_covering_tombstone_seq,
        &seq, ReadOptions());
    cout << "imm:" << found << "  ||  " << value << "  ||  " << seq << "\n";
  }

  {
    MergeContext merge_context;
    InternalKeyComparator ikey_cmp(options.comparator);
    SequenceNumber max_covering_tombstone_seq = 0;
    autovector<ReadOnlyMemTable*> to_delete;
    string value = "NO THINGS";
    Status s;
    SequenceNumber seq = 0;

    auto found = cfd->imm()->current()->Get(
        LookupKey(key1, 10241024000000), &value, /*columns*/ nullptr,
        /*timestamp*/ nullptr, &s, &merge_context, &max_covering_tombstone_seq,
        &seq, ReadOptions());
    cout << "Check imm:" << found << "  ||  " << value << "  ||  " << seq
         << "\n";
  }

  {
    string value0, value1;
    auto s0 = db.getDB()->Get(rocksdb::ReadOptions(), handles[1], key0, &value0);
    auto s1 = db.getDB()->Get(rocksdb::ReadOptions(), handles[1], key1, &value1);
    cout << "Get result: \n"
         << (int)s0.code() << " || " << (int)s1.code() << " || " << value0
         << " || " << value1 << "\n";
  }

  db.getDB()->Put(rocksdb::WriteOptions(), key1, "Okk0000");
}
