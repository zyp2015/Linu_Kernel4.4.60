cmd_sound/core/oss/snd-pcm-oss.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o sound/core/oss/snd-pcm-oss.ko sound/core/oss/snd-pcm-oss.o sound/core/oss/snd-pcm-oss.mod.o