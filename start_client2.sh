#!/bin/bash
# start_client2.sh
# Purpose: Print and execute the client command for Joel (Client 2)

CMD="./bin/client --user \"Joel\" --self-id 2 --peer-id 1 --listen 0.0.0.0:8002 --peer 10.0.0.5:8001 --server 10.0.0.13:7000"

# Print the command
echo "Executing: $CMD"

# Execute the command
eval $CMD

