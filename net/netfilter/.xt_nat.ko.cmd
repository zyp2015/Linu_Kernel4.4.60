cmd_net/netfilter/xt_nat.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/netfilter/xt_nat.ko net/netfilter/xt_nat.o net/netfilter/xt_nat.mod.o
