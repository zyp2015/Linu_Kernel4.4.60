cmd_net/ipv6/esp6.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/ipv6/esp6.ko net/ipv6/esp6.o net/ipv6/esp6.mod.o
