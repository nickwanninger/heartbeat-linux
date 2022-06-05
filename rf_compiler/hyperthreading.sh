#!/usr/bin/env bash

FLG=$1

if [[ $FLG == "" ]] 
then
    echo "usage:"
    echo "hyperthreading.sh on           to enable"
    echo "hyperthreading.sh off          to disable"
    exit 1
fi

cmd="echo $FLG > /sys/devices/system/cpu/smt/control"
sudo sh -c "$cmd"

