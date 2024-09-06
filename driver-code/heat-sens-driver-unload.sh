#!bin/sh
module=heat_sens_dev 
device=heat_sens_driver

rmmod $module || exit 1

rm -f /dev/${device}