cmd_drivers/phy/phy-qcom-dwc3.ko := arm-openwrt-linux-uclibcgnueabi-ld -EL -r  -T ./scripts/module-common.lds --build-id  -T ./arch/arm/kernel/module.lds -o drivers/phy/phy-qcom-dwc3.ko drivers/phy/phy-qcom-dwc3.o drivers/phy/phy-qcom-dwc3.mod.o