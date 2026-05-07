CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -g -fsanitize=address,undefined

test: test.c arena.c arena.h
	$(CC) $(CFLAGS) test.c arena.c -o test

run: test
	./test

clean:
	rm -f test

.PHONY: run clean
