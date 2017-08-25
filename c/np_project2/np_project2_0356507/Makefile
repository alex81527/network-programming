all:
	clang++ -std=c++11 -g -O3 concurrent_server.cpp semaph.c error.c -o concurrent_server
	clang++ -std=c++11 -g -O3 single_process_server.cpp -o single_process_server
clean:
	rm *_server
