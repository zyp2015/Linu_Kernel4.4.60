cmd_fs/fat/vfat.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o fs/fat/vfat.ko fs/fat/vfat.o fs/fat/vfat.mod.o
