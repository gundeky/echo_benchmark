#!/bin/bash

valgrind -v --tool=memcheck --leak-check=yes --trace-children=yes --log-file=a.log --show-reachable=yes ./test
