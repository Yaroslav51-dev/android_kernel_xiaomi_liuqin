// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 47247 $
 * $Date: 2019-07-10 10:41:36 +0800 (Wed, 10 Jul 2019) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#if !defined(NVT_NT36XXX_SPI) /* TOUCHSCREEN_NT36XXX I2C */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "nt36xxx.h"

#if NVT_TOUCH_EXT_PROC
#define NVT_FW_VERSION "nvt_fw_version"
#define NVT_BASELINE "nvt_baseline"
#define NVT_RAW "nvt_raw"
#define NVT_DIFF "nvt_diff"

#define BUS_TRANSFER_LENGTH  64

#define NORMAL_MODE 0x00
#define TEST_MODE_1 0x21
#define TEST_MODE_2 0x22
#define HANDSHAKING_HOST_READY 0xBB

#define XDATA_SECTOR_SIZE   256

static uint8_t xdata_tmp[2048] = {0};
static int32_t xdata[2048] = {0};

static struct proc_dir_entry *NVT_proc_fw_version_entry;
static struct proc_dir_entry *NVT_proc_baseline_entry;
static struct proc_dir_entry *NVT_proc_raw_entry;
static struct proc_dir_entry *NVT_proc_diff_entry;

/*******************************************************
Description:
	Novatek touchscreen change mode function.

return:
	n.a.
*******************************************************/
void nvt_change_mode(uint8_t mode)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = mode;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

	if (mode == NORMAL_MODE) {
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = HANDSHAKING_HOST_READY;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
		msleep(20);
	}
}

/*******************************************************
Description:
	Novatek touchscreen get firmware pipe function.

return:
	Executive outcomes. 0---pipe 0. 1---pipe 1.
*******************************************************/
uint8_t nvt_get_fw_pipe(void)
{
	uint8_t buf[8]= {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

	//---read fw status---
	buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
	buf[1] = 0x00;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

	//NVT_LOG("FW pipe=%d, buf[1]=0x%02X\n", (buf[1]&0x01), buf[1]);

	return (buf[1] & 0x01);
}

/*******************************************************
Description:
	Novatek touchscreen read meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_mdata(uint32_t xdata_addr, uint32_t xdata_btn_addr)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 1] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = ts->x_num * ts->y_num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_set_page(I2C_FW_Address, head_addr + XDATA_SECTOR_SIZE * i);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (XDATA_SECTOR_SIZE / BUS_TRANSFER_LENGTH); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(I2C_FW_Address, xdata_addr + data_len - residual_len);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (residual_len / BUS_TRANSFER_LENGTH + 1); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		xdata[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);
	}

#if TOUCH_KEY_NUM > 0
	//read button xdata : step3
	//---change xdata index---
	nvt_set_page(I2C_FW_Address, xdata_btn_addr);

	//---read data---
	buf[0] = (xdata_btn_addr & 0xFF);
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, (TOUCH_KEY_NUM * 2 + 1));

	//---2bytes-to-1data---
	for (i = 0; i < TOUCH_KEY_NUM; i++) {
		xdata[ts->x_num * ts->y_num + i] = (int16_t)(buf[1 + i * 2] + 256 * buf[1 + i * 2 + 1]);
	}
#endif

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR);
}

/*******************************************************
Description:
    Novatek touchscreen get meta data function.

return:
    n.a.
*******************************************************/
void nvt_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num)
{
    *m_x_num = ts->x_num;
    *m_y_num = ts->y_num;
    memcpy(buf, xdata, ((ts->x_num * ts->y_num + TOUCH_KEY_NUM) * sizeof(int32_t)));
}

/*******************************************************
Description:
	Novatek touchscreen firmware version show function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_fw_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "fw_ver=%d, x_num=%d, y_num=%d, button_num=%d\n", ts->fw_ver, ts->x_num, ts->y_num, ts->max_button_num);
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_show(struct seq_file *m, void *v)
{
	int32_t i = 0;
	int32_t j = 0;

	for (i = 0; i < ts->y_num; i++) {
		for (j = 0; j < ts->x_num; j++) {
			seq_printf(m, "%5d, ", xdata[i * ts->x_num + j]);
		}
		seq_puts(m, "\n");
	}

#if TOUCH_KEY_NUM > 0
	for (i = 0; i < TOUCH_KEY_NUM; i++) {
		seq_printf(m, "%5d, ", xdata[ts->x_num * ts->y_num + i]);
	}
	seq_puts(m, "\n");
#endif

	seq_printf(m, "\n\n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print start
	function.

return:
	Executive outcomes. 1---call next function.
	NULL---not call next function and sequence loop
	stop.
*******************************************************/
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print next
	function.

return:
	Executive outcomes. NULL---no next and call sequence
	stop function.
*******************************************************/
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print stop
	function.

return:
	n.a.
*******************************************************/
static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations nvt_fw_version_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_fw_version_show
};

const struct seq_operations nvt_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_fw_version open
	function.

return:
	n.a.
*******************************************************/
static int32_t nvt_fw_version_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_fw_version_seq_ops);
}

static const struct proc_ops nvt_fw_version_fops = {
	.proc_open = nvt_fw_version_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_baseline open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_baseline_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_read_mdata(ts->mmap->BASELINE_ADDR, ts->mmap->BASELINE_BTN_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

static const struct proc_ops nvt_baseline_fops = {
	.proc_open = nvt_baseline_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_raw open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_raw_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR, ts->mmap->RAW_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR, ts->mmap->RAW_BTN_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

static const struct proc_ops nvt_raw_fops = {
	.proc_open = nvt_raw_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_diff open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_diff_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

static const struct proc_ops nvt_diff_fops = {
	.proc_open = nvt_diff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
int32_t nvt_extra_proc_init(void)
{
	NVT_proc_fw_version_entry = proc_create(NVT_FW_VERSION, 0444, NULL,&nvt_fw_version_fops);
	if (NVT_proc_fw_version_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_FW_VERSION);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_FW_VERSION);
	}

	NVT_proc_baseline_entry = proc_create(NVT_BASELINE, 0444, NULL,&nvt_baseline_fops);
	if (NVT_proc_baseline_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_BASELINE);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_BASELINE);
	}

	NVT_proc_raw_entry = proc_create(NVT_RAW, 0444, NULL,&nvt_raw_fops);
	if (NVT_proc_raw_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_RAW);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_RAW);
	}

	NVT_proc_diff_entry = proc_create(NVT_DIFF, 0444, NULL,&nvt_diff_fops);
	if (NVT_proc_diff_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_DIFF);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_DIFF);
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	deinitial function.

return:
	n.a.
*******************************************************/
void nvt_extra_proc_deinit(void)
{
	if (NVT_proc_fw_version_entry != NULL) {
		remove_proc_entry(NVT_FW_VERSION, NULL);
		NVT_proc_fw_version_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_FW_VERSION);
	}

	if (NVT_proc_baseline_entry != NULL) {
		remove_proc_entry(NVT_BASELINE, NULL);
		NVT_proc_baseline_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_BASELINE);
	}

	if (NVT_proc_raw_entry != NULL) {
		remove_proc_entry(NVT_RAW, NULL);
		NVT_proc_raw_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_RAW);
	}

	if (NVT_proc_diff_entry != NULL) {
		remove_proc_entry(NVT_DIFF, NULL);
		NVT_proc_diff_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_DIFF);
	}
}
#endif

#else  /* NT36XXX_SPI */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "nt36xxx.h"

#if NVT_SPI_TOUCH_EXT_PROC
#define NVT_SPI_FW_VERSION "nvt_fw_version"
#define NVT_SPI_BASELINE "nvt_baseline"
#define NVT_SPI_RAW "nvt_raw"
#define NVT_SPI_DIFF "nvt_diff"
#define NVT_SPI_PEN_DIFF "nvt_pen_diff"

#define NVT_SPI_BUS_TRANSFER_LENGTH 256

#define NVT_SPI_NORMAL_MODE 0x00
#define NVT_SPI_TEST_MODE_2 0x22
#define NVT_SPI_HANDSHAKING_HOST_READY 0xBB

#define NVT_SPI_XDATA_SECTOR_SIZE 256

static uint8_t nvt_spi_xdata_tmp[5000] = {0};
static int32_t nvt_spi_xdata[2500] = {0};
static int32_t nvt_spi_xdata_pen_tip_x[256] = {0};
static int32_t nvt_spi_xdata_pen_tip_y[256] = {0};
static int32_t nvt_spi_xdata_pen_ring_x[256] = {0};
static int32_t nvt_spi_xdata_pen_ring_y[256] = {0};

static struct proc_dir_entry *nvt_spi_proc_fw_version_entry;
static struct proc_dir_entry *nvt_spi_proc_baseline_entry;
static struct proc_dir_entry *nvt_spi_proc_raw_entry;
static struct proc_dir_entry *nvt_spi_proc_diff_entry;
static struct proc_dir_entry *nvt_spi_proc_pen_diff_entry;

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen change mode function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
void nvt_spi_change_mode(uint8_t mode)
{
	uint8_t buf[8] = {0};
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---set xdata index to EVENT BUF ADDR---
	nvt_spi_set_page(ts->mmap->EVENT_BUF_ADDR | NVT_SPI_EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = NVT_SPI_EVENT_MAP_HOST_CMD;
	buf[1] = mode;
	nvt_spi_write(buf, 2);

	if (mode == NVT_SPI_NORMAL_MODE) {
		usleep_range(20000, 21000);
		buf[0] = NVT_SPI_EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = NVT_SPI_HANDSHAKING_HOST_READY;
		nvt_spi_write(buf, 2);
		usleep_range(20000, 21000);
	}
}

static int32_t nvt_set_pen_inband_mode_1_spi(uint8_t freq_idx, uint8_t x_term)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 5;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---set xdata index to EVENT BUF ADDR---
	nvt_spi_set_page(ts->mmap->EVENT_BUF_ADDR | NVT_SPI_EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = NVT_SPI_EVENT_MAP_HOST_CMD;
	buf[1] = 0xC1;
	buf[2] = 0x02;
	buf[3] = freq_idx;
	buf[4] = x_term;
	nvt_spi_write(buf, 5);

	for (i = 0; i < retry; i++) {
		buf[0] = NVT_SPI_EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		nvt_spi_read(buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 11000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -EIO;
	}

	return 0;
}

static int32_t nvt_spi_set_pen_normal_mode(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 5;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---set xdata index to EVENT BUF ADDR---
	nvt_spi_set_page(ts->mmap->EVENT_BUF_ADDR | NVT_SPI_EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = NVT_SPI_EVENT_MAP_HOST_CMD;
	buf[1] = 0xC1;
	buf[2] = 0x04;
	nvt_spi_write(buf, 3);

	for (i = 0; i < retry; i++) {
		buf[0] = NVT_SPI_EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		nvt_spi_read(buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 11000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -EIO;
	}

	return 0;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen get firmware pipe function.
 *
 * return:
 *	Executive outcomes. 0---pipe 0. 1---pipe 1.
 ******************************************************
 */
uint8_t nvt_spi_get_fw_pipe(void)
{
	uint32_t addr;
	uint8_t buf[8] = {0};
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---set xdata index to EVENT BUF ADDR---
	addr = ts->mmap->EVENT_BUF_ADDR;
	nvt_spi_set_page(addr | NVT_SPI_EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

	//---read fw status---
	buf[0] = NVT_SPI_EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
	buf[1] = 0x00;
	nvt_spi_read(buf, 2);

	//NVT_LOG("FW pipe=%d, buf[1]=0x%02X\n", (buf[1]&0x01), buf[1]);

	return (buf[1] & 0x01);
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen read meta data function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static void nvt_spi_read_mdata(uint32_t xdata_addr, uint32_t xdata_btn_addr)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[NVT_SPI_BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;
	int32_t index, data;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % NVT_SPI_XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = ts->x_num * ts->y_num * 2;
	residual_len = (head_addr + dummy_len + data_len) % NVT_SPI_XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n",
	//		head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / NVT_SPI_XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_spi_set_page(head_addr + NVT_SPI_XDATA_SECTOR_SIZE * i);

		//---read xdata by NVT_SPI_BUS_TRANSFER_LENGTH
		for (j = 0; j < (NVT_SPI_XDATA_SECTOR_SIZE / NVT_SPI_BUS_TRANSFER_LENGTH); j++) {
			//---read data---
			buf[0] = NVT_SPI_BUS_TRANSFER_LENGTH * j;
			nvt_spi_read(buf, NVT_SPI_BUS_TRANSFER_LENGTH + 1);

			//---copy buf to nvt_spi_xdata_tmp---
			for (k = 0; k < NVT_SPI_BUS_TRANSFER_LENGTH; k++) {
				index = NVT_SPI_XDATA_SECTOR_SIZE * i;
				index += NVT_SPI_BUS_TRANSFER_LENGTH * j + k;
				nvt_spi_xdata_tmp[index] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], index);
			}
		}
		//printk("addr=0x%05X\n", (head_addr+NVT_SPI_XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_spi_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by NVT_SPI_BUS_TRANSFER_LENGTH
		for (j = 0; j < (residual_len / NVT_SPI_BUS_TRANSFER_LENGTH + 1); j++) {
			//---read data---
			buf[0] = NVT_SPI_BUS_TRANSFER_LENGTH * j;
			nvt_spi_read(buf, NVT_SPI_BUS_TRANSFER_LENGTH + 1);

			//---copy buf to nvt_spi_xdata_tmp---
			for (k = 0; k < NVT_SPI_BUS_TRANSFER_LENGTH; k++) {
				index = (dummy_len + data_len - residual_len);
				index += NVT_SPI_BUS_TRANSFER_LENGTH * j + k;
				nvt_spi_xdata_tmp[index] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], index);

			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		data = nvt_spi_xdata_tmp[dummy_len + i * 2];
		data += 256 * nvt_spi_xdata_tmp[dummy_len + i * 2 + 1];
		nvt_spi_xdata[i] = (int16_t)data;
	}

#if NVT_SPI_TOUCH_KEY_NUM > 0
	//read button xdata : step3
	//---change xdata index---
	nvt_spi_set_page(xdata_btn_addr);

	//---read data---
	buf[0] = (xdata_btn_addr & 0xFF);
	nvt_spi_read(buf, (NVT_SPI_TOUCH_KEY_NUM * 2 + 1));

	//---2bytes-to-1data---
	for (i = 0; i < NVT_SPI_TOUCH_KEY_NUM; i++)
		nvt_spi_xdata[ts->x_num * ts->y_num + i] =
				(int16_t)(buf[1 + i * 2] + 256 * buf[1 + i * 2 + 1]);
#endif

	//---set xdata index to EVENT BUF ADDR---
	nvt_spi_set_page(ts->mmap->EVENT_BUF_ADDR);
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen get meta data function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
void nvt_spi_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	*m_x_num = ts->x_num;
	*m_y_num = ts->y_num;
	memcpy(buf, nvt_spi_xdata,
		((ts->x_num * ts->y_num + NVT_SPI_TOUCH_KEY_NUM) * sizeof(int32_t)));
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen read and get number of meta data function.
 *
 * return:
 *	n.a.
 *******************************************************
 */
void nvt_spi_read_get_num_mdata(uint32_t xdata_addr, int32_t *buffer, uint32_t num)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[NVT_SPI_BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;
	int32_t index, data;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % NVT_SPI_XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = num * 2;
	residual_len = (head_addr + dummy_len + data_len) % NVT_SPI_XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n",
	//	head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / NVT_SPI_XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_spi_set_page(head_addr + NVT_SPI_XDATA_SECTOR_SIZE * i);

		//---read xdata by NVT_SPI_BUS_TRANSFER_LENGTH
		for (j = 0; j < (NVT_SPI_XDATA_SECTOR_SIZE / NVT_SPI_BUS_TRANSFER_LENGTH); j++) {
			//---read data---
			buf[0] = NVT_SPI_BUS_TRANSFER_LENGTH * j;
			nvt_spi_read(buf, NVT_SPI_BUS_TRANSFER_LENGTH + 1);

			//---copy buf to nvt_spi_xdata_tmp---
			for (k = 0; k < NVT_SPI_BUS_TRANSFER_LENGTH; k++) {
				index = NVT_SPI_XDATA_SECTOR_SIZE * i;
				index += NVT_SPI_BUS_TRANSFER_LENGTH * j + k;
				nvt_spi_xdata_tmp[index] = buf[k + 1];
			}
		}
		//printk("addr=0x%05X\n", (head_addr+NVT_SPI_XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_spi_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by NVT_SPI_BUS_TRANSFER_LENGTH
		for (j = 0; j < (residual_len / NVT_SPI_BUS_TRANSFER_LENGTH + 1); j++) {
			//---read data---
			buf[0] = NVT_SPI_BUS_TRANSFER_LENGTH * j;
			nvt_spi_read(buf, NVT_SPI_BUS_TRANSFER_LENGTH + 1);

			//---copy buf to nvt_spi_xdata_tmp---
			for (k = 0; k < NVT_SPI_BUS_TRANSFER_LENGTH; k++) {
				index = (dummy_len + data_len - residual_len);
				index += NVT_SPI_BUS_TRANSFER_LENGTH * j + k;
				nvt_spi_xdata_tmp[index] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], index));

			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		data = nvt_spi_xdata_tmp[dummy_len + i * 2];
		data += 256 * nvt_spi_xdata_tmp[dummy_len + i * 2 + 1];
		buffer[i] = (int16_t)data;
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_spi_set_page(ts->mmap->EVENT_BUF_ADDR);
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen firmware version show function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 ******************************************************
 */
static int32_t c_fw_version_show(struct seq_file *m, void *v)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	seq_printf(m, "fw_ver=%d, x_num=%d, y_num=%d, button_num=%d\n",
		ts->fw_ver, ts->x_num, ts->y_num, ts->max_button_num);
	return 0;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen xdata sequence print show
 *	function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 ******************************************************
 */
static int32_t c_show(struct seq_file *m, void *v)
{
	int32_t i = 0;
	int32_t j = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	for (i = 0; i < ts->y_num; i++) {
		for (j = 0; j < ts->x_num; j++)
			seq_printf(m, "%5d, ", nvt_spi_xdata[i * ts->x_num + j]);

		seq_puts(m, "\n");
	}

#if NVT_SPI_TOUCH_KEY_NUM > 0
	for (i = 0; i < NVT_SPI_TOUCH_KEY_NUM; i++)
		seq_printf(m, "%5d, ", nvt_spi_xdata[ts->x_num * ts->y_num + i]);

	seq_puts(m, "\n");
#endif

	seq_puts(m, "\n\n");
	return 0;
}

/*
 *******************************************************
 * Description:
 *	Novatek pen 1D diff xdata sequence print show
 *	function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 ******************************************************
 */
static int32_t c_pen_1d_diff_show(struct seq_file *m, void *v)
{
	int32_t i = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	seq_puts(m, "Tip X:\n");
	for (i = 0; i < ts->x_num; i++)
		seq_printf(m, "%5d, ", nvt_spi_xdata_pen_tip_x[i]);

	seq_puts(m, "\n");
	seq_puts(m, "Tip Y:\n");
	for (i = 0; i < ts->y_num; i++)
		seq_printf(m, "%5d, ", nvt_spi_xdata_pen_tip_y[i]);

	seq_puts(m, "\n");
	seq_puts(m, "Ring X:\n");
	for (i = 0; i < ts->x_num; i++)
		seq_printf(m, "%5d, ", nvt_spi_xdata_pen_ring_x[i]);

	seq_puts(m, "\n");
	seq_puts(m, "Ring Y:\n");
	for (i = 0; i < ts->y_num; i++)
		seq_printf(m, "%5d, ", nvt_spi_xdata_pen_ring_y[i]);

	seq_puts(m, "\n");

	seq_puts(m, "\n\n");
	return 0;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen xdata sequence print start
 *	function.
 *
 * return:
 *	Executive outcomes. 1---call next function.
 *	NULL---not call next function and sequence loop
 *	stop.
 *******************************************************
 */
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen xdata sequence print next
 *	function.
 *
 * return:
 *	Executive outcomes. NULL---no next and call sequence
 *	stop function.
 ******************************************************
 */
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen xdata sequence print stop
 *	function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static void c_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations nvt_spi_fw_version_seq_ops = {
	.start = c_start,
	.next  = c_next,
	.stop  = c_stop,
	.show  = c_fw_version_show
};

const struct seq_operations nvt_spi_seq_ops = {
	.start = c_start,
	.next  = c_next,
	.stop  = c_stop,
	.show  = c_show
};

const struct seq_operations nvt_spi_pen_diff_seq_ops = {
	.start = c_start,
	.next  = c_next,
	.stop  = c_stop,
	.show  = c_pen_1d_diff_show
};

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen /proc/nvt_fw_version open
 *	function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static int32_t nvt_spi_fw_version_open(struct inode *inode, struct file *file)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	NVT_LOG("++\n");

#if NVT_SPI_TOUCH_ESD_PROTECT
	nvt_spi_esd_check_enable(false);
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

	if (nvt_spi_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_spi_fw_version_seq_ops);
}

static const struct proc_ops nvt_spi_fw_version_fops = {
	.proc_open = nvt_spi_fw_version_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen /proc/nvt_baseline open function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 ******************************************************
 */
static int32_t nvt_spi_baseline_open(struct inode *inode, struct file *file)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	NVT_LOG("++\n");

#if NVT_SPI_TOUCH_ESD_PROTECT
	nvt_spi_esd_check_enable(false);
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

	if (nvt_spi_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_spi_change_mode(NVT_SPI_TEST_MODE_2);

	if (nvt_spi_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_spi_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_spi_read_mdata(ts->mmap->BASELINE_ADDR, ts->mmap->BASELINE_BTN_ADDR);

	nvt_spi_change_mode(NVT_SPI_NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_spi_seq_ops);
}

static const struct proc_ops nvt_spi_baseline_fops = {
	.proc_open = nvt_spi_baseline_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen /proc/nvt_raw open function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 ******************************************************
 */
static int32_t nvt_spi_raw_open(struct inode *inode, struct file *file)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	NVT_LOG("++\n");

#if NVT_SPI_TOUCH_ESD_PROTECT
	nvt_spi_esd_check_enable(false);
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

	if (nvt_spi_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_spi_change_mode(NVT_SPI_TEST_MODE_2);

	if (nvt_spi_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_spi_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_spi_get_fw_pipe() == 0)
		nvt_spi_read_mdata(ts->mmap->RAW_PIPE0_ADDR, ts->mmap->RAW_BTN_PIPE0_ADDR);
	else
		nvt_spi_read_mdata(ts->mmap->RAW_PIPE1_ADDR, ts->mmap->RAW_BTN_PIPE1_ADDR);

	nvt_spi_change_mode(NVT_SPI_NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_spi_seq_ops);
}

static const struct proc_ops nvt_spi_raw_fops = {
	.proc_open = nvt_spi_raw_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen /proc/nvt_diff open function.
 *
 * return:
 *	Executive outcomes. 0---succeed. negative---failed.
 ******************************************************
 */
static int32_t nvt_spi_diff_open(struct inode *inode, struct file *file)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	NVT_LOG("++\n");

#if NVT_SPI_TOUCH_ESD_PROTECT
	nvt_spi_esd_check_enable(false);
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

	if (nvt_spi_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_spi_change_mode(NVT_SPI_TEST_MODE_2);

	if (nvt_spi_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_spi_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_spi_get_fw_pipe() == 0)
		nvt_spi_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_spi_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);

	nvt_spi_change_mode(NVT_SPI_NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_spi_seq_ops);
}

static const struct proc_ops nvt_spi_diff_fops = {
	.proc_open = nvt_spi_diff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen /proc/nvt_pen_diff open function.
 *
 * return:
 *	Executive outcomes. 0---succeed. negative---failed.
 ******************************************************
 */
static int32_t nvt_spi_pen_diff_open(struct inode *inode, struct file *file)
{
	uint32_t addr;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	NVT_LOG("++\n");

#if NVT_SPI_TOUCH_ESD_PROTECT
	nvt_spi_esd_check_enable(false);
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

	if (nvt_set_pen_inband_mode_1_spi(0xFF, 0x00)) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_spi_check_fw_reset_state(NVT_SPI_RESET_STATE_NORMAL_RUN)) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_spi_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_spi_change_mode(NVT_SPI_TEST_MODE_2);

	if (nvt_spi_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_spi_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	addr = ts->mmap->PEN_1D_DIFF_TIP_X_ADDR;
	nvt_spi_read_get_num_mdata(addr, nvt_spi_xdata_pen_tip_x, ts->x_num);

	addr = ts->mmap->PEN_1D_DIFF_TIP_Y_ADDR;
	nvt_spi_read_get_num_mdata(addr, nvt_spi_xdata_pen_tip_y, ts->y_num);

	addr = ts->mmap->PEN_1D_DIFF_RING_X_ADDR;
	nvt_spi_read_get_num_mdata(addr, nvt_spi_xdata_pen_ring_x, ts->x_num);

	addr = ts->mmap->PEN_1D_DIFF_RING_Y_ADDR;
	nvt_spi_read_get_num_mdata(addr, nvt_spi_xdata_pen_ring_y, ts->y_num);

	nvt_spi_change_mode(NVT_SPI_NORMAL_MODE);

	nvt_spi_set_pen_normal_mode();

	nvt_spi_check_fw_reset_state(NVT_SPI_RESET_STATE_NORMAL_RUN);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_spi_pen_diff_seq_ops);
}

static const struct proc_ops nvt_spi_pen_diff_fops = {
	.proc_open = nvt_spi_pen_diff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen extra function proc. file node
 *	initial function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -12---failed.
 ******************************************************
 */
int32_t nvt_spi_extra_proc_init(void)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	nvt_spi_proc_fw_version_entry = proc_create(NVT_SPI_FW_VERSION, 0444, NULL,
			&nvt_spi_fw_version_fops);
	if (nvt_spi_proc_fw_version_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_SPI_FW_VERSION);
		return -ENOMEM;
	}
	NVT_LOG("create proc/%s Succeeded!\n", NVT_SPI_FW_VERSION);

	nvt_spi_proc_baseline_entry = proc_create(NVT_SPI_BASELINE, 0444, NULL,
			&nvt_spi_baseline_fops);
	if (nvt_spi_proc_baseline_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_SPI_BASELINE);
		return -ENOMEM;
	}
	NVT_LOG("create proc/%s Succeeded!\n", NVT_SPI_BASELINE);

	nvt_spi_proc_raw_entry = proc_create(NVT_SPI_RAW, 0444, NULL, &nvt_spi_raw_fops);
	if (nvt_spi_proc_raw_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_SPI_RAW);
		return -ENOMEM;
	}
	NVT_LOG("create proc/%s Succeeded!\n", NVT_SPI_RAW);

	nvt_spi_proc_diff_entry = proc_create(NVT_SPI_DIFF, 0444, NULL, &nvt_spi_diff_fops);
	if (nvt_spi_proc_diff_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_SPI_DIFF);
		return -ENOMEM;
	}
	NVT_LOG("create proc/%s Succeeded!\n", NVT_SPI_DIFF);

	if (ts->pen_support) {
		nvt_spi_proc_pen_diff_entry = proc_create(NVT_SPI_PEN_DIFF, 0444, NULL,
				&nvt_spi_pen_diff_fops);
		if (nvt_spi_proc_pen_diff_entry == NULL) {
			NVT_ERR("create proc/%s Failed!\n", NVT_SPI_PEN_DIFF);
			return -ENOMEM;
		}
		NVT_LOG("create proc/%s Succeeded!\n", NVT_SPI_PEN_DIFF);
	}

	return 0;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen extra function proc. file node
 *	deinitial function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
void nvt_spi_extra_proc_deinit(void)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	if (nvt_spi_proc_fw_version_entry != NULL) {
		remove_proc_entry(NVT_SPI_FW_VERSION, NULL);
		nvt_spi_proc_fw_version_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_SPI_FW_VERSION);
	}

	if (nvt_spi_proc_baseline_entry != NULL) {
		remove_proc_entry(NVT_SPI_BASELINE, NULL);
		nvt_spi_proc_baseline_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_SPI_BASELINE);
	}

	if (nvt_spi_proc_raw_entry != NULL) {
		remove_proc_entry(NVT_SPI_RAW, NULL);
		nvt_spi_proc_raw_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_SPI_RAW);
	}

	if (nvt_spi_proc_diff_entry != NULL) {
		remove_proc_entry(NVT_SPI_DIFF, NULL);
		nvt_spi_proc_diff_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_SPI_DIFF);
	}

	if (ts->pen_support) {
		if (nvt_spi_proc_pen_diff_entry != NULL) {
			remove_proc_entry(NVT_SPI_PEN_DIFF, NULL);
			nvt_spi_proc_pen_diff_entry = NULL;
			NVT_LOG("Removed /proc/%s\n", NVT_SPI_PEN_DIFF);
		}
	}
}
#endif



#endif
