cmd_net/ipv4/ah4.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/ipv4/ah4.ko net/ipv4/ah4.o net/ipv4/ah4.mod.o
