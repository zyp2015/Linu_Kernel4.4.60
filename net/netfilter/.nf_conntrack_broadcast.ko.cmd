cmd_net/netfilter/nf_conntrack_broadcast.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/netfilter/nf_conntrack_broadcast.ko net/netfilter/nf_conntrack_broadcast.o net/netfilter/nf_conntrack_broadcast.mod.o
