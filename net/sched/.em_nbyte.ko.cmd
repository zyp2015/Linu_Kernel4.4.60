cmd_net/sched/em_nbyte.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/sched/em_nbyte.ko net/sched/em_nbyte.o net/sched/em_nbyte.mod.o
