cmd_net/sched/cls_route.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/sched/cls_route.ko net/sched/cls_route.o net/sched/cls_route.mod.o
