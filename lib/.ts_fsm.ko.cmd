cmd_lib/ts_fsm.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o lib/ts_fsm.ko lib/ts_fsm.o lib/ts_fsm.mod.o
