#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/mm.h>
#include <linux/bug.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/dnx_drm.h>
#include "dnx_selftest.h"
#include "dnx_drv.h"
#include "dnx_gpu.h"
#include "dnx_gem.h"
#include "dnx_dbg.h"
#include "dnx_debugfs.h"
#include "nx_register_address.h"

static int recover = 0;

module_param(recover, int, 0444);
MODULE_PARM_DESC(recover, "enable recovering from hang up");

static const struct platform_device_id dnx_id_table[] = {
  { "dnx", 0 },
  { }
};
MODULE_DEVICE_TABLE(platform, dnx_id_table);

static const struct of_device_id dnx_of_table[] = {
  { .compatible = "tes,dnx-1.0", .data = NULL },
  { }
};
MODULE_DEVICE_TABLE(of, dnx_of_table);

u32 dnx_reg_read(struct dnx_device *dnx, u32 reg)
{
  return ioread32(dnx->mmio + (reg*4));
}

void dnx_reg_write(struct dnx_device *dnx, u32 reg, u32 val)
{
  iowrite32(val, dnx->mmio + (reg*4));
}

/*
 * DNX ioctls:
 */

static int dnx_ioctl_get_reg(struct drm_device *dev, void *data,
	struct drm_file *file)
{
	return 0;
}

static int dnx_ioctl_set_reg(struct drm_device *dev, void *data,
	struct drm_file *file)
{
	return 0;
}

static int dnx_ioctl_self_test(struct drm_device *dev, void *data,
        struct drm_file *file)
{
        u32 *args = data;
	int err = 0;

        dev_info(dev->dev, "Starting self test...\n");

	err = dnx_selftest(dev->dev_private);

	*args = err;

        return 0;
}

static int dnx_ioctl_reset(struct drm_device *dev, void *data,
        struct drm_file *file)
{
	dev_info(dev->dev, "Resetting core...\n");

	dnx_hw_reset(dev->dev_private);

	return 0;
}

static int dnx_ioctl_gem_new(struct drm_device *dev, void *data,
	struct drm_file *file)
{
	struct drm_dnx_gem_new *args = data;
	struct dnx_bo *bo = NULL;
	u32 flags = 0;
	int ret;

	dev_dbg(dev->dev, "New bo:\n");
	dev_dbg(dev->dev, " size=0x%08llx\n", args->size);
	dev_dbg(dev->dev, " flags=0x%08x\n", args->flags);

	if (args->flags & ~(DNX_BO_CACHED | DNX_BO_WC | DNX_BO_UNCACHED | DNX_BO_SHADER_ARENA))
			return -EINVAL;

	if(args->flags & DNX_BO_SHADER_ARENA) {
		flags |= DNX_GEM_FLAG_ARENA_PROGRAM;
	}
	else {
		flags |= DNX_GEM_FLAG_ARENA_VIDEO;
	}

	bo = dnx_gem_create(dev, args->size, flags);
	if(IS_ERR(bo))
		return PTR_ERR(bo);

	args->paddr = bo->paddr;
	dev_dbg(dev->dev, " paddr=0x%08llx\n", args->paddr);
	dev_dbg(dev->dev, " vaddr=0x%p\n", bo->vaddr);

	ret = drm_gem_handle_create(file, &bo->base, &args->handle);
	drm_gem_object_unreference_unlocked(&bo->base);

	dev_dbg(dev->dev, " handle=0x%08x\n", args->handle);
	dev_dbg(dev->dev, " obj=0x%p\n", bo);
	dev_dbg(dev->dev, " actual_size=0x%08zx\n", bo->base.size);

	return ret;
}

static int dnx_ioctl_gem_info(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_dnx_gem_info *args = data;
	struct drm_gem_object *obj;
	int ret;

	if (args->pad)
		return -EINVAL;

	obj = drm_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

	ret = dnx_gem_mmap_offset(obj, &args->offset);
	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

static int dnx_ioctl_gem_user(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_dnx_gem_user *args = data;
	struct drm_gem_object *obj;
	struct dnx_bo *bo;

	if (args->pad)
		return -EINVAL;

	obj = drm_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

	bo = to_dnx_bo(obj);
	args->paddr = bo->paddr;

	drm_gem_object_unreference_unlocked(obj);

	return 0;
}

#define TS(t) ((struct timespec){ \
	.tv_sec = (t).tv_sec, \
	.tv_nsec = (t).tv_nsec \
})

static int dnx_ioctl_gem_cpu_prep(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_dnx_gem_cpu_prep *args = data;
	struct drm_gem_object *obj;
	int ret = -1;

	if (args->op & ~(DNX_PREP_READ | DNX_PREP_WRITE | DNX_PREP_NOSYNC))
		return -EINVAL;

	obj = drm_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

//	ret = dnx_gem_cpu_prep(obj, args->op, &TS(args->timeout));
	dev_err(dev->dev, "dnx_ioctl_gem_cpu_prep not implemented yet\n");

	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

static int dnx_ioctl_gem_cpu_fini(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_dnx_gem_cpu_fini *args = data;
	struct drm_gem_object *obj;
	int ret = -1;

	if (args->flags)
		return -EINVAL;

	obj = drm_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

//	ret = dnx_gem_cpu_fini(obj);
	dev_err(dev->dev, "dnx_ioctl_gem_cpu_fini not implemented yet\n");

	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

static int dnx_ioctl_wait_fence(struct drm_device *dev, void *data,
	struct drm_file *file)
{
	struct drm_dnx_wait_fence *args = data;
	struct timespec *timeout = &TS(args->timeout);
	int ret;

	dev_dbg(dev->dev, "Wait for fence:\n");
	dev_dbg(dev->dev, " fence=%u\n", args->fence);
	dev_dbg(dev->dev, " timeout.sec=%u\n", (u32) args->timeout.tv_sec);
	dev_dbg(dev->dev, " timeout.nsec=%u\n", (u32) args->timeout.tv_nsec);

	if(args->flags & DNX_WAIT_NONBLOCK)
		timeout = NULL;

	ret = dnx_gpu_wait_fence_interruptible(dev->dev_private, args->fence, timeout);


	return ret;
}

static const struct drm_ioctl_desc dnx_ioctls[] = {
#define DNX_IOCTL(n, func, flags) \
	DRM_IOCTL_DEF_DRV(DNX_##n, dnx_ioctl_##func, flags)
	DNX_IOCTL(GET_REG,       get_reg,       DRM_AUTH|DRM_RENDER_ALLOW),
	DNX_IOCTL(SET_REG,       set_reg,       DRM_AUTH|DRM_RENDER_ALLOW),
	DNX_IOCTL(SELF_TEST,     self_test,     DRM_AUTH|DRM_RENDER_ALLOW),
	DNX_IOCTL(RESET,         reset,         DRM_AUTH|DRM_RENDER_ALLOW),
	DNX_IOCTL(STREAM_SUBMIT, gem_submit,    DRM_AUTH|DRM_RENDER_ALLOW),
	DNX_IOCTL(WAIT_FENCE,    wait_fence,    DRM_AUTH|DRM_RENDER_ALLOW),
	DNX_IOCTL(GEM_NEW,       gem_new,       DRM_AUTH|DRM_RENDER_ALLOW),
	DNX_IOCTL(GEM_INFO,      gem_info,      DRM_AUTH|DRM_RENDER_ALLOW),
	DNX_IOCTL(GEM_USER,      gem_user,      DRM_AUTH|DRM_RENDER_ALLOW),
	DNX_IOCTL(GEM_CPU_PREP,  gem_cpu_prep,  DRM_AUTH|DRM_RENDER_ALLOW),
	DNX_IOCTL(GEM_CPU_FINI,  gem_cpu_fini,  DRM_AUTH|DRM_RENDER_ALLOW),
};

static irqreturn_t irq_handler(int irq, void *data)
{
	struct dnx_device *dnx = data;
	u32 stat = dnx_reg_read(dnx, DNX_REG_CONTROL_IRQ_STATE);

	dnx_reg_write(dnx, DNX_REG_CONTROL_IRQ_STATE, stat);

	stat &= dnx->reg_irqmask;

	if(stat & DNX_IRQ_MASK_STREAM_SOFT) {
		dev_info(dnx->dev, "IRQ soft triggered\n");
	}

	if(stat & DNX_IRQ_MASK_SDMA_DONE) {
		dev_info(dnx->dev, "IRQ SDMA transfer finished\n");
	}

	if(stat & DNX_IRQ_MASK_STREAM_SYNC) {
		dnx->fence_completed = dnx_reg_read(dnx, DNX_REG_CONTROL_SYNC_0);
		wake_up_interruptible(&dnx->fence_waitq);
		dnx_queue_work(dnx->drm, &dnx->retire_work);
	}


	if(stat & DNX_IRQ_MASK_STREAM_DONE) {
		BUG_ON(!dnx->stc_running && !(stat & DNX_IRQ_MASK_STREAM_SOFT));

		spin_lock(&dnx->stc_lock);
		dev_dbg(dnx->dev, "IRQ_STREAM: fence_completed=%d fence_active=%d\n",
				dnx->fence_completed, dnx->fence_active);
		/* Retrigger stream controller if we have outstanding command lists */
		if(dnx->fence_completed < dnx->fence_active) {
			/* We end up here, if the stream controller reached the last END cmd just
			 * before the next cmdbuf was queued.
			 */
			u32 stc_pos;
			/* we can use STC's stop position since it has been changed to a JMP already */
			dev_dbg(dnx->dev, "Restarting STC (completed=%u, active=%u\n",
					dnx->fence_completed, dnx->fence_active);
			stc_pos = dnx_reg_read(dnx, DNX_REG_CONTROL_STREAM_POS);
			dnx_reg_write(dnx, DNX_REG_CONTROL_STREAM_ADDR, stc_pos);
			while(!(dnx_reg_read(dnx, DNX_REG_CONTROL_BUSY) & 0x1)) {
				dev_dbg(dnx->dev, "Wait for STC to start...\n");
				// NOP
			}
		}
		else {
			dev_dbg(dnx->dev, "Stopping STC (c=%u,a=%u)\n",
					dnx->fence_completed, dnx->fence_active);
			dnx->stc_running = false;
		}
		spin_unlock(&dnx->stc_lock);
	}

	if(stat & DNX_IRQ_MASK_ERRORS) {
		dnx_debug_irq(dnx, stat);
		dnx_debug_reg_dump(dnx);

		if(DNX_IRQ_MASK_STREAM_ERR & stat)
			dnx_debug_stream_err(dnx);
	} 

	/* Debug stuff */
	spin_lock(&dnx->debug_irq_slck);
	dnx->debug_irq = stat;
	spin_unlock(&dnx->debug_irq_slck);
	wake_up_interruptible(&dnx->debug_irq_waitq);

	return IRQ_HANDLED;
}

static struct vm_operations_struct dnx_remap_vm_ops = {
// nothing to do here...
};

int dnx_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	size_t size = vma->vm_end - vma->vm_start;
	struct dnx_device *dnx = dev->dev_private;

	/* Check if we have an non-drm-gem mmap call here. We
	 * assume that DRM_FILE_PAGE_OFFSET_START is 0x10000. */
	if(vma->vm_pgoff >= 0x10000) {
		int ret;

		dev_dbg(dev->dev, "mmap dnx bo vm_pgoff=%lx\n", vma->vm_pgoff);

		ret = dnx_gem_mmap(filp, vma);
		if (ret) {
			dev_err(dev->dev, "mmap dnx gem failed: %d", ret);
			return ret;
		}
	}
	else {
		/* We detect the mapping type by its size.
		 * Register mapping should be removed and replaced by specific IOCTLs.
		 */
		if(size == dnx->mmio_size) {
			vma->vm_pgoff = (size_t)dnx->base_reg >> PAGE_SHIFT;
		}
		else if(size == 0x08000000) {
			dev_err(dev->dev, "!!!using dedicated video memory is deprecated!!!\n");
			return -EINVAL;
		}
		else {
			dev_err(dev->dev, "illegal mmap length 0x%x\n", size);
		}

		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		vma->vm_ops = &dnx_remap_vm_ops;

		if(remap_pfn_range(vma,
				   vma->vm_start,
				   vma->vm_pgoff,
				   size,
				   vma->vm_page_prot)) {
			return -EAGAIN;
		}
	}

	return 0;
}

static const struct file_operations dnx_fops = {
  .owner          = THIS_MODULE,
  .open           = drm_open,
  .release        = drm_release,
  .unlocked_ioctl = drm_ioctl,
#ifdef CONFIG_COMPAT
  .compat_ioctl   = drm_compat_ioctl,
#endif
  .poll           = drm_poll,
  .read           = drm_read,
  .llseek         = no_llseek,
  .mmap           = dnx_mmap,
};

const struct vm_operations_struct dnx_gem_vm_ops = {
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

/* todo: remove prime support for the time being */
static struct drm_driver dnx_driver = {
  .driver_features           = DRIVER_HAVE_IRQ | DRIVER_GEM | DRIVER_PRIME | DRIVER_RENDER,
  .gem_free_object           = dnx_gem_free_object,
  .prime_handle_to_fd        = drm_gem_prime_handle_to_fd,
  .prime_fd_to_handle        = drm_gem_prime_fd_to_handle,
  .gem_prime_import          = drm_gem_prime_import,
  .gem_prime_export          = drm_gem_prime_export,
  .gem_prime_get_sg_table    = drm_gem_cma_prime_get_sg_table,
  .gem_prime_import_sg_table = drm_gem_cma_prime_import_sg_table,
  .gem_prime_vmap            = drm_gem_cma_prime_vmap,
  .gem_prime_vunmap          = drm_gem_cma_prime_vunmap,
  .gem_prime_mmap            = drm_gem_cma_prime_mmap,
  .gem_vm_ops                = &dnx_gem_vm_ops,
  .dumb_create               = drm_gem_dumb_create,
  .dumb_map_offset           = drm_gem_dumb_map_offset,
  .dumb_destroy              = drm_gem_dumb_destroy,
#ifdef CONFIG_DEBUG_FS
  .debugfs_init              = dnx_debugfs_init,
#endif
  .ioctls = dnx_ioctls,
  .num_ioctls = DRM_DNX_NUM_IOCTLS,
  .fops  = &dnx_fops,
  .name  = "tes-dnx",
  .desc  = "tes-dnx DRM",
  .date  = "20170103",
  .major = 1,
  .minor = 0,
};

static int dnx_pm_suspend(struct device *dev) {

  dev_dbg(dev, "%s\n", __func__);

  return 0;
}

static int dnx_pm_resume(struct device *dev) {

  dev_dbg(dev, "%s\n", __func__);

  return 0;
}

static const struct dev_pm_ops dnx_pm_ops = {
  SET_SYSTEM_SLEEP_PM_OPS(dnx_pm_suspend, dnx_pm_resume)
};

static int dnx_remove(struct platform_device *pdev) {
  struct dnx_device *dnx = platform_get_drvdata(pdev);
  struct drm_device *ddev = dnx->drm;

  dnx_gpu_release(dnx);

  drm_dev_unregister(ddev);
  drm_dev_unref(ddev);

  return 0;
}

static int dnx_probe(struct platform_device *pdev) {
	struct dnx_device *dnx;
	struct drm_device *ddev;
	struct resource *mem;
	struct device_node *np = pdev->dev.of_node;
	dnx_config_ver_t version;
	dnx_config_1_t config1;
	int ret = 0;

	dnx = devm_kzalloc(&pdev->dev, sizeof(*dnx), GFP_KERNEL);
	if(!dnx)
		return -ENOMEM;

	mutex_init(&dnx->lock);
	spin_lock_init(&dnx->stc_lock);
	init_waitqueue_head(&dnx->fence_waitq);

	dnx->recover = recover ? true : false;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(!mem)
	{
		dev_err(&pdev->dev, "Can't find base address\n");
		return -ENODEV;
	}

	dnx->base_reg = mem->start;

	dnx->mmio = devm_ioremap_resource(&pdev->dev, mem);
	if(IS_ERR(dnx->mmio))
	{
		dev_err(&pdev->dev, "failed to ioremap %s: %ld\n", dev_name(dnx->dev), PTR_ERR(dnx->mmio));
		return PTR_ERR(dnx->mmio);
	}
	dnx->mmio_size = resource_size(mem);

	np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (np) {
		dev_err(&pdev->dev, "Using reserved memory as CMA pool\n");
		ret = of_reserved_mem_device_init(&pdev->dev);
		if(ret) {
			dev_err(&pdev->dev, "Could not get reserved memory\n");
			return ret;
		}
		of_node_put(np);
	}
	else
		dev_err(&pdev->dev, "Using default CMA pool\n");

	/* Enabling IRQ */
	dnx->irq = platform_get_irq(pdev, 0);
	if(dnx->irq < 0) {
		dev_err(&pdev->dev, "failed to get irq: %d\n", ret);
		return dnx->irq;
	}

	/* Validate HW */
	version.m_data = dnx_reg_read(dnx, DNX_REG_CONTROL_VERSION);
	if(version.bits.m_device != 0xd5) {
		dev_err(&pdev->dev, "could not access D/AVE NX hardware or wrong version, DNX_REG_CONTROL_VERSION is 0x%08x\n", version.m_data);
		return -ENODEV;
	}
	dev_info(&pdev->dev, "D/AVE NX HW ver. %u (SVN rev. %u):\n", version.bits.m_hwver, version.bits.m_vcsver);

	config1.m_data = dnx_reg_read(dnx, DNX_REG_CONTROL_CONFIG_1);
	dev_info(&pdev->dev, "\tShaders: %u\n", config1.bits.m_shader_count);
	dev_info(&pdev->dev, "\tALUs: %u\n", config1.bits.m_shader_alu_count*4);
	dev_info(&pdev->dev, "\tTexture units: %u\n", config1.bits.m_tex_units_count);
	dev_info(&pdev->dev, "\tAuto recover: %s\n", recover ? "enabled" : "disabled");

	if(DNX_HWVERSION != version.bits.m_hwver) {
		dev_err(&pdev->dev, "unsupported D/AVE NX hardware version (required: %u)\n", DNX_HWVERSION);
		return -ENODEV;
	}

	/* DRM/KMS objects */
	ddev = drm_dev_alloc(&dnx_driver, &pdev->dev);
	if (IS_ERR(ddev))
		return PTR_ERR(ddev);

	dnx->dev = &pdev->dev;
	dnx->drm = ddev;
	ddev->dev_private = dnx;

	platform_set_drvdata(pdev, dnx);

	ret = dnx_gpu_init(dnx);
	if(ret) {
		dev_err(&pdev->dev, "failed to initialize GPU: %d\n", ret);
		return ret;
	}

	/* setup debug facility */
	spin_lock_init(&dnx->debug_irq_slck);
	init_waitqueue_head(&dnx->debug_irq_waitq);

	/* Enable IRQ handler after HW and device object was set up properly */
	ret = devm_request_irq(dnx->dev, dnx->irq, irq_handler, 0, dev_name(dnx->dev), dnx);
	if(ret) {
		dev_err(&pdev->dev, "failed to request IRQ %u: %d\n", dnx->irq, ret);
		return ret;
	}

	/* Register the DRM device. */
	ret = drm_dev_register(ddev, 0);
	if (ret)
		goto error;

	return 0;

error:
	dnx_remove(pdev);

	return ret;
}

static struct platform_driver dnx_platform_driver = {
  .probe = dnx_probe,
  .remove = dnx_remove,
  .driver = {
    .name = "tes-dnx",
    .pm = &dnx_pm_ops,
    .of_match_table = dnx_of_table,
  },
  .id_table = dnx_id_table,
};
module_platform_driver(dnx_platform_driver);

MODULE_AUTHOR("Christian Thaler <christian.thaler@tes-dst.com>");
MODULE_DESCRIPTION("D/AVE NX DRM Driver");
MODULE_LICENSE("GPL v2");
