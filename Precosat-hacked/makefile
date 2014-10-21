CXX=g++
CXXFLAGS=-O3 -m64 -static -Wall -Wextra -DNDEBUG -DNLOGPRECO -DNSTATSPRECO -finline-limit=10000000 -fomit-frame-pointer -fno-exceptions
all: precosat
clean:
	rm -f precosat *.o makefile precocfg.hh 
	rm -f *.gcno *.gcda cscope.out gmon.out *.gcov
precosat: precosat.o precobnr.o
	$(CXX) $(CXXFLAGS) -o precosat precosat.o precobnr.o
precosat.o: precosat.hh precosat.cc makefile
	$(CXX) $(CXXFLAGS) -c precosat.cc
precobnr.o: precobnr.cc precobnr.hh precocfg.hh
	$(CXX) $(CXXFLAGS) -c precobnr.cc
precocfg.hh: makefile VERSION mkconfig precosat.hh precosat.cc
	rm -f $@; ./mkconfig > $@
