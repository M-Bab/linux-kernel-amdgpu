/*
 * AMD ALSA SoC PCM Driver
 *
 * Copyright 2016 Advanced Micro Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/err.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include "acp3x.h"

struct i2s_dev_data {
	unsigned int i2s_irq;
	void __iomem *acp3x_base;
	struct snd_pcm_substream *play_stream;
	struct snd_pcm_substream *capture_stream;
};

struct i2s_stream_instance {
	u16 num_pages;
	u16 channels;
	u32 xfer_resolution;
	u32 fmt;
	u32 val;
	struct page *pg;
	void __iomem *acp3x_base;
};

static const struct snd_pcm_hardware acp3x_pcm_hardware_playback = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_BATCH |
		SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.formats = SNDRV_PCM_FMTBIT_S16_LE |  SNDRV_PCM_FMTBIT_S8 |
		   SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S24_LE |
		   SNDRV_PCM_FMTBIT_S32_LE,
	.channels_min = 2,
	.channels_max = 8,
	.rates = SNDRV_PCM_RATE_8000_96000,
	.rate_min = 8000,
	.rate_max = 96000,
	.buffer_bytes_max = PLAYBACK_MAX_NUM_PERIODS * PLAYBACK_MAX_PERIOD_SIZE,
	.period_bytes_min = PLAYBACK_MIN_PERIOD_SIZE,
	.period_bytes_max = PLAYBACK_MAX_PERIOD_SIZE,
	.periods_min = PLAYBACK_MIN_NUM_PERIODS,
	.periods_max = PLAYBACK_MAX_NUM_PERIODS,
};

static const struct snd_pcm_hardware acp3x_pcm_hardware_capture = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_BATCH |
	    SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S8 |
		   SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S24_LE |
		   SNDRV_PCM_FMTBIT_S32_LE,
	.channels_min = 2,
	.channels_max = 2,
	.rates = SNDRV_PCM_RATE_8000_48000,
	.rate_min = 8000,
	.rate_max = 48000,
	.buffer_bytes_max = CAPTURE_MAX_NUM_PERIODS * CAPTURE_MAX_PERIOD_SIZE,
	.period_bytes_min = CAPTURE_MIN_PERIOD_SIZE,
	.period_bytes_max = CAPTURE_MAX_PERIOD_SIZE,
	.periods_min = CAPTURE_MIN_NUM_PERIODS,
	.periods_max = CAPTURE_MAX_NUM_PERIODS,
};

static int acp3x_power_on(void __iomem *acp3x_base, bool on)
{
	u16 val, mask;
	u32 timeout;

	if (on == true) {
		val = 1;
		mask = 0;
	} else {
		val = 0;
		mask = 2;
	}

	rv_writel(val, acp3x_base + mmACP_PGFSM_CONTROL);
	timeout = 0;
	while (true) {
		val = rv_readl(acp3x_base + mmACP_PGFSM_STATUS);
		if ((val & 0x3) == mask)
			break;
		if (timeout > 100) {
			pr_err("ACP3x power state change failure\n");
			return -ENODEV;
		}
		timeout++;
		cpu_relax();
	}
	return 0;
}

static int acp3x_reset(void __iomem *acp3x_base)
{
	u32 val, timeout;

	rv_writel(1, acp3x_base + mmACP_SOFT_RESET);
	timeout = 0;
	while (true) {
		val = rv_readl(acp3x_base + mmACP_SOFT_RESET);
		if ((val & 0x00010001) || timeout > 100) {
			if (val & 0x00010001)
				break;
			return -ENODEV;
		}
		timeout++;
		cpu_relax();
	}

	rv_writel(0, acp3x_base + mmACP_SOFT_RESET);
	timeout = 0;
	while (true) {
		val = rv_readl(acp3x_base + mmACP_SOFT_RESET);
		if (!val || timeout > 100) {
			if (!val)
				break;
			return -ENODEV;
		}
		timeout++;
		cpu_relax();
	}
	return 0;
}

static int acp3x_init(void __iomem *acp3x_base)
{
	int ret;

	/* power on */
	ret = acp3x_power_on(acp3x_base, true);
	if (ret) {
		pr_err("ACP3x power on failed\n");
		return ret;
	}

	/* Reset */
	ret = acp3x_reset(acp3x_base);
	if (ret) {
		pr_err("ACP3x reset failed\n");
		return ret;
	}

	pr_info("ACP Initialized\n");
	return 0;
}

static int acp3x_deinit(void __iomem *acp3x_base)
{
	int ret;

	/* Reset */
	ret = acp3x_reset(acp3x_base);
	if (ret) {
		pr_err("ACP3x reset failed\n");
		return ret;
	}

	/* power off */
	ret = acp3x_power_on(acp3x_base, false);
	if (ret) {
		pr_err("ACP3x power off failed\n");
		return ret;
	}

	pr_info("ACP De-Initialized\n");
	return 0;
}

static irqreturn_t i2s_irq_handler(int irq, void *dev_id)
{
	u16 play_flag, cap_flag;
	u32 val;
	struct i2s_dev_data *rv_i2s_data = dev_id;

	if (rv_i2s_data == NULL)
		return IRQ_NONE;

	play_flag = cap_flag = 0;

	val = rv_readl(rv_i2s_data->acp3x_base + mmACP_EXTERNAL_INTR_STAT);
	if ((val & BIT(BT_TX_THRESHOLD)) && (rv_i2s_data->play_stream)) {
		rv_writel(BIT(BT_TX_THRESHOLD), rv_i2s_data->acp3x_base +
			mmACP_EXTERNAL_INTR_STAT);
		snd_pcm_period_elapsed(rv_i2s_data->play_stream);
		play_flag = 1;
	}

	if ((val & BIT(BT_RX_THRESHOLD)) && rv_i2s_data->capture_stream) {
		rv_writel(BIT(BT_RX_THRESHOLD), rv_i2s_data->acp3x_base +
			mmACP_EXTERNAL_INTR_STAT);
		snd_pcm_period_elapsed(rv_i2s_data->capture_stream);
		cap_flag = 1;
	}

	if (play_flag | cap_flag)
		return IRQ_HANDLED;
	else
		return IRQ_NONE;
}

static void config_acp3x_dma(struct i2s_stream_instance *rtd, int direction)
{
	u16 page_idx;
	u64 addr;
	u32 low, high, val, acp_fifo_addr;
	struct page *pg = rtd->pg;

	/* 8 scratch registers used to map one 64 bit address.
	 * For 2 pages (4096 * 2 bytes), it will be 16 registers.
	 */
	if (direction == SNDRV_PCM_STREAM_PLAYBACK)
		val = 0;
	else
		val = 16;

	/* Group Enable */
	rv_writel(ACP_SRAM_PTE_OFFSET | BIT(31), rtd->acp3x_base +
					mmACPAXI2AXI_ATU_BASE_ADDR_GRP_1);
	rv_writel(PAGE_SIZE_4K_ENABLE, rtd->acp3x_base +
			mmACPAXI2AXI_ATU_PAGE_SIZE_GRP_1);

	for (page_idx = 0; page_idx < rtd->num_pages; page_idx++) {
		/* Load the low address of page int ACP SRAM through SRBM */
		addr = page_to_phys(pg);
		low = lower_32_bits(addr);
		high = upper_32_bits(addr);

		rv_writel(low, rtd->acp3x_base + mmACP_SCRATCH_REG_0 + val);
		high |= BIT(31);
		rv_writel(high, rtd->acp3x_base + mmACP_SCRATCH_REG_0 + val
				+ 4);
		/* Move to next physically contiguos page */
		val += 8;
		pg++;
	}

	if (direction == SNDRV_PCM_STREAM_PLAYBACK) {
		/* Config ringbuffer */
		rv_writel(MEM_WINDOW_START, rtd->acp3x_base +
				mmACP_BT_TX_RINGBUFADDR);
		rv_writel(MAX_BUFFER, rtd->acp3x_base +
				mmACP_BT_TX_RINGBUFSIZE);
		rv_writel(0x40, rtd->acp3x_base + mmACP_BT_TX_DMA_SIZE);

		/* Config audio fifo */
		acp_fifo_addr = ACP_SRAM_PTE_OFFSET + (rtd->num_pages * 8)
				+ 1024;
		rv_writel(acp_fifo_addr, rtd->acp3x_base +
				mmACP_BT_TX_FIFOADDR);
		rv_writel(256, rtd->acp3x_base + mmACP_BT_TX_FIFOSIZE);
		rv_writel(PLAYBACK_MIN_PERIOD_SIZE, rtd->acp3x_base +
				mmACP_BT_TX_INTR_WATERMARK_SIZE);
	} else {
		/* Config ringbuffer */
		rv_writel(MEM_WINDOW_START + MAX_BUFFER, rtd->acp3x_base +
				mmACP_BT_RX_RINGBUFADDR);
		rv_writel(MAX_BUFFER, rtd->acp3x_base +
				mmACP_BT_RX_RINGBUFSIZE);
		rv_writel(0x40, rtd->acp3x_base + mmACP_BT_RX_DMA_SIZE);

		/* Config audio fifo */
		acp_fifo_addr = ACP_SRAM_PTE_OFFSET +
				(rtd->num_pages * 8) + 1024 + 256;
		rv_writel(acp_fifo_addr, rtd->acp3x_base +
				mmACP_BT_RX_FIFOADDR);
		rv_writel(256, rtd->acp3x_base + mmACP_BT_RX_FIFOSIZE);
		rv_writel(CAPTURE_MIN_PERIOD_SIZE, rtd->acp3x_base +
				mmACP_BT_RX_INTR_WATERMARK_SIZE);
	}

	/* Enable  watermark/period interrupt to host */
	rv_writel(BIT(BT_TX_THRESHOLD) | BIT(BT_RX_THRESHOLD),
			rtd->acp3x_base + mmACP_EXTERNAL_INTR_CNTL);
}

static int acp3x_dma_open(struct snd_pcm_substream *substream)
{
	int ret = 0;

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *prtd = substream->private_data;
	struct i2s_dev_data *adata = dev_get_drvdata(prtd->platform->dev);

	struct i2s_stream_instance *i2s_data = kzalloc(sizeof(
				struct i2s_stream_instance), GFP_KERNEL);
	if (i2s_data == NULL)
		return -EINVAL;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		runtime->hw = acp3x_pcm_hardware_playback;
	else
		runtime->hw = acp3x_pcm_hardware_capture;

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_err(prtd->platform->dev, "set integer constraint failed\n");
		return ret;
	}

	if (!adata->play_stream && !adata->capture_stream)
		rv_writel(1, adata->acp3x_base + mmACP_EXTERNAL_INTR_ENB);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		adata->play_stream = substream;
	else
		adata->capture_stream = substream;

	i2s_data->acp3x_base = adata->acp3x_base;
	runtime->private_data = i2s_data;
	return 0;
}

static int acp3x_dma_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	int status;
	uint64_t size;
	struct snd_dma_buffer *dma_buffer;
	struct page *pg;
	struct i2s_stream_instance *rtd = substream->runtime->private_data;

	if (rtd == NULL)
		return -EINVAL;

	dma_buffer = &substream->dma_buffer;
	size = params_buffer_bytes(params);
	status = snd_pcm_lib_malloc_pages(substream, size);
	if (status < 0)
		return status;

	memset(substream->runtime->dma_area, 0, params_buffer_bytes(params));
	pg = virt_to_page(substream->dma_buffer.area);
	if (pg != NULL) {
		rtd->pg = pg;
		rtd->num_pages = (PAGE_ALIGN(size) >> PAGE_SHIFT);
		config_acp3x_dma(rtd, substream->stream);
		status = 0;
	} else {
		status = -ENOMEM;
	}
	return status;
}

static snd_pcm_uframes_t acp3x_dma_pointer(struct snd_pcm_substream *substream)
{
	u32 pos = 0;
	struct i2s_stream_instance *rtd = substream->runtime->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pos = rv_readl(rtd->acp3x_base +
				mmACP_BT_TX_LINKPOSITIONCNTR);
	else
		pos = rv_readl(rtd->acp3x_base +
				mmACP_BT_RX_LINKPOSITIONCNTR);

	if (pos >= MAX_BUFFER)
		pos = 0;

	return bytes_to_frames(substream->runtime, pos);
}

static int acp3x_dma_new(struct snd_soc_pcm_runtime *rtd)
{
	return snd_pcm_lib_preallocate_pages_for_all(rtd->pcm,
							SNDRV_DMA_TYPE_DEV,
							NULL, MIN_BUFFER,
							MAX_BUFFER);
}

static int acp3x_dma_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int acp3x_dma_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static int acp3x_dma_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *prtd = substream->private_data;
	struct i2s_dev_data *adata = dev_get_drvdata(prtd->platform->dev);
	struct i2s_stream_instance *rtd = substream->runtime->private_data;

	kfree(rtd);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		adata->play_stream = NULL;
	else
		adata->capture_stream = NULL;

	/* Disable ACP irq, when the current stream is being closed and
	 * another stream is also not active.
	 */
	if (!adata->play_stream && !adata->capture_stream)
		rv_writel(0, adata->acp3x_base + mmACP_EXTERNAL_INTR_ENB);

	return 0;
}

static struct snd_pcm_ops acp3x_dma_ops = {
	.open = acp3x_dma_open,
	.close = acp3x_dma_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = acp3x_dma_hw_params,
	.hw_free = acp3x_dma_hw_free,
	.pointer = acp3x_dma_pointer,
	.mmap = acp3x_dma_mmap,
};

static struct snd_soc_platform_driver acp3x_asoc_platform = {
	.ops = &acp3x_dma_ops,
	.pcm_new = acp3x_dma_new,
};

struct snd_soc_dai_ops acp3x_dai_i2s_ops = {
	.hw_params = NULL,
	.trigger   = NULL,
	.set_fmt = NULL,
};

static struct snd_soc_dai_driver acp3x_i2s_dai_driver = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S8 |
					SNDRV_PCM_FMTBIT_U8 |
					SNDRV_PCM_FMTBIT_S24_LE |
					SNDRV_PCM_FMTBIT_S32_LE,
		.channels_min = 2,
		.channels_max = 8,

		.rate_min = 8000,
		.rate_max = 96000,
	},
	.capture = {
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S8 |
					SNDRV_PCM_FMTBIT_U8 |
					SNDRV_PCM_FMTBIT_S24_LE |
					SNDRV_PCM_FMTBIT_S32_LE,
		.channels_min = 2,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 48000,
	},
	.ops = &acp3x_dai_i2s_ops,
};

static const struct snd_soc_component_driver acp3x_i2s_component = {
	.name           = "acp3x_i2s",
};

static int acp3x_audio_probe(struct platform_device *pdev)
{
	int status;
	struct resource *res;
	struct i2s_dev_data *adata;
	unsigned int irqflags;

	if (pdev->dev.platform_data == NULL) {
		dev_err(&pdev->dev, "platform_data not retrieved\n");
		return -ENODEV;
	}
	irqflags = *((unsigned int *)(pdev->dev.platform_data));

	adata = devm_kzalloc(&pdev->dev, sizeof(struct i2s_dev_data),
				GFP_KERNEL);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "IORESOURCE_IRQ FAILED\n");
			return -ENODEV;
	}

	adata->acp3x_base = devm_ioremap(&pdev->dev, res->start,
			resource_size(res));

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "IORESOURCE_IRQ FAILED\n");
		return -ENODEV;
	}

	adata->i2s_irq = res->start;
	adata->play_stream = NULL;
	adata->capture_stream = NULL;

	dev_set_drvdata(&pdev->dev, adata);
	/* Initialize ACP */
	status = acp3x_init(adata->acp3x_base);
	if (status)
		return -ENODEV;

	status = snd_soc_register_platform(&pdev->dev, &acp3x_asoc_platform);
	if (status != 0) {
		dev_err(&pdev->dev, "Fail to register ALSA platform device\n");
		goto dev_err;
	}

	status = devm_snd_soc_register_component(&pdev->dev,
			&acp3x_i2s_component, &acp3x_i2s_dai_driver, 1);
	if (status != 0) {
		dev_err(&pdev->dev, "Fail to register acp i2s dai\n");
		snd_soc_unregister_platform(&pdev->dev);
		goto dev_err;
	}

	status = devm_request_irq(&pdev->dev, adata->i2s_irq, i2s_irq_handler,
					irqflags, "ACP3x_I2S_IRQ", adata);
	if (status) {
		dev_err(&pdev->dev, "ACP3x I2S IRQ request failed\n");
		snd_soc_unregister_platform(&pdev->dev);
		snd_soc_unregister_component(&pdev->dev);
		goto dev_err;
	}

	return 0;
dev_err:
	status = acp3x_deinit(adata->acp3x_base);
	if (status)
		dev_err(&pdev->dev, "ACP de-init failed\n");
	else
		dev_info(&pdev->dev, "ACP de-initialized\n");
	/*ignore device status and return driver probe error*/
	return -ENODEV;
}

static int acp3x_audio_remove(struct platform_device *pdev)
{
	int ret;
	struct i2s_dev_data *adata = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	snd_soc_unregister_platform(&pdev->dev);

	ret = acp3x_deinit(adata->acp3x_base);
	if (ret)
		dev_err(&pdev->dev, "ACP de-init failed\n");
	else
		dev_info(&pdev->dev, "ACP de-initialized\n");

	return 0;
}

static struct platform_driver acp3x_dma_driver = {
	.probe = acp3x_audio_probe,
	.remove = acp3x_audio_remove,
	.driver = {
		.name = "acp3x_rv_i2s",
	},
};

module_platform_driver(acp3x_dma_driver);

MODULE_AUTHOR("Maruthi.Bayyavarapu@amd.com");
MODULE_DESCRIPTION("AMD ACP 3.x PCM Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:acp3x-i2s-audio");
