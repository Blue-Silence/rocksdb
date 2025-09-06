DBPATH = /home/usera/FASTER/cc

helloworld: helloworld.cpp
	g++ -o helloworld helloworld.cpp -I$(DBPATH)/src -I$(DBPATH) -I.. -L$(DBPATH)/build/Release -lfaster -luuid -ltbb -laio

baseline_faster: baseline_faster.cpp
	g++ -o baseline_faster baseline_faster.cpp -I$(DBPATH)/src -I$(DBPATH) -I.. -L$(DBPATH)/build/Release -lfaster -luuid -ltbb -laio
