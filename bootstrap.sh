#!/bin/bash

mkdir -p ./install/bootstrap

gcc -O2 -I. \
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
	./src/main.c -o ./install/bootstrap/cc

export $PATH=$PATH:${PWD}/install/bootstrap

# optional: install to /usr/local/bin
# ln -s ./install/bootstrap/cc /usr/local/bin/cc
