#!/sbin/sh
#

# Run loki patch on boot images with certain bootloaders
#



cd /tmp
dd if=/dev/block/platform/msm_sdcc.1/by-name/aboot of=aboot.img
#dd if=/dev/block/platform/msm_sdcc.1/by-name/boot of=boot.img
./loki_tool patch boot aboot.img newboot.img out.lok
./loki_tool flash boot /tmp/out.lok


# cleanup


