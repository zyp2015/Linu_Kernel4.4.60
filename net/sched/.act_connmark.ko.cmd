cmd_net/sched/act_connmark.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o net/sched/act_connmark.ko net/sched/act_connmark.o net/sched/act_connmark.mod.o
