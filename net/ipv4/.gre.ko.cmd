cmd_net/ipv4/gre.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/ipv4/gre.ko net/ipv4/gre.o net/ipv4/gre.mod.o
