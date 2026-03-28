CXX = g++
CXXFLAGS = -I. -Wall

all: test_v1 test_v2

test_v1: main.cpp Arduino.h PhysicsSim.h
	$(CXX) $(CXXFLAGS) -o test_v1 main.cpp

test_v2: main.cpp Arduino.h PhysicsSim.h
	$(CXX) $(CXXFLAGS) -DUSE_ESTIMATOR_2 -o test_v2 main.cpp

clean:
	rm -f test_v1 test_v2
