# cannot find fixdep (/home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/tools/build/fixdep)
# using basic dep data

tests/llvm-src-base.o: tests/llvm-src-base.c \
 /home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/tools/perf/tests/llvm.h \
 /home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/staging_dir/toolchain-arm_cortex-a7_gcc-5.2.0_uClibc-1.0.14_eabi/lib/gcc/arm-openwrt-linux-uclibcgnueabi/5.2.0/include/stddef.h \
 /home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/staging_dir/toolchain-arm_cortex-a7_gcc-5.2.0_uClibc-1.0.14_eabi/lib/gcc/arm-openwrt-linux-uclibcgnueabi/5.2.0/include/stdbool.h
cmd_tests/llvm-src-base.o := arm-openwrt-linux-uclibcgnueabi-gcc -Wp,-MD,tests/.llvm-src-base.o.d,-MT,tests/llvm-src-base.o  -Wbad-function-cast -Wdeclaration-after-statement -Wformat-security -Wformat-y2k -Winit-self -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wno-system-headers -Wold-style-definition -Wpacked -Wredundant-decls -Wshadow -Wstrict-aliasing=3 -Wstrict-prototypes -Wswitch-default -Wswitch-enum -Wundef -Wwrite-strings -Wformat -DHAVE_PERF_REGS_SUPPORT -O6 -fno-omit-frame-pointer -ggdb3 -funwind-tables -Wall -Wextra -std=gnu99 -D_FORTIFY_SOURCE=2 -I/home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/tools/perf/util/include -I/home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/tools/perf/arch/arm/include -I/home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/tools/include/ -I/home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/arch/arm/include/uapi -I/home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/arch/arm/include -I/home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/include/uapi -I/home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/include -I/home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/tools/perf/util -I/home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/tools/perf -I/home/kokozhao/svn/RT5000W-D187/spf7.0/02.Coding/qualcomm_sdk/qsdk/qca/src/linux-4.4/tools/lib/ -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DHAVE_SYNC_COMPARE_AND_SWAP_SUPPORT -DHAVE_PTHREAD_ATTR_SETAFFINITY_NP -DHAVE_LIBELF_SUPPORT -DHAVE_LIBELF_MMAP_SUPPORT -DHAVE_ELF_GETPHDRNUM_SUPPORT -DHAVE_DWARF_SUPPORT  -DHAVE_LIBBPF_SUPPORT -DHAVE_DWARF_UNWIND_SUPPORT -DNO_LIBPERL -DHAVE_TIMERFD_SUPPORT -DNO_LIBPYTHON -DHAVE_LIBBFD_SUPPORT -DHAVE_ZLIB_SUPPORT -D"BUILD_STR(s)=\#s"   -c -o tests/llvm-src-base.o tests/llvm-src-base.c
