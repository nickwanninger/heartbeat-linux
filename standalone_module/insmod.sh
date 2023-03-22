#!/usr/bin/env bash

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root"
   exit 1
fi

MOD=${1:-./mod.ko}

insmod $MOD \
    hb_error_entry=0x$(grep ' error_entry' /proc/kallsyms | cut -d' ' -f1) \
    hb_error_return=0x$(grep ' error_return' /proc/kallsyms | cut -d' ' -f1)

mknod /dev/heartbeat c 239 0
chmod 777 /dev/heartbeat
