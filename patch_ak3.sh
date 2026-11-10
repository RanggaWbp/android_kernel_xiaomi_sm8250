#!/bin/bash
# Replace anykernel.sh with the exact AstideLabs content (known-good recovery flash)
# usage: patch_ak3.sh /path/to/anykernel.sh
set -e
f="$1"
cat > "$f" <<'AKEOF'
### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers

### AnyKernel setup
# global properties
properties() { '
kernel.string=TanpaNama by RanggaWbp
do.devicecheck=0
do.modules=0
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=@DEVICE_NAME@
supported.versions=
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties


### AnyKernel install
# boot shell variables
BLOCK=boot;
IS_SLOT_DEVICE=auto;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;
NO_BLOCK_DISPLAY=1;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

# Select the correct image to flash
userflavor="$(file_getprop /system/build.prop "ro.build.flavor")";
case "$userflavor" in
    missi*|qssi*) os="miui"; os_string="MIUI ROM";;
    *) os="aosp"; os_string="AOSP ROM";;
esac;
ui_print "  -> $os_string is detected!";
if [ -f $AKHOME/kernels/$os/Image ] && [ -f $AKHOME/kernels/$os/dtb ] && [ -f $AKHOME/kernels/$os/dtbo.img ]; then
    mv $AKHOME/kernels/$os/Image $AKHOME/Image;
    mv $AKHOME/kernels/$os/dtb $AKHOME/dtb;
    mv $AKHOME/kernels/$os/dtbo.img $AKHOME/dtbo.img;
else
    ui_print "  -> There is no kernel files for $os_string in this zip! Aborting...";
    ui_print "  -> Please check that you have the correct kernel zip!";
    exit 1;
fi;

# boot install
split_boot;

flash_boot;
flash_generic dtbo;
## end boot install
AKEOF
# substitute device name (already applied by build_kernel.sh sed before this, keep safe)
sed -i "s/@DEVICE_NAME@/${DEVICE_NAME:-alioth}/" "$f"
echo "patched ok"
