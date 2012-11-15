#!/bin/bash
printf "Starting sender on port 2000\n";                   \
printf "\trequester on port 4000, rate: 10, len: 100,\n"; \
printf "\temulator @ mumble-30:3000, i: 2, t: 100\n\n";    \
./sender -p 2000 -g 4000 -r 10 -q 1 -l 100 -f mumble-30 -h 3000 -i 2 -t 100
