DBPATH = /home/usera/rocksdb

demo: demo.cpp
	g++ -o demo demo.cpp -I$(DBPATH)/include -I$(DBPATH) -I.. -L$(DBPATH) -lrocksdb -lzstd -lbz2 -llz4 -lz -lsnappy

test: test.cpp
	g++ -o test test.cpp -I$(DBPATH)/include -I$(DBPATH) -I.. -L$(DBPATH) -lrocksdb -lzstd -lbz2 -llz4 -lz -lsnappy

baseline: baseline.cpp
	g++ -o baseline baseline.cpp -I$(DBPATH)/include -I$(DBPATH) -I.. -L$(DBPATH) -lrocksdb -lzstd -lbz2 -llz4 -lz -lsnappy

baseline_batch: baseline_batch.cpp
	g++ -o baseline_batch baseline_batch.cpp -I$(DBPATH)/include -I$(DBPATH) -I.. -L$(DBPATH) -lrocksdb -lzstd -lbz2 -llz4 -lz -lsnappy