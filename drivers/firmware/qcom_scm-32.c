/* Copyright (c) 2010,2015-2016, The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/qcom_scm.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#include "qcom_scm.h"

#define QCOM_SCM_FLAG_COLDBOOT_CPU0	0x00
#define QCOM_SCM_FLAG_COLDBOOT_CPU1	0x01
#define QCOM_SCM_FLAG_COLDBOOT_CPU2	0x08
#define QCOM_SCM_FLAG_COLDBOOT_CPU3	0x20

#define QCOM_SCM_FLAG_WARMBOOT_CPU0	0x04
#define QCOM_SCM_FLAG_WARMBOOT_CPU1	0x02
#define QCOM_SCM_FLAG_WARMBOOT_CPU2	0x10
#define QCOM_SCM_FLAG_WARMBOOT_CPU3	0x40

struct qcom_scm_entry {
	int flag;
	void *entry;
};

static struct qcom_scm_entry qcom_scm_wb[] = {
	{ .flag = QCOM_SCM_FLAG_WARMBOOT_CPU0 },
	{ .flag = QCOM_SCM_FLAG_WARMBOOT_CPU1 },
	{ .flag = QCOM_SCM_FLAG_WARMBOOT_CPU2 },
	{ .flag = QCOM_SCM_FLAG_WARMBOOT_CPU3 },
};

static DEFINE_MUTEX(qcom_scm_lock);

/**
 * struct qcom_scm_command - one SCM command buffer
 * @len: total available memory for command and response
 * @buf_offset: start of command buffer
 * @resp_hdr_offset: start of response buffer
 * @id: command to be executed
 * @buf: buffer returned from qcom_scm_get_command_buffer()
 *
 * An SCM command is laid out in memory as follows:
 *
 *	------------------- <--- struct qcom_scm_command
 *	| command header  |
 *	------------------- <--- qcom_scm_get_command_buffer()
 *	| command buffer  |
 *	------------------- <--- struct qcom_scm_response and
 *	| response header |      qcom_scm_command_to_response()
 *	------------------- <--- qcom_scm_get_response_buffer()
 *	| response buffer |
 *	-------------------
 *
 * There can be arbitrary padding between the headers and buffers so
 * you should always use the appropriate qcom_scm_get_*_buffer() routines
 * to access the buffers in a safe manner.
 */
struct qcom_scm_command {
	__le32 len;
	__le32 buf_offset;
	__le32 resp_hdr_offset;
	__le32 id;
	__le32 buf[0];
};

/**
 * struct qcom_scm_response - one SCM response buffer
 * @len: total available memory for response
 * @buf_offset: start of response data relative to start of qcom_scm_response
 * @is_complete: indicates if the command has finished processing
 */
struct qcom_scm_response {
	__le32 len;
	__le32 buf_offset;
	__le32 is_complete;
};

/**
 * qcom_scm_command_to_response() - Get a pointer to a qcom_scm_response
 * @cmd: command
 *
 * Returns a pointer to a response for a command.
 */
static inline struct qcom_scm_response *qcom_scm_command_to_response(
		const struct qcom_scm_command *cmd)
{
	return (void *)cmd + le32_to_cpu(cmd->resp_hdr_offset);
}

/**
 * qcom_scm_get_command_buffer() - Get a pointer to a command buffer
 * @cmd: command
 *
 * Returns a pointer to the command buffer of a command.
 */
static inline void *qcom_scm_get_command_buffer(const struct qcom_scm_command *cmd)
{
	return (void *)cmd->buf;
}

/**
 * qcom_scm_get_response_buffer() - Get a pointer to a response buffer
 * @rsp: response
 *
 * Returns a pointer to a response buffer of a response.
 */
static inline void *qcom_scm_get_response_buffer(const struct qcom_scm_response *rsp)
{
	return (void *)rsp + le32_to_cpu(rsp->buf_offset);
}

static u32 smc(u32 cmd_addr)
{
	int context_id;
	register u32 r0 asm("r0") = 1;
	register u32 r1 asm("r1") = (u32)&context_id;
	register u32 r2 asm("r2") = cmd_addr;
	do {
		asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r0")
			__asmeq("%2", "r1")
			__asmeq("%3", "r2")
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
			"smc	#0	@ switch to secure world\n"
			: "=r" (r0)
			: "r" (r0), "r" (r1), "r" (r2)
			: "r3");
	} while (r0 == QCOM_SCM_INTERRUPTED);

	return r0;
}

/**
 * qcom_scm_call() - Send an SCM command
 * @dev: struct device
 * @svc_id: service identifier
 * @cmd_id: command identifier
 * @cmd_buf: command buffer
 * @cmd_len: length of the command buffer
 * @resp_buf: response buffer
 * @resp_len: length of the response buffer
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 *
 * A note on cache maintenance:
 * Note that any buffers that are expected to be accessed by the secure world
 * must be flushed before invoking qcom_scm_call and invalidated in the cache
 * immediately after qcom_scm_call returns. Cache maintenance on the command
 * and response buffers is taken care of by qcom_scm_call; however, callers are
 * responsible for any other cached buffers passed over to the secure world.
 */
static int qcom_scm_call(struct device *dev, u32 svc_id, u32 cmd_id,
			 const void *cmd_buf, size_t cmd_len, void *resp_buf,
			 size_t resp_len)
{
	int ret;
	struct qcom_scm_command *cmd;
	struct qcom_scm_response *rsp;
	size_t alloc_len = sizeof(*cmd) + cmd_len + sizeof(*rsp) + resp_len;
	dma_addr_t cmd_phys;

	cmd = kzalloc(PAGE_ALIGN(alloc_len), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->len = cpu_to_le32(alloc_len);
	cmd->buf_offset = cpu_to_le32(sizeof(*cmd));
	cmd->resp_hdr_offset = cpu_to_le32(sizeof(*cmd) + cmd_len);

	cmd->id = cpu_to_le32((svc_id << 10) | cmd_id);
	if (cmd_buf)
		memcpy(qcom_scm_get_command_buffer(cmd), cmd_buf, cmd_len);

	rsp = qcom_scm_command_to_response(cmd);

	cmd_phys = dma_map_single(dev, cmd, alloc_len, DMA_TO_DEVICE);
	if (dma_mapping_error(dev, cmd_phys)) {
		kfree(cmd);
		return -ENOMEM;
	}

	mutex_lock(&qcom_scm_lock);
	ret = smc(cmd_phys);
	if (ret < 0)
		ret = qcom_scm_remap_error(ret);
	mutex_unlock(&qcom_scm_lock);
	if (ret)
		goto out;

	do {
		dma_sync_single_for_cpu(dev, cmd_phys + sizeof(*cmd) + cmd_len,
					sizeof(*rsp), DMA_FROM_DEVICE);
	} while (!rsp->is_complete);

	if (resp_buf) {
		dma_sync_single_for_cpu(dev, cmd_phys + sizeof(*cmd) + cmd_len +
					le32_to_cpu(rsp->buf_offset),
					resp_len, DMA_FROM_DEVICE);
		memcpy(resp_buf, qcom_scm_get_response_buffer(rsp),
		       resp_len);
	}
out:
	dma_unmap_single(dev, cmd_phys, alloc_len, DMA_TO_DEVICE);
	kfree(cmd);
	return ret;
}

#define SCM_CLASS_REGISTER	(0x2 << 8)
#define SCM_MASK_IRQS		BIT(5)
#define SCM_ATOMIC(svc, cmd, n) (((((svc) << 10)|((cmd) & 0x3ff)) << 12) | \
				SCM_CLASS_REGISTER | \
				SCM_MASK_IRQS | \
				(n & 0xf))

/**
 * qcom_scm_call_atomic1() - Send an atomic SCM command with one argument
 * @svc_id: service identifier
 * @cmd_id: command identifier
 * @arg1: first argument
 *
 * This shall only be used with commands that are guaranteed to be
 * uninterruptable, atomic and SMP safe.
 */
static s32 qcom_scm_call_atomic1(u32 svc, u32 cmd, u32 arg1)
{
	int context_id;

	register u32 r0 asm("r0") = SCM_ATOMIC(svc, cmd, 1);
	register u32 r1 asm("r1") = (u32)&context_id;
	register u32 r2 asm("r2") = arg1;

	asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r0")
			__asmeq("%2", "r1")
			__asmeq("%3", "r2")
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
			"smc    #0      @ switch to secure world\n"
			: "=r" (r0)
			: "r" (r0), "r" (r1), "r" (r2)
			: "r3");
	return r0;
}

/**
 * qcom_scm_call_atomic2() - Send an atomic SCM command with two arguments
 * @svc_id:	service identifier
 * @cmd_id:	command identifier
 * @arg1:	first argument
 * @arg2:	second argument
 *
 * This shall only be used with commands that are guaranteed to be
 * uninterruptable, atomic and SMP safe.
 */
static s32 qcom_scm_call_atomic2(u32 svc, u32 cmd, u32 arg1, u32 arg2)
{
	int context_id;

	register u32 r0 asm("r0") = SCM_ATOMIC(svc, cmd, 2);
	register u32 r1 asm("r1") = (u32)&context_id;
	register u32 r2 asm("r2") = arg1;
	register u32 r3 asm("r3") = arg2;

	asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r0")
			__asmeq("%2", "r1")
			__asmeq("%3", "r2")
			__asmeq("%4", "r3")
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
			"smc    #0      @ switch to secure world\n"
			: "=r" (r0)
			: "r" (r0), "r" (r1), "r" (r2), "r" (r3)
			);
	return r0;
}

#define R0_STR "r0"
#define R1_STR "r1"
#define R2_STR "r2"
#define R3_STR "r3"
#define R4_STR "r4"
#define R5_STR "r5"
#define R6_STR "r6"

static int __scm_call_armv8_32(u32 w0, u32 w1, u32 w2, u32 w3, u32 w4, u32 w5,
				u64 *ret1, u64 *ret2, u64 *ret3)
{
	register u32 r0 asm("r0") = w0;
	register u32 r1 asm("r1") = w1;
	register u32 r2 asm("r2") = w2;
	register u32 r3 asm("r3") = w3;
	register u32 r4 asm("r4") = w4;
	register u32 r5 asm("r5") = w5;
	register u32 r6 asm("r6") = 0;

	do {
		asm volatile(
			__asmeq("%0", R0_STR)
			__asmeq("%1", R1_STR)
			__asmeq("%2", R2_STR)
			__asmeq("%3", R3_STR)
			__asmeq("%4", R0_STR)
			__asmeq("%5", R1_STR)
			__asmeq("%6", R2_STR)
			__asmeq("%7", R3_STR)
			__asmeq("%8", R4_STR)
			__asmeq("%9", R5_STR)
			__asmeq("%10", R6_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
			"smc	#0\n"
			: "=r" (r0), "=r" (r1), "=r" (r2), "=r" (r3)
			: "r" (r0), "r" (r1), "r" (r2), "r" (r3), "r" (r4),
			 "r" (r5), "r" (r6));

	} while (r0 == QCOM_SCM_INTERRUPTED);

	if (ret1)
		*ret1 = r1;
	if (ret2)
		*ret2 = r2;
	if (ret3)
		*ret3 = r3;

	return r0;
}

static enum scm_interface_version {
	SCM_UNKNOWN,
	SCM_LEGACY,
	SCM_ARMV8_32,
} scm_version = SCM_UNKNOWN;

/* This function is used to find whether TZ is in AARCH64 mode.
 * If this function returns 1, then its in AARCH64 mode and
 * calling conventions for AARCH64 TZ is different, we need to
 * use them.
 */
static bool is_scm_armv8(void)
{
	int ret;
	u64 ret1, x0;

	if (likely(scm_version != SCM_UNKNOWN))
		return (scm_version == SCM_ARMV8_32);

	/* Try SMC32 call */
	ret1 = 0;
	x0 = SCM_SIP_FNID(QCOM_SCM_SVC_INFO, QCOM_IS_CALL_AVAIL_CMD) |
			QCOM_SMC_ATOMIC_MASK;

	ret = __scm_call_armv8_32(x0, SCM_ARGS(1), x0, 0, 0, 0,
				  &ret1, NULL, NULL);
	if (ret || !ret1)
		scm_version = SCM_LEGACY;
	else
		scm_version = SCM_ARMV8_32;

	pr_debug("scm_call: scm version is %x\n", scm_version);

	return (scm_version == SCM_ARMV8_32);
}

/**
 * qcom_scm_call2() - Invoke a syscall in the secure world
 * @fn_id: The function ID for this syscall
 * @desc: Descriptor structure containing arguments and return values
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This should *only* be called in pre-emptible context.
 *
 */
static int qcom_scm_call2(u32 fn_id, struct scm_desc *desc)
{
	int ret, retry_count = 0;
	u64 x0;

	if (unlikely(!is_scm_armv8()))
		return -ENODEV;

	x0 = fn_id;

	do {
		mutex_lock(&qcom_scm_lock);

		desc->ret[0] = desc->ret[1] = desc->ret[2] = 0;

		ret = __scm_call_armv8_32(x0, desc->arginfo,
					  desc->args[0], desc->args[1],
					  desc->args[2], desc->x5,
					  &desc->ret[0], &desc->ret[1],
					  &desc->ret[2]);
		mutex_unlock(&qcom_scm_lock);

		if (ret == QCOM_SCM_V2_EBUSY)
			msleep(QCOM_SCM_EBUSY_WAIT_MS);
	}  while (ret == QCOM_SCM_V2_EBUSY &&
			(retry_count++ < QCOM_SCM_EBUSY_MAX_RETRY));

	if (ret < 0)
		pr_err("scm_call failed: func id %#llx ret: %d"
			" syscall returns: %#llx, %#llx, %#llx\n",
			x0, ret, desc->ret[0],
			desc->ret[1], desc->ret[2]);

	if (ret < 0)
		return qcom_scm_remap_error(ret);
	return 0;
}

u32 qcom_scm_get_version(void)
{
	int context_id;
	static u32 version = -1;
	register u32 r0 asm("r0");
	register u32 r1 asm("r1");

	if (version != -1)
		return version;

	mutex_lock(&qcom_scm_lock);

	r0 = 0x1 << 8;
	r1 = (u32)&context_id;
	do {
		asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r1")
			__asmeq("%2", "r0")
			__asmeq("%3", "r1")
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
			"smc	#0	@ switch to secure world\n"
			: "=r" (r0), "=r" (r1)
			: "r" (r0), "r" (r1)
			: "r2", "r3");
	} while (r0 == QCOM_SCM_INTERRUPTED);

	version = r1;
	mutex_unlock(&qcom_scm_lock);

	return version;
}
EXPORT_SYMBOL(qcom_scm_get_version);

/**
 * qcom_scm_set_cold_boot_addr() - Set the cold boot address for cpus
 * @entry: Entry point function for the cpus
 * @cpus: The cpumask of cpus that will use the entry point
 *
 * Set the cold boot address of the cpus. Any cpu outside the supported
 * range would be removed from the cpu present mask.
 */
int __qcom_scm_set_cold_boot_addr(void *entry, const cpumask_t *cpus)
{
	int flags = 0;
	int cpu;
	int scm_cb_flags[] = {
		QCOM_SCM_FLAG_COLDBOOT_CPU0,
		QCOM_SCM_FLAG_COLDBOOT_CPU1,
		QCOM_SCM_FLAG_COLDBOOT_CPU2,
		QCOM_SCM_FLAG_COLDBOOT_CPU3,
	};

	if (!cpus || (cpus && cpumask_empty(cpus)))
		return -EINVAL;

	for_each_cpu(cpu, cpus) {
		if (cpu < ARRAY_SIZE(scm_cb_flags))
			flags |= scm_cb_flags[cpu];
		else
			set_cpu_present(cpu, false);
	}

	return qcom_scm_call_atomic2(QCOM_SCM_SVC_BOOT, QCOM_SCM_BOOT_ADDR,
				    flags, virt_to_phys(entry));
}

/**
 * qcom_scm_set_warm_boot_addr() - Set the warm boot address for cpus
 * @entry: Entry point function for the cpus
 * @cpus: The cpumask of cpus that will use the entry point
 *
 * Set the Linux entry point for the SCM to transfer control to when coming
 * out of a power down. CPU power down may be executed on cpuidle or hotplug.
 */
int __qcom_scm_set_warm_boot_addr(struct device *dev, void *entry,
				  const cpumask_t *cpus)
{
	int ret;
	int flags = 0;
	int cpu;
	struct {
		__le32 flags;
		__le32 addr;
	} cmd;

	/*
	 * Reassign only if we are switching from hotplug entry point
	 * to cpuidle entry point or vice versa.
	 */
	for_each_cpu(cpu, cpus) {
		if (entry == qcom_scm_wb[cpu].entry)
			continue;
		flags |= qcom_scm_wb[cpu].flag;
	}

	/* No change in entry function */
	if (!flags)
		return 0;

	cmd.addr = cpu_to_le32(virt_to_phys(entry));
	cmd.flags = cpu_to_le32(flags);
	ret = qcom_scm_call(dev, QCOM_SCM_SVC_BOOT, QCOM_SCM_BOOT_ADDR,
			    &cmd, sizeof(cmd), NULL, 0);
	if (!ret) {
		for_each_cpu(cpu, cpus)
			qcom_scm_wb[cpu].entry = entry;
	}

	return ret;
}

/**
 * qcom_scm_cpu_power_down() - Power down the cpu
 * @flags - Flags to flush cache
 *
 * This is an end point to power down cpu. If there was a pending interrupt,
 * the control would return from this function, otherwise, the cpu jumps to the
 * warm boot entry point set for this cpu upon reset.
 */
void __qcom_scm_cpu_power_down(u32 flags)
{
	qcom_scm_call_atomic1(QCOM_SCM_SVC_BOOT, QCOM_SCM_CMD_TERMINATE_PC,
			flags & QCOM_SCM_FLUSH_FLAG_MASK);
}

int __qcom_scm_is_call_available(struct device *dev, u32 svc_id, u32 cmd_id)
{
	int ret;
	__le32 svc_cmd = cpu_to_le32((svc_id << 10) | cmd_id);
	__le32 ret_val = 0;

	ret = qcom_scm_call(dev, QCOM_SCM_SVC_INFO, QCOM_IS_CALL_AVAIL_CMD,
			    &svc_cmd, sizeof(svc_cmd), &ret_val,
			    sizeof(ret_val));
	if (ret)
		return ret;

	return le32_to_cpu(ret_val);
}

int __qcom_scm_hdcp_req(struct device *dev, struct qcom_scm_hdcp_req *req,
			u32 req_cnt, u32 *resp)
{
	if (req_cnt > QCOM_SCM_HDCP_MAX_REQ_CNT)
		return -ERANGE;

	return qcom_scm_call(dev, QCOM_SCM_SVC_HDCP, QCOM_SCM_CMD_HDCP,
		req, req_cnt * sizeof(*req), resp, sizeof(*resp));
}

int __qcom_scm_regsave(struct device *dev,
				u32 svc_id, u32 cmd_id, void *scm_regsave)
{
	long ret;
	struct {
		unsigned addr;
		int len;
	} cmd_buf;

	if (!scm_regsave)
		return -EINVAL;

	if (is_scm_armv8()) {
		__le32 scm_ret;
		struct scm_desc desc = {0};

		desc.args[0] = (u64)virt_to_phys(scm_regsave);
		desc.args[1] = PAGE_SIZE;
		desc.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);
		ret = qcom_scm_call2(SCM_SIP_FNID(QCOM_SCM_SVC_REGSAVE,
				QCOM_SCM_REGSAVE_CMD), &desc);
		scm_ret = desc.ret[0];
		if (!ret)
			return le32_to_cpu(scm_ret);
	} else {
		cmd_buf.addr = virt_to_phys(scm_regsave);
		cmd_buf.len = PAGE_SIZE;
		ret = qcom_scm_call(dev, svc_id, cmd_id, &cmd_buf,
				sizeof(cmd_buf), NULL, 0);
	}

	return ret;
}

void __qcom_scm_init(void)
{
}

bool __qcom_scm_pas_supported(struct device *dev, u32 peripheral)
{
	__le32 out;
	__le32 in;
	int ret;

	in = cpu_to_le32(peripheral);
	ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL,
			    QCOM_SCM_PAS_IS_SUPPORTED_CMD,
			    &in, sizeof(in),
			    &out, sizeof(out));

	return ret ? false : !!out;
}

int __qcom_scm_pas_init_image(struct device *dev, u32 peripheral,
			      dma_addr_t metadata_phys)
{
	__le32 scm_ret;
	int ret;
	struct {
		__le32 proc;
		__le32 image_addr;
	} request;
	struct scm_desc desc = {0};

	if (!is_scm_armv8()) {
		request.proc = cpu_to_le32(peripheral);
		request.image_addr = cpu_to_le32(metadata_phys);
		ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL,
			    QCOM_SCM_PAS_INIT_IMAGE_CMD,
			    &request, sizeof(request),
			    &scm_ret, sizeof(scm_ret));
	} else {
		desc.args[0] = peripheral;
		desc.args[1] = metadata_phys;
		desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_RW);
		ret = qcom_scm_call2(SCM_SIP_FNID(QCOM_SCM_SVC_PIL,
				QCOM_SCM_PAS_INIT_IMAGE_CMD), &desc);
		scm_ret = desc.ret[0];
	}

	return ret ? : le32_to_cpu(scm_ret);
}

int __qcom_scm_pas_mem_setup(struct device *dev, u32 peripheral,
			     phys_addr_t addr, phys_addr_t size)
{
	__le32 scm_ret;
	int ret;
	struct {
		__le32 proc;
		__le32 addr;
		__le32 len;
	} request;

	request.proc = cpu_to_le32(peripheral);
	request.addr = cpu_to_le32(addr);
	request.len = cpu_to_le32(size);

	ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL,
			    QCOM_SCM_PAS_MEM_SETUP_CMD,
			    &request, sizeof(request),
			    &scm_ret, sizeof(scm_ret));

	return ret ? : le32_to_cpu(scm_ret);
}

int __qcom_scm_pas_auth_and_reset(struct device *dev, u32 peripheral)
{
	__le32 out;
	__le32 in;
	int ret;
	struct scm_desc desc = {0};

	if (!is_scm_armv8()) {
		in = cpu_to_le32(peripheral);
		ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL,
			    QCOM_SCM_PAS_AUTH_AND_RESET_CMD,
			    &in, sizeof(in),
			    &out, sizeof(out));
	} else {
		desc.args[0] = peripheral;
		desc.arginfo = SCM_ARGS(1);
		ret = qcom_scm_call2(SCM_SIP_FNID(QCOM_SCM_SVC_PIL,
				QCOM_SCM_PAS_AUTH_AND_RESET_CMD), &desc);
		out = desc.ret[0];
	}
	return ret ? : le32_to_cpu(out);
}

int __qcom_scm_pas_shutdown(struct device *dev, u32 peripheral)
{
	__le32 out;
	__le32 in;
	int ret;

	in = cpu_to_le32(peripheral);
	ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL,
			    QCOM_SCM_PAS_SHUTDOWN_CMD,
			    &in, sizeof(in),
			    &out, sizeof(out));

	return ret ? : le32_to_cpu(out);
}

int __qcom_scm_pas_mss_reset(struct device *dev, bool reset)
{
	__le32 out;
	__le32 in = cpu_to_le32(reset);
	int ret;

	ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL, QCOM_SCM_PAS_MSS_RESET,
			&in, sizeof(in),
			&out, sizeof(out));

	return ret ? : le32_to_cpu(out);
}

int __qcom_scm_tcsr(struct device *dev, u32 svc_id, u32 cmd_id,
			struct qcom_scm_tcsr_req *tcsr_cmd)
{
	long ret;

	ret = qcom_scm_call(dev, svc_id, cmd_id, tcsr_cmd,
				sizeof(*tcsr_cmd), NULL, 0);

	return ret;
}

static int __qcom_scm_dload_v8(struct device *dev, void *cmd_buf)
{
	struct scm_desc desc = {0};
	int ret;
	unsigned int enable;

#define TCSR_BOOT_MISC_REG		0x193d100ull
#define DLOAD_MODE_ENABLE		0x10ull
#define DLOAD_MODE_DISABLE		0x00ull

	enable = cmd_buf ? *((unsigned int *)cmd_buf) : 0;
	desc.args[0] = TCSR_BOOT_MISC_REG;
	desc.args[1] = enable ? DLOAD_MODE_ENABLE : DLOAD_MODE_DISABLE;
	desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);
	ret = qcom_scm_call2(SCM_SIP_FNID(SCM_SVC_IO_ACCESS,
					SCM_IO_WRITE), &desc);
	if (ret)
		return ret;

	return le32_to_cpu(desc.ret[0]);
}

int __qcom_scm_dload(struct device *dev, u32 svc_id, u32 cmd_id, void *cmd_buf)
{
	long ret;

	if (is_scm_armv8())
		return __qcom_scm_dload_v8(dev, cmd_buf);

	if (cmd_buf)
		ret = qcom_scm_call(dev, svc_id, cmd_id, cmd_buf,
				sizeof(cmd_buf), NULL, 0);
	else
		ret = qcom_scm_call(dev, svc_id, cmd_id, NULL, 0, NULL, 0);

	return ret;
}

static int __qcom_scm_sdi_v8(struct device *dev)
{
	struct scm_desc desc = {0};
	int ret;

	desc.args[0] = 1ull;	/* Disable wdog debug */
	desc.args[1] = 0ull;	/* SDI Enable */
	desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);
	ret = qcom_scm_call2(SCM_SIP_FNID(QCOM_SCM_SVC_BOOT,
				SCM_CMD_TZ_CONFIG_HW_FOR_RAM_DUMP_ID), &desc);

	if (ret)
		return ret;

	return le32_to_cpu(desc.ret[0]);
}

int __qcom_scm_sdi(struct device *dev, u32 svc_id, u32 cmd_id)
{
	long ret;
	unsigned int clear_info[] = {
		1 /* Disable wdog debug */, 0 /* SDI enable*/, };

	if (is_scm_armv8())
		return __qcom_scm_sdi_v8(dev);

	ret = qcom_scm_call(dev, svc_id, cmd_id, &clear_info,
				sizeof(clear_info), NULL, 0);

	return ret;
}

int __qcom_scm_tzsched(struct device *dev, const void *req,
			size_t req_size, void *resp, size_t resp_size)
{
	int ret;

	ret = qcom_scm_call(dev, SCM_SVC_TZSCHEDULER, 1, req,
				req_size, resp, resp_size);

	return ret;
}

int __qcom_scm_pinmux_read(u32 svc_id, u32 cmd_id, u32 arg1)
{
	s32 ret;

	ret = qcom_scm_call_atomic1(svc_id, cmd_id, arg1);

	return ret;
}

int __qcom_scm_pinmux_write(u32 svc_id, u32 cmd_id, u32 arg1, u32 arg2)
{
	s32 ret;

	ret = qcom_scm_call_atomic2(svc_id, cmd_id, arg1, arg2);

	return ret;
}

int __qcom_scm_cache_dump(u32 cpu)
{
	long ret;

	ret = qcom_scm_call_atomic1(SCM_SVC_UTIL, SCM_CMD_CACHE_BUFFER_DUMP,
									cpu);
	return ret;
}

int __qcom_scm_get_cache_dump_size(struct device *dev, u32 cmd_id,
						void *cmd_buf, u32 size)
{
	long ret;

	ret = qcom_scm_call(dev, SCM_SVC_UTIL, cmd_id, NULL, 0, cmd_buf, size);

	return ret;
}

int __qcom_scm_send_cache_dump_addr(struct device *dev, u32 cmd_id,
						void *cmd_buf, u32 size)
{
	long ret;

	ret = qcom_scm_call(dev, SCM_SVC_UTIL, cmd_id, cmd_buf, size, NULL, 0);

	return ret;
}

static int __qcom_scm_tz_log_v8(struct device *dev, u32 svc_id, u32 cmd_id,
						u32 log_buf, u32 buf_size)
{
	struct scm_desc desc = {0};
	int ret;

	desc.args[0] = log_buf;
	desc.args[1] = buf_size;
	desc.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);

	ret = qcom_scm_call2(SCM_SIP_FNID(svc_id, cmd_id), &desc);

	if (ret)
		return ret;

	return le32_to_cpu(desc.ret[0]);
}

int __qcom_scm_tz_log(struct device *dev, u32 svc_id, u32 cmd_id,
				void *ker_buf, u32 *buf_len, u32 **ring_off,
				struct tzbsp_diag_log_t **log)
{
	int ret;
	struct log_read rdip;
	struct tzbsp_diag_t *tz_diag;
	struct tzbsp_diag_t_v8 *tz_diag_v8;
	dma_addr_t log_buf;

	if (is_scm_armv8()) {

		log_buf = dma_map_single(dev, ker_buf, BUF_LEN_V8,
						DMA_FROM_DEVICE);
		ret = dma_mapping_error(dev, log_buf);

		if (ret != 0) {
			pr_err("DMA Mapping Error : %d\n", ret);
			return -EINVAL;
		}

		ret = __qcom_scm_tz_log_v8(dev, svc_id, cmd_id,
						log_buf, BUF_LEN_V8);
		dma_unmap_single(dev, log_buf, BUF_LEN_V8, DMA_FROM_DEVICE);

		if (ret == 0) {
			tz_diag_v8 = (struct tzbsp_diag_t_v8 *) ker_buf;
			*ring_off = (uint32_t *)&(tz_diag_v8->ring_off);
			*log = (struct tzbsp_diag_log_t *) &tz_diag_v8->log;
			*buf_len = BUF_LEN_V8;
		}
	} else {

		rdip.buf_size = BUF_LEN_V7;
		rdip.log_buf = dma_map_single(dev, ker_buf, BUF_LEN_V7,
						DMA_FROM_DEVICE);
		ret = dma_mapping_error(dev, rdip.log_buf);

		if (ret != 0) {
			pr_err("DMA Mapping Error : %d\n", ret);
			return -EINVAL;
		}

		ret = qcom_scm_call(dev, svc_id, cmd_id, &rdip,
					sizeof(struct log_read), NULL, 0);
		dma_unmap_single(dev, rdip.log_buf, BUF_LEN_V7,
						DMA_FROM_DEVICE);

		if (ret == 0) {
			tz_diag = (struct tzbsp_diag_t *) ker_buf;
			*ring_off = (uint32_t *) &(tz_diag->ring_off);
			*log = (struct tzbsp_diag_log_t *) &tz_diag->log;
			*buf_len = BUF_LEN_V7;
		}
	}

	return ret;
}

int __qcom_los_scm_call(struct device *dev, u32 svc_id, u32 cmd_id,
					void *cmd_buf, size_t size)
{
	long ret;

	ret = qcom_scm_call(dev, svc_id, cmd_id, cmd_buf, size, NULL, 0);

	return ret;
}
