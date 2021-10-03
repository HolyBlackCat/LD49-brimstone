set -e

rm -rf win_bin
mkdir win_bin
cp -r bin win_bin/brimstone
find win_bin -name '*.so*' -exec rm -rf {} \;
rm -rf win_bin/brimstone/assets/_* win_bin/brimstone/*.txt win_bin/brimstone/brimstone
make HOST_OS=linux OBJECT_DIR=win_obj LIBRARY_PACK_DIR=../_win_dependencies OUTPUT_FILE=win_bin/brimstone/brimstone mode=release -j12

filename="brimstone-windows-$(bash -c "echo $(cat version.txt)").zip"
rm -f "$filename"

(cd win_bin && zip -r ../"$filename" brimstone)
