cmd_kernel/locking/built-in.o :=  arm-openwrt-linux-uclibcgnueabi-ld -EL    -r -o kernel/locking/built-in.o kernel/locking/mutex.o kernel/locking/semaphore.o kernel/locking/rwsem.o kernel/locking/percpu-rwsem.o kernel/locking/spinlock.o kernel/locking/osq_lock.o kernel/locking/lglock.o kernel/locking/rtmutex.o kernel/locking/rwsem-xadd.o 
