cmd_fs/ntfs/ntfs.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o fs/ntfs/ntfs.ko fs/ntfs/ntfs.o fs/ntfs/ntfs.mod.o
