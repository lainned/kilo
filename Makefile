kilo: kilo.c
	clang kilo.c -o kilo -Wall -Wextra -pedantic -fsanitize=address -std=c99

	