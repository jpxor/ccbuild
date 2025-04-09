
all: build_all

build_all:
# tests
	gcc -g -O0 test_cc_allocator.c -o alloc_test
	gcc -g -O0 test_cc_trie_map.c -o trie_test
	gcc -g -O0 -DNDEBUG test_cc_strings.c -o string_test
# compilation units
	gcc -c -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 -DNDEBUG cc_allocator.c
	gcc -c -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 -DNDEBUG cc_trie_map.c
	gcc -c -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 -DNDEBUG cc_strings.c

run_tests:
	@alloc_test
	@trie_test
	@string_test
