cmd_fs/fuse/fuse.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o fs/fuse/fuse.ko fs/fuse/fuse.o fs/fuse/fuse.mod.o
