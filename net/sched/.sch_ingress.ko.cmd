cmd_net/sched/sch_ingress.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/sched/sch_ingress.ko net/sched/sch_ingress.o net/sched/sch_ingress.mod.o