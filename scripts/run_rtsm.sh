#!/bin/sh
FVP_VE_Cortex-A15x1 -C motherboard.smsc_91c111.enabled=1 -C motherboard.hostbridge.userNetworking=1 -a cluster.cpu0=../../build/hvc-man-switch.axf
