cmd_sound/core/snd-compress.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o sound/core/snd-compress.ko sound/core/snd-compress.o sound/core/snd-compress.mod.o