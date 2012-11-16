#!/bin/bash
printf "Starting emulator on port 3000\n"                    \
printf "\tqueue size: 50, forwards: table1, log: log1\n\n"; \
./emulator -p 3000 -q 50 -f table1 -l log1

