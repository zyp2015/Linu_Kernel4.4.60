cmd_net/netfilter/xt_connlimit.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/netfilter/xt_connlimit.ko net/netfilter/xt_connlimit.o net/netfilter/xt_connlimit.mod.o
