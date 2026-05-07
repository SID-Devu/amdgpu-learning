CXX      := g++
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Werror -pthread

test: test.cpp spsc.hpp
	$(CXX) $(CXXFLAGS) test.cpp -o test

run: test
	./test

tsan: test.cpp spsc.hpp
	$(CXX) $(CXXFLAGS) -fsanitize=thread test.cpp -o tsan_test
	./tsan_test

clean:
	rm -f test tsan_test

.PHONY: run tsan clean
