
CXXFLAGS=-std=c++11 -g -O2 -Wall -L.
LDLIBS=-ludt

all: filesender filereceiver

filesender: filesender.cpp udtsend.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDLIBS)

filereceiver: filereceiver.cpp udtsend.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f *.o filesender filereceiver
	rm -Rf filesender.dSYM filereceiver.dSYM

