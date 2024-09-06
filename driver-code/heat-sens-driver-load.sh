#!/bin/sh
module=heat_sens_dev 
device=heat_sens_driver
mode="664"

if lsmod | grep "$module" &> /dev/null ; then
    echo "$MODULE_NAME module is already loaded."
else 
    echo "Loading local built file ${module}.ko"
    insmod ./$module.ko $* || exit 1
fi

major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)
rm -f /dev/${device}
mknod /dev/${device} c $major 0
chgrp $group /dev/${device}
chmod $mode  /dev/${device}