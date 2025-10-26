#!/bin/bash
# start_client2.sh
# Purpose: Print and execute the client command for Joel (Client 2)

CMD="./bin/client --user \"Joel\" --self-id 2 --peer-id 1 --listen 0.0.0.0:8002 --peer 172.31.22.222:8001 --server 172.31.23.9:7000"

# Print the command
echo "Executing: $CMD"

# Execute the command
eval $CMD

