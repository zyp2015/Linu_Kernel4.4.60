cmd_net/ipv6/sit.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/ipv6/sit.ko net/ipv6/sit.o net/ipv6/sit.mod.o
