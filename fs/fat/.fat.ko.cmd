cmd_fs/fat/fat.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o fs/fat/fat.ko fs/fat/fat.o fs/fat/fat.mod.o
