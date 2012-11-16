#!/bin/bash
printf "Starting requester on port 4000\n";                            \
printf "\temulator @ mumble-30:3000, file: file.txt, window: 10\n\n"; \
./requester -p 4000 -f mumble-30 -h 3000 -o file.txt -w 10

