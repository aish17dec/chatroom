#!/bin/bash
# run_server.sh
# Purpose: Print and execute the server command.

CMD="./bin/server --bind 0.0.0.0:7000 --file ./chat.txt"

# Print the command being executed
echo "Executing: $CMD"

# Execute the command
$CMD

