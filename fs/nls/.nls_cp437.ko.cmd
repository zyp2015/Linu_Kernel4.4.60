cmd_fs/nls/nls_cp437.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o fs/nls/nls_cp437.ko fs/nls/nls_cp437.o fs/nls/nls_cp437.mod.o
