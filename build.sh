echo "1. build zImage"
make -j3

echo "2. copy zImage to boot/kernel/<"
cp arch/arm/boot/zImage ~/android/boot/kernel/

echo "3. copy modules to boot/system/lib/modules<"
find ~/android/v500_cm -name "*.ko" -exec cp {} ~/android/boot/system/lib/modules/ \;

zipfile="mani.zip"
echo "4. build zipfile"
cd ~/android/boot/
rm -f *.zip
zip -9 -r $zipfile *
rm -f /tmp/*.zip
cp *.zip /tmp
