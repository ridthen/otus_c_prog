#!/bin/bash

valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
	 --verbose --log-file=valgrind-out.txt \
	 cmake-build-debug/wordFreq README.MD

less valgrind-out.txt 
