/* Copyright (c) 2016 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/remoteproc.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include "remoteproc_internal.h"
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/qcom_scm.h>
#include <linux/elf.h>
#include <soc/qcom/subsystem_restart.h>

static int debug_wcss;

#define DEBUG_WCSS_BREAK_AT_START	1
#define DEBUG_WCSS_NO_RESTART		2

#define WCSS_CRASH_REASON_SMEM 421
#define WCNSS_PAS_ID		6
#define QCOM_MDT_TYPE_MASK      (7 << 24)
#define QCOM_MDT_TYPE_HASH      (2 << 24)
#define STOP_ACK_TIMEOUT_MS 2000

#define QDSP6SS_RST_EVB 0x10
#define QDSP6SS_RESET 0x14
#define QDSP6SS_DBG_CFG 0x18
#define QDSP6SS_XO_CBCR 0x38
#define QDSP6SS_MEM_PWR_CTL 0xb0
#define QDSP6SS_SLEEP_CBCR 0x3C
#define QDSP6SS_BHS_STATUS 0x78
#define TCSR_GLOBAL_CFG0 0x0
#define TCSR_GLOBAL_CFG1 0x4

#define QDSP6SS_GFMUX_CTL 0x20
#define QDSP6SS_PWR_CTL 0x30
#define TCSR_HALTREQ 0x0
#define TCSR_HALTACK 0x4
#define TCSR_Q6_HALTREQ 0x0
#define TCSR_Q6_HALTACK 0x4
#define SSCAON_CONFIG 0x8
#define SSCAON_STATUS 0xc
#define GCC_WCSS_BCR 0x0
#define GCC_WCSSAON_RESET 0x10
#define GCC_WCSS_Q6_BCR 0x100
#define GCC_MISC_RESET_ADDR 0x8
#define HALTACK BIT(0)
#define BHS_EN_REST_ACK BIT(0)

struct q6v5_rtable {
	struct resource_table rtable;
	struct fw_rsc_hdr last_hdr;
};

static struct q6v5_rtable q6v5_rtable = {
	.rtable = {
		.ver = 1,
		.num = 0,
	},
	.last_hdr = {
		.type = RSC_LAST,
	},
};

struct q6v5_rproc_pdata {
	void __iomem *q6_base;
	void __iomem *tcsr_q6_base;
	void __iomem *tcsr_base;
	void __iomem *mpm_base;
	void __iomem *gcc_bcr_base;
	void __iomem *gcc_misc_base;
	void __iomem *tcsr_global_base;
	struct rproc *rproc;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	struct completion stop_done;
	struct completion err_ready;
	unsigned int err_ready_irq;
	struct qcom_smem_state *state;
	unsigned stop_bit;
	unsigned shutdown_bit;
	bool running;
	int emulation;
	int secure;
};

static struct q6v5_rproc_pdata *q6v5_rproc_pdata;

#define subsys_to_pdata(d) container_of(d, struct q6v5_rproc_pdata, subsys_desc)

static struct resource_table *q6v5_find_loaded_rsc_table(struct rproc *rproc,
	const struct firmware *fw)
{
	q6v5_rtable.rtable.offset[0] = sizeof(struct resource_table);
	return &(q6v5_rtable.rtable);
}

static struct resource_table *q6v5_find_rsc_table(struct rproc *rproc,
	const struct firmware *fw, int *tablesz)
{
	*tablesz = sizeof(struct q6v5_rtable);

	q6v5_rtable.rtable.offset[0] = sizeof(struct resource_table);
	return &(q6v5_rtable.rtable);
}

static void wcss_powerdown(struct q6v5_rproc_pdata *pdata)
{
	unsigned int nretry = 0;
	unsigned long val = 0;

	/* Assert WCSS/Q6 HALTREQ - 1 */
	nretry = 0;
	val = readl(pdata->tcsr_base + TCSR_HALTREQ);
	val |= BIT(0);
	writel(val, pdata->tcsr_base + TCSR_HALTREQ);
	while (1) {
		val = readl(pdata->tcsr_base + TCSR_HALTACK);
		if (val & HALTACK)
			break;
		mdelay(1);
		nretry++;
		if (nretry >= 10) {
			pr_warn("can't get TCSR haltACK\n");
			break;
		}
	}

	/* Check HALTACK */
	/* Set MPM_SSCAON_CONFIG 13 - 2 */
	val = readl(pdata->mpm_base + SSCAON_CONFIG);
	val |= BIT(13);
	writel(val, pdata->mpm_base + SSCAON_CONFIG);

	/* Set MPM_SSCAON_CONFIG 15 - 3 */
	val = readl(pdata->mpm_base + SSCAON_CONFIG);
	val |= BIT(15);
	val &= ~(BIT(16));
	val &= ~(BIT(17));
	val &= ~(BIT(18));
	writel(val, pdata->mpm_base + SSCAON_CONFIG);

	/* Set MPM_SSCAON_CONFIG 1 - 4 */
	val = readl(pdata->mpm_base + SSCAON_CONFIG);
	val |= BIT(1);
	writel(val, pdata->mpm_base + SSCAON_CONFIG);

	/* wait for SSCAON_STATUS to be 0x400 - 5 */
	nretry = 0;
	while (1) {
		val = readl(pdata->mpm_base + SSCAON_STATUS);
		/* ignore bits 16 to 31 */
		val &= 0xffff;
		if (val == BIT(10))
			break;
		nretry++;
		mdelay(1);
		if (nretry == 10) {
			pr_warn("can't get SSCAON_STATUS\n");
			break;
		}
	}

	/* Enable Q6/WCSS BLOCK ARES - 6 */
	val = readl(pdata->gcc_bcr_base + GCC_WCSSAON_RESET);
	val |= BIT(0);
	writel(val, pdata->gcc_bcr_base + GCC_WCSSAON_RESET);
	mdelay(1);

	/* Enable MPM_WCSSAON_CONFIG 13 - 7 */
	val = readl(pdata->mpm_base + SSCAON_CONFIG);
	val &= (~(BIT(13)));
	writel(val, pdata->mpm_base + SSCAON_CONFIG);

	/* Enable A2AB/ACMT/ECHAB ARES - 8 */
	/* De-assert WCSS/Q6 HALTREQ - 8 */
	val = readl(pdata->gcc_bcr_base + GCC_WCSS_BCR);
	val |= BIT(0);
	writel(val, pdata->gcc_bcr_base + GCC_WCSS_BCR);
	mdelay(1);
	val = readl(pdata->tcsr_base + TCSR_HALTREQ);
	val &= ~(BIT(0));
	writel(val, pdata->tcsr_base + TCSR_HALTREQ);

	return;
}

static void q6_powerdown(struct q6v5_rproc_pdata *pdata)
{
	int i = 0;
	unsigned int nretry = 0;
	unsigned long val = 0;

	/* Halt Q6 bus interface - 9*/
	val = readl(pdata->tcsr_q6_base + TCSR_Q6_HALTREQ);
	val |= BIT(0);
	writel(val, pdata->tcsr_q6_base + TCSR_Q6_HALTREQ);

	nretry = 0;
	while (1) {
		val = readl(pdata->tcsr_q6_base + TCSR_Q6_HALTACK);
		if (val & HALTACK)
			break;
		mdelay(1);
		nretry++;
		if (nretry >= 10) {
			pr_err("can't get TCSR Q6 haltACK\n");
			break;
		}
	}

	/* Disable Q6 Core clock - 10 */
	val = readl(pdata->q6_base + QDSP6SS_GFMUX_CTL);
	val &= (~(BIT(1)));
	writel(val, pdata->q6_base + QDSP6SS_GFMUX_CTL);

	/* Clamp I/O - 11 */
	val = readl(pdata->q6_base + QDSP6SS_PWR_CTL);
	val |= BIT(20);
	writel(val, pdata->q6_base + QDSP6SS_PWR_CTL);

	/* Clamp WL - 12 */
	val = readl(pdata->q6_base + QDSP6SS_PWR_CTL);
	val |= BIT(21);
	writel(val, pdata->q6_base + QDSP6SS_PWR_CTL);

	/* Clear Erase standby - 13 */
	val = readl(pdata->q6_base + QDSP6SS_PWR_CTL);
	val &= (~(BIT(18)));
	writel(val, pdata->q6_base + QDSP6SS_PWR_CTL);

	/* Clear Sleep RTN - 14 */
	val = readl(pdata->q6_base + QDSP6SS_PWR_CTL);
	val &= (~(BIT(19)));
	writel(val, pdata->q6_base + QDSP6SS_PWR_CTL);

	/* turn off QDSP6 memory foot/head switch one bank at a time - 15*/
	for (i = 0; i < 20; i++) {
		val = readl(pdata->q6_base + QDSP6SS_MEM_PWR_CTL);
		val &= (~(BIT(i)));
		writel(val, pdata->q6_base + QDSP6SS_MEM_PWR_CTL);
		mdelay(1);
	}

	/* Assert QMC memory RTN - 16 */
	val = readl(pdata->q6_base + QDSP6SS_PWR_CTL);
	val |= BIT(22);
	writel(val, pdata->q6_base + QDSP6SS_PWR_CTL);

	/* Turn off BHS - 17 */
	val = readl(pdata->q6_base + QDSP6SS_PWR_CTL);
	val &= (~(BIT(24)));
	writel(val, pdata->q6_base + QDSP6SS_PWR_CTL);
	udelay(1);
	/* Wait till BHS Reset is done */
	nretry = 0;
	while (1) {
		val = readl(pdata->q6_base + QDSP6SS_BHS_STATUS);
		if (!(val & BHS_EN_REST_ACK))
			break;
		mdelay(1);
		nretry++;
		if (nretry >= 10) {
			pr_err("BHS_STATUS not OFF\n");
			break;
		}
	}

	/* HALT CLEAR - 18 */
	val = readl(pdata->tcsr_q6_base + TCSR_Q6_HALTREQ);
	val &= ~(BIT(0));
	writel(val, pdata->tcsr_q6_base + TCSR_Q6_HALTREQ);

	/* Enable Q6 Block reset - 19 */
	val = readl(pdata->gcc_bcr_base + GCC_WCSS_Q6_BCR);
	val |= BIT(0);
	writel(val, pdata->gcc_bcr_base + GCC_WCSS_Q6_BCR);
	mdelay(2);

	return;
}

static int q6_rproc_stop(struct rproc *rproc)
{
	struct device *dev = rproc->dev.parent;
	struct platform_device *pdev = to_platform_device(dev);
	struct q6v5_rproc_pdata *pdata = platform_get_drvdata(pdev);
	int ret = 0;

	pdata->running = false;
	if (pdata->secure) {
		ret = qcom_scm_pas_shutdown(WCNSS_PAS_ID);
		if (ret)
			dev_err(dev, "failted to shutdown %d\n", ret);
		return ret;
	}

	/* Non secure */
	/* WCSS powerdown */
	wcss_powerdown(pdata);

	/* Q6 Power down */
	q6_powerdown(pdata);
	disable_irq(pdata->err_ready_irq);

	return ret;
}

static int wait_for_err_ready(struct q6v5_rproc_pdata *pdata)
{
	int ret;

wait_again:
	ret = wait_for_completion_timeout(&pdata->err_ready,
					  msecs_to_jiffies(10000));
	if (!ret) {
		if (debug_wcss)
			goto wait_again;
		pr_err("[%s]: Error ready timed out\n",
				pdata->subsys_desc.name);
		return -ETIMEDOUT;
	}

	return 0;
}

static int q6_rproc_start(struct rproc *rproc)
{
	struct device *dev = rproc->dev.parent;
	struct platform_device *pdev = to_platform_device(dev);
	struct q6v5_rproc_pdata *pdata = platform_get_drvdata(pdev);
	int temp = 19;
	unsigned long val = 0;
	unsigned int nretry = 0;
	int ret = 0;

	enable_irq(pdata->err_ready_irq);

	if (pdata->secure) {
		ret = qcom_scm_pas_auth_and_reset(WCNSS_PAS_ID);
		if (ret) {
			dev_err(dev, "q6-wcss reset failed\n");
			return ret;
		}
		goto skip_reset;
	}

	/* Release Q6 and WCSS reset */
	val = readl(pdata->gcc_bcr_base + GCC_WCSS_BCR);
	val &= ~(BIT(0));
	writel(val, pdata->gcc_bcr_base + GCC_WCSS_BCR);
	val = readl(pdata->gcc_bcr_base + GCC_WCSS_Q6_BCR);
	val &= ~(BIT(0));
	writel(val, pdata->gcc_bcr_base + GCC_WCSS_Q6_BCR);

	/* This is for Lithium configuration - clock gating */
	val = readl(pdata->tcsr_global_base + TCSR_GLOBAL_CFG0);
	val |= 0x14;
	writel(val, pdata->tcsr_global_base + TCSR_GLOBAL_CFG0);

	/* This is for Lithium configuration - bus arbitration */
	val = readl(pdata->tcsr_global_base + TCSR_GLOBAL_CFG1);
	val = 0;
	writel(val, pdata->tcsr_global_base + TCSR_GLOBAL_CFG1);

	/* Write bootaddr to EVB so that Q6WCSS will jump there after reset */
	writel(rproc->bootaddr >> 4, pdata->q6_base + QDSP6SS_RST_EVB);
	/* Turn on XO clock. It is required for BHS and memory operation */
	writel(0x1, pdata->q6_base + QDSP6SS_XO_CBCR);
	/* Turn on BHS */
	writel(0x1700000, pdata->q6_base + QDSP6SS_PWR_CTL);
	udelay(1);

	/* Wait till BHS Reset is done */
	while (1) {
		val = readl(pdata->q6_base + QDSP6SS_BHS_STATUS);
		if (val & BHS_EN_REST_ACK)
			break;
		mdelay(1);
		nretry++;
		if (nretry >= 10) {
			pr_err("BHS_STATUS not ON\n");
			break;
		}
	}

	/* Put LDO in bypass mode */
	writel(0x3700000, pdata->q6_base + QDSP6SS_PWR_CTL);
	/* De-assert QDSP6 complier memory clamp */
	writel(0x3300000, pdata->q6_base + QDSP6SS_PWR_CTL);
	/* De-assert memory peripheral sleep and L2 memory standby */
	writel(0x33c0000, pdata->q6_base + QDSP6SS_PWR_CTL);

	/* turn on QDSP6 memory foot/head switch one bank at a time */
	while  (temp >= 0) {
		val = readl(pdata->q6_base + QDSP6SS_MEM_PWR_CTL);
		val = val | 1 << temp;
		writel(val, pdata->q6_base + QDSP6SS_MEM_PWR_CTL);
		val = readl(pdata->q6_base + QDSP6SS_MEM_PWR_CTL);
		mdelay(10);
		temp -= 1;
	}
	/* Remove the QDSP6 core memory word line clamp */
	writel(0x31FFFFF, pdata->q6_base + QDSP6SS_PWR_CTL);
	/* Remove QDSP6 I/O clamp */
	writel(0x30FFFFF, pdata->q6_base + QDSP6SS_PWR_CTL);

	if (debug_wcss & DEBUG_WCSS_BREAK_AT_START)
		writel(0x20000001, pdata->q6_base + QDSP6SS_DBG_CFG);

	/* Bring Q6 out of reset and stop the core */
	writel(0x5, pdata->q6_base + QDSP6SS_RESET);

	/* Retain debugger state during next QDSP6 reset */
	if (!(debug_wcss & DEBUG_WCSS_BREAK_AT_START))
		writel(0x0, pdata->q6_base + QDSP6SS_DBG_CFG);

	/* Turn on the QDSP6 core clock */
	writel(0x102, pdata->q6_base + QDSP6SS_GFMUX_CTL);
	/* Enable the core to run */
	writel(0x4, pdata->q6_base + QDSP6SS_RESET);

skip_reset:

	ret = wait_for_err_ready(pdata);
	if (!ret)
		pdata->running = true;

	return ret;
}

static void *q6_da_to_va(struct rproc *rproc, u64 da, int len)
{
	unsigned long addr = (__force unsigned long)(da & 0xFFFFFFFF);

	return ioremap(addr, len);
}

static struct rproc_ops q6v5_rproc_ops = {
	.start		= q6_rproc_start,
	.da_to_va	= q6_da_to_va,
	.stop		= q6_rproc_stop,
};

static struct rproc_fw_ops q6_fw_ops;

static irqreturn_t wcss_err_ready_intr_handler(int irq, void *dev_id)
{
	struct q6v5_rproc_pdata *pdata = dev_id;

	pr_info("Subsystem error monitoring/handling services are up\n");

	complete(&pdata->err_ready);
	return IRQ_HANDLED;
}

static irqreturn_t wcss_err_fatal_intr_handler(int irq, void *dev_id)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(dev_id);
	char *msg;
	size_t len;

	if (!pdata->running)
		return IRQ_HANDLED;

	msg = qcom_smem_get(QCOM_SMEM_HOST_ANY, WCSS_CRASH_REASON_SMEM, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0])
		pr_err("Fatal error received from wcss software!: %s\n", msg);
	else
		pr_err("Fatal error received no message!\n");

	if (debug_wcss & DEBUG_WCSS_NO_RESTART) {
		pr_info("WCSS Restart is disabled, Ignoring fatal error.\n");
		return IRQ_HANDLED;
	}

	subsys_set_crash_status(pdata->subsys, CRASH_STATUS_ERR_FATAL);
	subsystem_restart_dev(pdata->subsys);
	return IRQ_HANDLED;
}

static irqreturn_t wcss_stop_ack_intr_handler(int irq, void *dev_id)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(dev_id);

	pr_info("Received stop ack interrupt from wcss\n");
	complete(&pdata->stop_done);
	return IRQ_HANDLED;
}

static irqreturn_t wcss_wdog_bite_intr_handler(int irq, void *dev_id)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(dev_id);
	char *msg;
	size_t len;

	if (!pdata->running) {
		complete(&pdata->stop_done);
		return IRQ_HANDLED;
	}

	msg = qcom_smem_get(QCOM_SMEM_HOST_ANY, WCSS_CRASH_REASON_SMEM, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0])
		pr_err("Watchdog bite received from wcss software!: %s\n", msg);
	else
		pr_err("Watchdog bit received no message!\n");

	if (debug_wcss & DEBUG_WCSS_NO_RESTART) {
		pr_info("WCSS Restart is disabled, Ignoring WDOG Bite.\n");
		return IRQ_HANDLED;
	}

	subsys_set_crash_status(pdata->subsys, CRASH_STATUS_WDOG_BITE);
	subsystem_restart_dev(pdata->subsys);

	return IRQ_HANDLED;
}


static int start_q6(const struct subsys_desc *subsys)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(subsys);
	struct rproc *rproc = pdata->rproc;
	int ret = 0;

	reinit_completion(&pdata->stop_done);
	reinit_completion(&pdata->err_ready);
	pdata->subsys_desc.ramdump_disable = 1;
	ret = rproc_add(rproc);
	if (ret)
		return ret;

	wait_for_completion(&rproc->firmware_loading_complete);

	ret = rproc_boot(rproc);
	if (ret) {
		pr_err("couldn't boot q6v5: %d\n", ret);
	}

	return ret;
}

static int stop_q6(const struct subsys_desc *subsys, bool force_stop)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(subsys);
	struct rproc *rproc = pdata->rproc;
	int ret = 0;

	if (!subsys_get_crash_status(pdata->subsys) && force_stop) {
		qcom_smem_state_update_bits(pdata->state,
			BIT(pdata->stop_bit), BIT(pdata->stop_bit));

		ret = wait_for_completion_timeout(&pdata->stop_done,
			msecs_to_jiffies(STOP_ACK_TIMEOUT_MS));

		if (ret == 0)
			pr_err("Timedout waiting for stop-ack\n");

		qcom_smem_state_update_bits(pdata->state,
			BIT(pdata->stop_bit), 0);
	}

	rproc_shutdown(rproc);
	rproc_del(rproc);

	return 0;
}

static void wcss_panic_handler(const struct subsys_desc *subsys)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(subsys);

	pdata->running = false;

	if (!subsys_get_crash_status(pdata->subsys)) {
		qcom_smem_state_update_bits(pdata->state,
			BIT(pdata->shutdown_bit), BIT(pdata->shutdown_bit));
		mdelay(STOP_ACK_TIMEOUT_MS);
	}
	return;
}

static int q6v5_load(struct rproc *rproc, const struct firmware *fw)
{
	int ret = 0;
	const char *name = rproc->firmware;
	size_t name_len;
	char *segment_name;
	struct device *dev_rproc = rproc->dev.parent;
	struct elf32_hdr *ehdr;
	int i = 0;
	const u8 *elf_data;
	struct elf32_phdr *phdr;
	struct platform_device *pdev = to_platform_device(dev_rproc);
	struct q6v5_rproc_pdata *pdata = platform_get_drvdata(pdev);

	name_len = strlen(name);
	if (name_len <= 4) {
		dev_err(dev_rproc, "Firmware name should be >4bytes (*.mdt)\n");
		return -EINVAL;
	}

	segment_name = kstrdup(name, GFP_KERNEL);
	if (!segment_name)
		return -ENOMEM;

	if (!fw) {
		dev_err(dev_rproc, "failed to load %s\n", name);
		return -EINVAL;
	}

	elf_data = fw->data;
	ehdr = (struct elf32_hdr *)fw->data;
	phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);

	if (pdata->secure) {
		ret = qcom_scm_pas_init_image(WCNSS_PAS_ID, fw->data, fw->size);
		if (ret) {
			dev_err(dev_rproc, "image authentication failed\n");
			return ret;
		}
	}
	pr_info("Sanity check passed for the image\n");

	/* go through the available ELF segments and load it */
	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		u32 pa = phdr->p_paddr;
		u32 memsz = phdr->p_memsz;
		u32 filesz = phdr->p_filesz;
		void *ptr;

		if ((phdr->p_type != PT_LOAD) ||
				((phdr->p_flags & QCOM_MDT_TYPE_MASK)
				 == QCOM_MDT_TYPE_HASH) || (!phdr->p_memsz))
			continue;

		/* grab the kernel address for this device address */
		ptr = rproc_da_to_va(rproc, pa, memsz);
		if (!ptr) {
			dev_err(dev_rproc, "bad phdr pa 0x%x mem 0x%x\n", pa,
					memsz);
			ret = -EINVAL;
			return ret;
		}

		/* put the segment where the remote processor expects it */
		if (phdr->p_filesz) {
			/* The firmware file has <name>.mdt as the ELF header +
			 * hash segment, followed by <name>.b00, <name>.b01, etc
			 * for every ELF segment of the firmware file. The
			 * rproc loads the first <name>.mdt file, and for the
			 * ELF segments that we need to load, we make the
			 * filename as <name>.b"segment_number"
			 */
			snprintf(segment_name + name_len - 3,
				sizeof(segment_name + name_len - 3),
				"b%02d", i);
			ret = request_firmware(&fw, segment_name, dev_rproc);
			if (ret) {
				dev_err(dev_rproc, "can't to load %s\n",
						segment_name);
				iounmap(ptr);
				break;
			}
			memcpy_toio(ptr, fw->data, fw->size);
			release_firmware(fw);
		}

		/* for .bss and sections that needs to be memset to zero */
		if (memsz > filesz)
			memset_io(ptr + filesz, 0, memsz - filesz);

		iounmap(ptr);
	}
	kfree(segment_name);
	return ret;
}

static int q6_rproc_probe(struct platform_device *pdev)
{
	const char *firmware_name;
	struct rproc *rproc;
	int ret;
	struct resource *resource;
	struct qcom_smem_state *state;
	unsigned stop_bit;

	state = qcom_smem_state_get(&pdev->dev, "stop",
					&stop_bit);
	if (IS_ERR(state)) {
		/* Wait till SMP2P is registered and up */
		return -EPROBE_DEFER;
	}

	ret = dma_set_coherent_mask(&pdev->dev,
			DMA_BIT_MASK(sizeof(dma_addr_t) * 8));
	if (ret) {
		dev_err(&pdev->dev, "dma_set_coherent_mask: %d\n", ret);
		return ret;
	}

	ret = of_property_read_string(pdev->dev.of_node, "firmware",
			&firmware_name);
	if (ret) {
		dev_err(&pdev->dev, "couldn't read firmware name: %d\n", ret);
		return ret;
	}

	rproc = rproc_alloc(&pdev->dev, "q6v5-wcss", &q6v5_rproc_ops,
				firmware_name,
				sizeof(*q6v5_rproc_pdata));
	if (unlikely(!rproc))
		return -ENOMEM;

	q6v5_rproc_pdata = rproc->priv;
	q6v5_rproc_pdata->rproc = rproc;
	q6v5_rproc_pdata->emulation = of_property_read_bool(pdev->dev.of_node,
					"qca,emulation");
	q6v5_rproc_pdata->secure = of_property_read_bool(pdev->dev.of_node,
					"qca,secure");
	rproc->has_iommu = false;

	q6_fw_ops = *(rproc->fw_ops);
	q6_fw_ops.find_rsc_table = q6v5_find_rsc_table;
	q6_fw_ops.find_loaded_rsc_table = q6v5_find_loaded_rsc_table;
	q6_fw_ops.load = q6v5_load;

	if (!q6v5_rproc_pdata->secure) {
		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"wcss-base");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->q6_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->q6_base)
			goto free_rproc;

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"tcsr-q6-base");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->tcsr_q6_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->tcsr_q6_base)
			goto free_rproc;

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"tcsr-base");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->tcsr_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->tcsr_base)
			goto free_rproc;

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"mpm-base");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->mpm_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->mpm_base)
			goto free_rproc;

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"gcc-wcss-bcr-base");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->gcc_bcr_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->gcc_bcr_base)
			goto free_rproc;

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"gcc-wcss-misc-base");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->gcc_misc_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->gcc_misc_base)
			goto free_rproc;

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"tcsr-global");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->tcsr_global_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->tcsr_global_base)
			goto free_rproc;

	}

	platform_set_drvdata(pdev, q6v5_rproc_pdata);

	rproc->fw_ops = &q6_fw_ops;

	q6v5_rproc_pdata->err_ready_irq = platform_get_irq_byname(pdev,
						"qcom,gpio-err-ready");
	if (q6v5_rproc_pdata->err_ready_irq < 0) {
		pr_err("Can't get err-ready irq number %d\n",
			q6v5_rproc_pdata->err_ready_irq);
		goto free_rproc;
	}
	ret = devm_request_threaded_irq(&pdev->dev,
				q6v5_rproc_pdata->err_ready_irq,
				NULL, wcss_err_ready_intr_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"error_ready_interrupt", q6v5_rproc_pdata);
	if (ret < 0) {
		pr_err("Can't register err ready handler\n");
		goto free_rproc;
	}
	disable_irq(q6v5_rproc_pdata->err_ready_irq);

	q6v5_rproc_pdata->state = qcom_smem_state_get(&pdev->dev, "stop",
			&q6v5_rproc_pdata->stop_bit);
	if (IS_ERR(q6v5_rproc_pdata->state)) {
		pr_err("Can't get stop bit status fro SMP2P\n");
		goto free_rproc;
	}

	q6v5_rproc_pdata->state = qcom_smem_state_get(&pdev->dev, "shutdown",
			&q6v5_rproc_pdata->shutdown_bit);
	if (IS_ERR(q6v5_rproc_pdata->state)) {
		pr_err("Can't get shutdown bit status fro SMP2P\n");
		goto free_rproc;
	}

	q6v5_rproc_pdata->subsys_desc.is_not_loadable = 0;
	q6v5_rproc_pdata->subsys_desc.name = "q6v5-wcss";
	q6v5_rproc_pdata->subsys_desc.dev = &pdev->dev;
	q6v5_rproc_pdata->subsys_desc.owner = THIS_MODULE;
	q6v5_rproc_pdata->subsys_desc.shutdown = stop_q6;
	q6v5_rproc_pdata->subsys_desc.powerup = start_q6;
	q6v5_rproc_pdata->subsys_desc.ramdump = NULL;
	q6v5_rproc_pdata->subsys_desc.crash_shutdown = wcss_panic_handler;
	q6v5_rproc_pdata->subsys_desc.err_fatal_handler =
				wcss_err_fatal_intr_handler;
	q6v5_rproc_pdata->subsys_desc.stop_ack_handler =
				wcss_stop_ack_intr_handler;
	q6v5_rproc_pdata->subsys_desc.wdog_bite_handler =
				wcss_wdog_bite_intr_handler;

	q6v5_rproc_pdata->subsys = subsys_register(
					&q6v5_rproc_pdata->subsys_desc);
	if (IS_ERR(q6v5_rproc_pdata->subsys)) {
		ret = PTR_ERR(q6v5_rproc_pdata->subsys);
		goto free_rproc;
	}

	init_completion(&q6v5_rproc_pdata->stop_done);
	init_completion(&q6v5_rproc_pdata->err_ready);
	q6v5_rproc_pdata->running = false;

	return 0;

free_rproc:
	if (q6v5_rproc_pdata->gcc_misc_base)
		iounmap(q6v5_rproc_pdata->gcc_misc_base);

	if (q6v5_rproc_pdata->gcc_bcr_base)
		iounmap(q6v5_rproc_pdata->gcc_bcr_base);

	if (q6v5_rproc_pdata->mpm_base)
		iounmap(q6v5_rproc_pdata->mpm_base);

	if (q6v5_rproc_pdata->tcsr_base)
		iounmap(q6v5_rproc_pdata->tcsr_base);

	if (q6v5_rproc_pdata->tcsr_q6_base)
		iounmap(q6v5_rproc_pdata->tcsr_q6_base);

	if (q6v5_rproc_pdata->q6_base)
		iounmap(q6v5_rproc_pdata->q6_base);

	if (q6v5_rproc_pdata->tcsr_global_base)
		iounmap(q6v5_rproc_pdata->tcsr_global_base);


	rproc_put(rproc);
	return -EIO;
}

static int q6_rproc_remove(struct platform_device *pdev)
{
	struct q6v5_rproc_pdata *pdata;
	struct rproc *rproc;

	pdata = platform_get_drvdata(pdev);
	rproc = q6v5_rproc_pdata->rproc;

	rproc_del(rproc);
	rproc_put(rproc);

	subsys_unregister(pdata->subsys);

	return 0;
}

static const struct of_device_id q6_match_table[] = {
	{ .compatible = "qca,q6v5-wcss-rproc" },
	{}
};

static struct platform_driver q6_rproc_driver = {
	.probe = q6_rproc_probe,
	.remove = q6_rproc_remove,
	.driver = {
		.name = "q6v5-wcss",
		.of_match_table = q6_match_table,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(q6_rproc_driver);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("QCA Remote Processor control driver");

module_param(debug_wcss, int, 0644);
