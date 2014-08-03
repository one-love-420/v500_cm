echo "1. build zImage"
make -j3

rm -f zip/kernel/zImage
echo "2. copy zImage to zip/kernel/<"
cp arch/arm/boot/zImage zip/kernel/

echo "3. copy modules to zip/system/lib/modules<"
rm -r zip/system/lib/modules
mkdir zip/system/lib/modules
find -type d \( ! -name zip-type d \( ! -name tmp \) \) -name "*.ko" -exec cp {} zip/system/lib/modules/ \;

zipfile="mani.zip"
echo "4. build .zip"
cd zip/
rm -f *.zip
zip -9 -r $zipfile *
rm -f /tmp/*.zip
cp *.zip /tmp
