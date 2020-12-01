#!/bin/sh

# Documentation/admin-guide/pm/cpufreq.rst

[ -z "$1" ] && { echo "governor expected" >&2; exit 1; }

for p in /sys/devices/system/cpu/cpufreq/policy*
do
	echo "$1" > $p/scaling_governor
done
