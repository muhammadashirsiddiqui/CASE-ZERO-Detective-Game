#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
if ! pkg-config --exists gtk+-3.0 gio-2.0; then
 echo 'GTK3 development files missing. On Ubuntu/Debian run:'
 echo 'sudo apt update && sudo apt install build-essential libgtk-3-dev'
 exit 1
fi
gcc -std=c11 -O2 -DCASE_GUI case_zero.c -o case_zero_engine
gcc -std=c11 -O2 -Wall -Wextra case_zero_gui.c -o case_zero_gui $(pkg-config --cflags --libs gtk+-3.0 gio-2.0)
./case_zero_gui
