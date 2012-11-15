#!/bin/bash
printf "Starting emulator on port 3000\n"                    \
printf "\tqueue size: 100, forwards: table1, log: log1\n\n"; \
./emulator -p 3000 -q 100 -f table1 -l log1

