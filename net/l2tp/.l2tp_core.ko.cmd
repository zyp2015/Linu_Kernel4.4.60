cmd_net/l2tp/l2tp_core.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/l2tp/l2tp_core.ko net/l2tp/l2tp_core.o net/l2tp/l2tp_core.mod.o
