#!/bin/bash
# start_client1.sh
# Purpose: Print and execute the client command for Lucy (Client 1)

CMD="./bin/client --user Lucy --self-id 1 --peer-id 2 --listen 0.0.0.0:8001 --peer 172.31.27.84:8002 --server 172.31.23.9:7000"

# Print the command
echo "Executing: $CMD"

# Execute the command
$CMD

