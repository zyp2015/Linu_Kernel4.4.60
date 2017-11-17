cmd_fs/fat/msdos.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o fs/fat/msdos.ko fs/fat/msdos.o fs/fat/msdos.mod.o
