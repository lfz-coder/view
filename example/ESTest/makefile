main: test.cc ../../source/viewElastic.cc ../../source/viewLog.cc ../../source/viewUtil.cc
	g++ -o $@ $^ -I../../source -lpthread -lspdlog -lfmt -lgflags -lcpr -ljsoncpp -lelasticlient

.PHONY: clean
clean:
	rm -f main