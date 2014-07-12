echo "build zImage"
make -j3 -o3

echo "collect modules"
find ~/android/v500_cm -name "*.ko" -exec cp {} ~/android/boot/kernelmodul/ \;
