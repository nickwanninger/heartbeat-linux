insmod /etc/mod.ko \
    hb_error_entry=0x$(grep ' error_entry' /proc/kallsyms | cut -d' ' -f1) \
    hb_error_return=0x$(grep ' error_return' /proc/kallsyms | cut -d' ' -f1)

mknod /dev/heartbeat c 247 0

