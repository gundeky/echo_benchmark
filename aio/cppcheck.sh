#!/bin/bash

cppcheck -iunix/backup --enable=all --suppress=missingIncludeSystem .
