cmd_net/bridge/netfilter/ebt_snat.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/bridge/netfilter/ebt_snat.ko net/bridge/netfilter/ebt_snat.o net/bridge/netfilter/ebt_snat.mod.o