cmd_arch/arm/boot/dts/qcom-ipq8064-ap148_1xx.dtb := mkdir -p arch/arm/boot/dts/ ; arm-openwrt-linux-uclibcgnueabi-gcc -E -Wp,-MD,arch/arm/boot/dts/.qcom-ipq8064-ap148_1xx.dtb.d.pre.tmp -nostdinc -I./arch/arm/boot/dts -I./arch/arm/boot/dts/include -I./drivers/of/testcase-data -undef -D__DTS__ -x assembler-with-cpp -o arch/arm/boot/dts/.qcom-ipq8064-ap148_1xx.dtb.dts.tmp arch/arm/boot/dts/qcom-ipq8064-ap148_1xx.dts ; ./scripts/dtc/dtc -O dtb -o arch/arm/boot/dts/qcom-ipq8064-ap148_1xx.dtb -b 0 -i arch/arm/boot/dts/  -d arch/arm/boot/dts/.qcom-ipq8064-ap148_1xx.dtb.d.dtc.tmp arch/arm/boot/dts/.qcom-ipq8064-ap148_1xx.dtb.dts.tmp ; cat arch/arm/boot/dts/.qcom-ipq8064-ap148_1xx.dtb.d.pre.tmp arch/arm/boot/dts/.qcom-ipq8064-ap148_1xx.dtb.d.dtc.tmp > arch/arm/boot/dts/.qcom-ipq8064-ap148_1xx.dtb.d

source_arch/arm/boot/dts/qcom-ipq8064-ap148_1xx.dtb := arch/arm/boot/dts/qcom-ipq8064-ap148_1xx.dts

deps_arch/arm/boot/dts/qcom-ipq8064-ap148_1xx.dtb := \
  arch/arm/boot/dts/qcom-ipq8064-v1.0.dtsi \
  arch/arm/boot/dts/qcom-ipq8064.dtsi \
  arch/arm/boot/dts/skeleton.dtsi \
  arch/arm/boot/dts/include/dt-bindings/clock/qcom,gcc-ipq806x.h \
  arch/arm/boot/dts/include/dt-bindings/mfd/qcom-rpm.h \
  arch/arm/boot/dts/include/dt-bindings/clock/qcom,lcc-ipq806x.h \
  arch/arm/boot/dts/include/dt-bindings/soc/qcom,gsbi.h \
  arch/arm/boot/dts/include/dt-bindings/reset/qcom,gcc-ipq806x.h \
  arch/arm/boot/dts/include/dt-bindings/interrupt-controller/arm-gic.h \
  arch/arm/boot/dts/include/dt-bindings/interrupt-controller/irq.h \
  arch/arm/boot/dts/include/dt-bindings/gpio/gpio.h \
  arch/arm/boot/dts/qcom-ipq8064-ap148_1xx.dtsi \
  arch/arm/boot/dts/include/dt-bindings/input/input.h \
  arch/arm/boot/dts/include/dt-bindings/input/linux-event-codes.h \

arch/arm/boot/dts/qcom-ipq8064-ap148_1xx.dtb: $(deps_arch/arm/boot/dts/qcom-ipq8064-ap148_1xx.dtb)

$(deps_arch/arm/boot/dts/qcom-ipq8064-ap148_1xx.dtb):
