cmd_sound/drivers/snd-virmidi.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o sound/drivers/snd-virmidi.ko sound/drivers/snd-virmidi.o sound/drivers/snd-virmidi.mod.o