#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# Default serial port
PORT="/dev/ttyACM0"

# Function to display usage/help
show_help() {
    echo "Usage: $0 [b] [f] [m]"
    echo "  b : Build (idf.py build)"
    echo "  f : Flash (idf.py -p $PORT flash)"
    echo "  m : Monitor (idf.py -p $PORT monitor)"
    echo ""
    echo "If no arguments are provided, all steps (b f m) are executed in order."
    echo "Arguments can be stacked in any order (e.g., $0 b f m, $0 f m)."
}

# Flags to track requested actions
DO_BUILD=false
DO_FLASH=false
DO_MONITOR=false

# If no arguments passed, run all commands
if [ $# -eq 0 ]; then
    DO_BUILD=true
    DO_FLASH=true
    DO_MONITOR=true
else
    # Parse command line arguments
    for arg in "$@"; do
        case "$arg" in
            b)
                DO_BUILD=true
                ;;
            f)
                DO_FLASH=true
                ;;
            m)
                DO_MONITOR=true
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                echo "Error: Unknown argument '$arg'"
                show_help
                exit 1
                ;;
        esac
    done
fi

# Execute actions in the proper sequence
if [ "$DO_BUILD" = true ]; then
    echo "==> Building project..."
    idf.py build
fi

if [ "$DO_FLASH" = true ]; then
    echo "==> Flashing device on $PORT..."
    idf.py -p "$PORT" flash
fi

if [ "$DO_MONITOR" = true ]; then
    echo "==> Starting monitor on $PORT..."
    idf.py -p "$PORT" monitor
fi