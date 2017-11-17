cmd_lib/ts_kmp.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o lib/ts_kmp.ko lib/ts_kmp.o lib/ts_kmp.mod.o
