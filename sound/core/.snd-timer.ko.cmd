cmd_sound/core/snd-timer.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o sound/core/snd-timer.ko sound/core/snd-timer.o sound/core/snd-timer.mod.o