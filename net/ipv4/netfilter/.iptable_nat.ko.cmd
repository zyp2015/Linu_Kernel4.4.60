cmd_net/ipv4/netfilter/iptable_nat.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/ipv4/netfilter/iptable_nat.ko net/ipv4/netfilter/iptable_nat.o net/ipv4/netfilter/iptable_nat.mod.o