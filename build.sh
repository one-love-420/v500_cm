echo "1. build zImage"
make -j3

rm -f zip/kernel/zImage
echo "2. copy zImage to zip/kernel/<"
cp arch/arm/boot/zImage zip/kernel/

echo "3. copy modules to zip/system/lib/modules<"
find ~/android/v500_cm \( ! -name ~/android/v500_cm/zip \) -name "*.ko" -exec cp {} zip/system/lib/modules/ \;

zipfile="mani.zip"
echo "4. build .zip"
cd zip/
rm -f *.zip
zip -9 -r $zipfile *
rm -f /tmp/*.zip
cp *.zip /tmp
