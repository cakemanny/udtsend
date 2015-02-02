
UDTDIR="/Users/daniel/src/third-party/udt-git/udt4/src"
CXXFLAGS=-std=c++11 -g -O2 -Wall -I$(UDTDIR) -L$(UDTDIR)
LDLIBS=-ludt -lpthread -lc

all: filesender filereceiver

filesender: filesender.cpp udtsend.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDLIBS)

filereceiver: filereceiver.cpp udtsend.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f *.o filesender filereceiver
	rm -Rf filesender.dSYM filereceiver.dSYM

