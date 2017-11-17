cmd_crypto/md5.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o crypto/md5.ko crypto/md5.o crypto/md5.mod.o
