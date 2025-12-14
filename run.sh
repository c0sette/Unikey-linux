#!/bin/bash
# Start UniKey with proper permissions

if [ "$EUID" -ne 0 ]; then
    if ! groups | grep -q input; then
        echo "You need to be in the 'input' group or run as root"
        echo "Run: sudo usermod -aG input $USER"
        echo "Then log out and back in"
        exit 1
    fi
fi

exec ./unikey "$@"
