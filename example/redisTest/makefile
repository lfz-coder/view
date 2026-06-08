main: test.cc ../../source/viewRedis.cc ../../source/viewLog.cc
	g++ -o $@ $^ -I../../source -lredis++ -lhiredis -lpthread -lspdlog -lfmt -lgflags

.PHONY: clean
clean:
	rm -f main