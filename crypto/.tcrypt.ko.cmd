cmd_crypto/tcrypt.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o crypto/tcrypt.ko crypto/tcrypt.o crypto/tcrypt.mod.o
