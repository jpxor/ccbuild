
alloc_test:
	gcc -g -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 test_cc_allocator.c -o test
	@test

alloc_ndebug_build:
	gcc -c -g -Wall -Wextra -g -O2 -D_FORTIFY_SOURCE=2 -DNDEBUG cc_allocator.c
