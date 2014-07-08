echo "build zImage"
make -j3 -o3

cp arch/arm/boot/zImage ~/android/boot/

cd ~/android/boot/

echo "extract ramdisk"
unmkbootimg -i boot.img

rm -rf kernel
rm -rf boot.img

echo "make boot.img"
mkbootimg --base 0 --pagesize 2048 --kernel_offset 0x80208000 --ramdisk_offset 0x88f10810 --second_offset 0x81100000 --tags_offset 0x80200100 --cmdline 'console=ttyHSL0,115200,n8 user_debug=31 msm_rtb.filter=0x3F ehci-hcd.park=3 lpj=67677 androidboot.hardware=awifi vmalloc=600M' --kernel zImage --ramdisk ramdisk.cpio.gz -o boot.img

rm -rf zImage
rm -rf ramdisk.cpio.gz
