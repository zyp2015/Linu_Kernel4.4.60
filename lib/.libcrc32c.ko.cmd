cmd_lib/libcrc32c.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o lib/libcrc32c.ko lib/libcrc32c.o lib/libcrc32c.mod.o
