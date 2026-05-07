CXX      := g++
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Werror -pthread

test: test.cpp thread_pool.hpp
	$(CXX) $(CXXFLAGS) test.cpp -o test

run: test
	./test

clean:
	rm -f test

.PHONY: run clean
