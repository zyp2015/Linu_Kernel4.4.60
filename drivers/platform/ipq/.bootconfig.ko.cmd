cmd_drivers/platform/ipq/bootconfig.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o drivers/platform/ipq/bootconfig.ko drivers/platform/ipq/bootconfig.o drivers/platform/ipq/bootconfig.mod.o