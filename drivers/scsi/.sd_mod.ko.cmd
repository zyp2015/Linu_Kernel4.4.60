cmd_drivers/scsi/sd_mod.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o drivers/scsi/sd_mod.ko drivers/scsi/sd_mod.o drivers/scsi/sd_mod.mod.o
