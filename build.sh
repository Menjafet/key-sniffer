#!/bin/sh

set -xe
Clang -Wall -Wextra -fsanitize=address  -g  -o reader   main.c 


