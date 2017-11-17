/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/device.h>
#include <linux/sysfs.h>

#include "wil6210.h"
#include "wmi.h"

static ssize_t
wil_ftm_txrx_offset_sysfs_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	struct {
		struct wmi_cmd_hdr wmi;
		struct wmi_tof_get_tx_rx_offset_event evt;
	} __packed reply;
	int rc;
	ssize_t len;

	if (!test_bit(WMI_FW_CAPABILITY_FTM, wil->fw_capabilities))
		return -EOPNOTSUPP;

	memset(&reply, 0, sizeof(reply));
	rc = wmi_call(wil, WMI_TOF_GET_TX_RX_OFFSET_CMDID, NULL, 0,
		      WMI_TOF_GET_TX_RX_OFFSET_EVENTID,
		      &reply, sizeof(reply), 100);
	if (rc < 0)
		return rc;
	if (reply.evt.status) {
		wil_err(wil, "get_tof_tx_rx_offset failed, error %d\n",
			reply.evt.status);
		return -EIO;
	}
	len = snprintf(buf, PAGE_SIZE, "%u %u\n",
		       le32_to_cpu(reply.evt.tx_offset),
		       le32_to_cpu(reply.evt.rx_offset));
	return len;
}

static ssize_t
wil_ftm_txrx_offset_sysfs_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	struct wmi_tof_set_tx_rx_offset_cmd cmd;
	struct {
		struct wmi_cmd_hdr wmi;
		struct wmi_tof_set_tx_rx_offset_event evt;
	} __packed reply;
	unsigned int tx_offset, rx_offset;
	int rc;

	if (sscanf(buf, "%u %u", &tx_offset, &rx_offset) != 2)
		return -EINVAL;

	if (!test_bit(WMI_FW_CAPABILITY_FTM, wil->fw_capabilities))
		return -EOPNOTSUPP;

	memset(&cmd, 0, sizeof(cmd));
	cmd.tx_offset = cpu_to_le32(tx_offset);
	cmd.rx_offset = cpu_to_le32(rx_offset);
	memset(&reply, 0, sizeof(reply));
	rc = wmi_call(wil, WMI_TOF_SET_TX_RX_OFFSET_CMDID, &cmd, sizeof(cmd),
		      WMI_TOF_SET_TX_RX_OFFSET_EVENTID,
		      &reply, sizeof(reply), 100);
	if (rc < 0)
		return rc;
	if (reply.evt.status) {
		wil_err(wil, "set_tof_tx_rx_offset failed, error %d\n",
			reply.evt.status);
		return -EIO;
	}
	return count;
}

static DEVICE_ATTR(ftm_txrx_offset, 0644,
		   wil_ftm_txrx_offset_sysfs_show,
		   wil_ftm_txrx_offset_sysfs_store);

static ssize_t
wil_board_file_sysfs_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	ssize_t len;

	len = snprintf(buf, PAGE_SIZE, "%s\n", wil_get_board_file(wil));
	return len;
}

static ssize_t
wil_board_file_sysfs_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	size_t len;

	mutex_lock(&wil->mutex);

	kfree(wil->board_file);
	wil->board_file = NULL;

	len = count;
	if (buf[count - 1] == '\n')
		len--;
	len = strnlen(buf, len);
	if (len > 0) {
		wil->board_file = kmalloc(len + 1, GFP_KERNEL);
		if (!wil->board_file) {
			mutex_unlock(&wil->mutex);
			return -ENOMEM;
		}
		strlcpy(wil->board_file, buf, len + 1);
	}
	mutex_unlock(&wil->mutex);

	return count;
}

static DEVICE_ATTR(board_file, 0644,
		   wil_board_file_sysfs_show,
		   wil_board_file_sysfs_store);

static ssize_t
wil_fst_link_loss_sysfs_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	ssize_t len = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(wil->sta); i++)
		if (wil->sta[i].status == wil_sta_connected)
			len += snprintf(buf + len, PAGE_SIZE - len,
					"[%d] %pM %s\n", i, wil->sta[i].addr,
					wil->sta[i].fst_link_loss ?
					"On" : "Off");

	return len;
}

static ssize_t
wil_fst_link_loss_sysfs_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	u8 addr[ETH_ALEN];
	char *token, *dupbuf, *tmp;
	int rc = -EINVAL;
	bool fst_link_loss;

	tmp = kmemdup(buf, count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	tmp[count] = '\0';
	dupbuf = tmp;

	token = strsep(&dupbuf, " ");
	if (!token)
		goto out;

	/* mac address */
	if (sscanf(token, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		   &addr[0], &addr[1], &addr[2],
		   &addr[3], &addr[4], &addr[5]) != 6)
		goto out;

	/* On/Off */
	if (strtobool(dupbuf, &fst_link_loss))
		goto out;

	wil_dbg_misc(wil, "set [%pM] with %d\n", addr, fst_link_loss);

	rc = wmi_link_maintain_cfg_write(wil, addr, fst_link_loss);
	if (!rc)
		rc = count;

out:
	kfree(tmp);
	return rc;
}

static DEVICE_ATTR(fst_link_loss, 0644,
		   wil_fst_link_loss_sysfs_show,
		   wil_fst_link_loss_sysfs_store);

static struct attribute *wil6210_sysfs_entries[] = {
	&dev_attr_ftm_txrx_offset.attr,
	&dev_attr_board_file.attr,
	&dev_attr_fst_link_loss.attr,
	NULL
};

static struct attribute_group wil6210_attribute_group = {
	.name = "wil6210",
	.attrs = wil6210_sysfs_entries,
};

int wil6210_sysfs_init(struct wil6210_priv *wil)
{
	struct device *dev = wil_to_dev(wil);
	int err;

	err = sysfs_create_group(&dev->kobj, &wil6210_attribute_group);
	if (err) {
		wil_err(wil, "failed to create sysfs group: %d\n", err);
		return err;
	}

	return 0;
}

void wil6210_sysfs_remove(struct wil6210_priv *wil)
{
	struct device *dev = wil_to_dev(wil);

	sysfs_remove_group(&dev->kobj, &wil6210_attribute_group);
}
