cmd_net/ipv4/xfrm4_tunnel.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/ipv4/xfrm4_tunnel.ko net/ipv4/xfrm4_tunnel.o net/ipv4/xfrm4_tunnel.mod.o
