cmd_lib/ts_bm.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o lib/ts_bm.ko lib/ts_bm.o lib/ts_bm.mod.o
