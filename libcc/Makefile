
all: build_all

tests: test_strings test_alloc test_trie test_threadpool

build_all:
	gcc -c -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 -DNDEBUG cc_allocator.c
	gcc -c -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 -DNDEBUG cc_trie_map.c
	gcc -c -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 -DNDEBUG cc_strings.c
	gcc -c -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 -DNDEBUG cc_files.c
	gcc -c -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 -DNDEBUG cc_threadpool.c

test_strings:
	gcc -g -O0 -DNDEBUG test_cc_strings.c -o test_strings
	@test_strings

test_alloc:
	gcc -g -O0 test_cc_allocator.c -o test_alloc
	@test_alloc

test_trie:
	gcc -g -O0 test_cc_trie_map.c -o test_trie
	@test_trie

test_threadpool:
	gcc -g -O0 test_cc_threadpool.c -o test_threadpool
	@test_threadpool

