proj2: proj2.c
	rm -f proj2.out
	gcc -std=gnu99 -Wall -Wextra -pthread -Werror -pedantic -o proj2 proj2.c: