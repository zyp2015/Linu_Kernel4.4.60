cmd_crypto/jitterentropy.o := arm-openwrt-linux-uclibcgnueabi-gcc -Wp,-MD,crypto/.jitterentropy.o.d  -nostdinc -isystem /home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/staging_dir/toolchain-arm_cortex-a7_gcc-5.2.0_uClibc-1.0.14_eabi/lib/gcc/arm-openwrt-linux-uclibcgnueabi/5.2.0/include -I./arch/arm/include -Iarch/arm/include/generated/uapi -Iarch/arm/include/generated  -Iinclude -I./arch/arm/include/uapi -Iarch/arm/include/generated/uapi -I./include/uapi -Iinclude/generated/uapi -include ./include/linux/kconfig.h -D__KERNEL__ -mlittle-endian -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -std=gnu89 -fno-PIE -fno-dwarf2-cfi-asm -fno-ipa-sra -mabi=aapcs-linux -mno-thumb-interwork -mfpu=vfp -funwind-tables -marm -D__LINUX_ARM_ARCH__=7 -march=armv7-a -msoft-float -Uarm -fno-delete-null-pointer-checks -Wno-maybe-uninitialized -Os -fno-caller-saves --param=allow-store-data-races=0 -Wframe-larger-than=1024 -fstack-protector -Wno-unused-but-set-variable -fomit-frame-pointer -fno-var-tracking-assignments -g -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -Werror=date-time -DCC_HAVE_ASM_GOTO -O0   -ffunction-sections -fdata-sections  -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(jitterentropy)"  -D"KBUILD_MODNAME=KBUILD_STR(jitterentropy_rng)" -c -o crypto/jitterentropy.o crypto/jitterentropy.c

source_crypto/jitterentropy.o := crypto/jitterentropy.c

deps_crypto/jitterentropy.o := \
    $(wildcard include/config/x86/tsc.h) \

crypto/jitterentropy.o: $(deps_crypto/jitterentropy.o)

$(deps_crypto/jitterentropy.o):
