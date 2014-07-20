echo "build zImage"
make -j3 -o3

echo "copy zImage to ~/android/boot/<"
cp arch/arm/boot/zImage ~/android/boot/

echo "copy modules to ~/android/boot/modules/<"
find ~/android/v500_cm -name "*.ko" -exec cp {} ~/android/boot/modules/ \;
