codeql:
	gcc -I. \
	./vendor/inih/ini.c \
	./vendor/cwalk/cwalk.c \
	./libcc/cc_strings.c \
	./libcc/cc_allocator.c \
	./libcc/cc_trie_map.c \
	./libcc/cc_threadpool.c \
	./libcc/cc_files.c \
	./src/str_list.c \
	./src/build_opts.c \
	./src/cmd_build.c \
	./src/cmd_clean.c \
	./src/main.c -o cc
