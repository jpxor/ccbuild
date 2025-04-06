
all: tests

tests: alloc_test trie_test

######### TESTS ###########

alloc_test:
	gcc -g -O0 test_cc_allocator.c -o alloc_test
	@alloc_test

trie_test:
	gcc -g -O0 test_cc_trie_map.c -o trie_test
	@trie_test


######### OTHER ###########

alloc_build:
	gcc -c -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 -DNDEBUG cc_allocator.c

trie_build:
	gcc -c -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 -DNDEBUG cc_trie_map.c
