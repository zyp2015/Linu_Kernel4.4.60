cmd_drivers/ptp/ptp.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o drivers/ptp/ptp.ko drivers/ptp/ptp.o drivers/ptp/ptp.mod.o
