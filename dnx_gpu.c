#include "dnx_gpu.h"

#include <linux/delay.h>
#include <drm/drm_mm.h>

#include "dnx_drv.h"
#include "dnx_gem.h"
#include "dnx_buffer.h"
#include "nx_register_address.h"
#include "nx_types.h"


static void dnx_hw_init(struct dnx_device *dnx)
{
	dnx_reg_write(dnx, DNX_REG_CONTROL_IRQ_MASK, dnx->reg_irqmask);
	dnx->reg_irqmask = dnx_reg_read(dnx, DNX_REG_CONTROL_IRQ_MASK);
}


static void retire_worker(struct work_struct *work)
{
	struct dnx_device *dnx = container_of(work, struct dnx_device, retire_work);
	u32 fence = dnx->fence_completed;
	struct dnx_cmdbuf *cmdbuf, *tmp;
	unsigned int i;

	mutex_lock(&dnx->lock);

	list_for_each_entry_safe(cmdbuf, tmp, &dnx->active_cmd_list, node) {
		if (!fence_completed(dnx, cmdbuf->fence))
			break;

		list_del(&cmdbuf->node);
		--dnx->active_cmd_count;

		for (i = 0; i < cmdbuf->nr_bos; i++) {
			struct dnx_bo *bo = cmdbuf->bos[i];

			/* drop the refcount taken in dnx_ioctl_gem_submit */
			drm_gem_object_unreference_unlocked(&bo->base);
		}

		dnx_gpu_cmdbuf_free(cmdbuf);
	}

	dnx->fence_retired = fence;

	mutex_unlock(&dnx->lock);

//	wake_up_all(&gpu->fence_event);
}


static int dnx_gpu_arena_create(struct dnx_device *dnx, struct dnx_arena *arena, u32 size)
{
	int ret = 0;

	arena->size = size;
	arena->vaddr = dma_alloc_wc(dnx->dev, size, &arena->paddr, GFP_KERNEL);
	if (!arena->vaddr) {
		dev_err(dnx->dev, "failed to allocate arena with size %zu\n",
				size);
		ret = -ENOMEM;
	}

	drm_mm_init(&arena->mm, (u64) arena->paddr, size);

	return ret;
}


static void dnx_gpu_arena_delete(struct dnx_device *dnx, struct dnx_arena *arena)
{
	drm_mm_takedown(&arena->mm);
	dma_free_wc(dnx->dev, arena->size, arena->vaddr, arena->paddr);
	arena->vaddr = 0;
	arena->paddr = 0;
}


static struct dnx_ringbuf *dnx_gpu_ringbuf_new(struct dnx_device *dnx, u32 size)
{
	struct dnx_ringbuf *ringbuf;

	if(size != PAGE_SIZE)
	{
		dev_err(dnx->dev, "%s currently not implemented properly: only capable of creating ring buffer\n", __func__);
		return NULL;
	}

	ringbuf = kzalloc(sizeof(*ringbuf), GFP_KERNEL);
	if(!ringbuf)
		return NULL;

	ringbuf->vaddr = dma_alloc_wc(dnx->dev, size, &ringbuf->paddr, GFP_KERNEL);
	ringbuf->size = size;

	return ringbuf;
}


static void dnx_gpu_ringbuf_free(struct dnx_device *dnx, struct dnx_ringbuf *ringbuf)
{
	dma_free_wc(dnx->dev, ringbuf->size, ringbuf->vaddr, ringbuf->paddr);
	kfree(ringbuf);
}


int dnx_gpu_init(struct dnx_device *dnx) 
{
	int ret = 0;

	/* Enable all but SDMA irq.
	   Currently, there are 5 DMA transfers per vertex. That would
	   lead to a huge overhead for vertex processing.
	 */
	dnx->reg_irqmask = ~DNX_IRQ_MASK_SDMA_DONE;

	dnx_hw_init(dnx);

	/* create ring-buffer */
	dnx->buffer = dnx_gpu_ringbuf_new(dnx, DNX_RINGBUFFER_SIZE);
	if(!dnx->buffer) {
		dev_err(dnx->dev, "could not create command buffer\n");
		return -ENOMEM;
	}
	dnx_buffer_init(dnx);

	/* create shader program memory arena */
	ret = dnx_gpu_arena_create(dnx, &dnx->program_arena, DNX_PROGRAM_ARENA_SIZE);
	if(ret) {
		dev_err(dnx->dev, "could not create shader program arena\n");
		goto error_arena;
	}
	dnx_reg_write(dnx, DNX_REG_PGM_BASE_ADDR, dnx->program_arena.paddr);
	dev_dbg(dnx->dev, "created shader program arena at %zx\n", dnx->program_arena.paddr);

	INIT_LIST_HEAD(&dnx->active_cmd_list);
	dnx->active_cmd_count = 0;

	INIT_WORK(&dnx->retire_work, retire_worker);

	dnx->wq = alloc_ordered_workqueue("dnx", 0);
	if (!dnx->wq) {
		ret = -ENOMEM;
		goto error_wq;
	}

	return 0;

error_wq:
	dnx_gpu_arena_delete(dnx, &dnx->program_arena);

error_arena:
	dnx_gpu_ringbuf_free(dnx, dnx->buffer);

	return ret;
}


void dnx_gpu_release(struct dnx_device *dnx)
{
	flush_workqueue(dnx->wq);
	destroy_workqueue(dnx->wq);

	dnx_gpu_arena_delete(dnx, &dnx->program_arena);

	if(dnx->buffer) {
		dnx_gpu_ringbuf_free(dnx, dnx->buffer);
		dnx->buffer = NULL;
	}
}


void dnx_hw_reset(struct dnx_device *dnx)
{
	dnx_reg_write(dnx, DNX_REG_CONTROL_SOFT_RESET, 0xDEADC07E);
	msleep(1);

	dnx_hw_init(dnx);
}


void dnx_gpu_recover_hangup(struct dnx_device *dnx)
{
	dnx_hw_reset(dnx);

	dnx->stc_running = false;
}


struct dnx_cmdbuf *dnx_gpu_cmdbuf_new(struct dnx_device *dnx, size_t nr_bo)
{
	struct dnx_cmdbuf *buf;
	size_t size = size_vstruct(nr_bo, sizeof(buf->bos[0]), sizeof(*buf));

	buf = kzalloc(size, GFP_KERNEL);
	if(!buf)
		return NULL;

	buf->dnx = dnx;

	dev_dbg(dnx->dev, "new cmd buffer %p (bos size=%d)\n", buf, size-sizeof(*buf));

	return buf;
}


int dnx_gpu_cmdbuf_lookup_objects(struct dnx_cmdbuf *buf,
	struct drm_file *file, u32 *handles, unsigned nr_bos)
{
	unsigned i;
	int ret = 0;

	spin_lock(&file->table_lock);

	for (i = 0; i < nr_bos; i++) {
		struct drm_gem_object *obj;

		/* normally use drm_gem_object_lookup(), but for bulk lookup
		 * all under single table_lock just hit object_idr directly:
		 */
		obj = idr_find(&file->object_idr, handles[i]);
		if (!obj) {
			DRM_ERROR("invalid handle %u at index %u\n",
					handles[i], i);
			ret = -EINVAL;
			goto out_unlock;
		}

		/*
		 * Take a refcount on the object. The file table lock
		 * prevents the object_idr's refcount on this being dropped.
		 */
		drm_gem_object_reference(obj);

		buf->bos[i] = to_dnx_bo(obj);
	}

out_unlock:
	buf->nr_bos = i;
	spin_unlock(&file->table_lock);

	return ret;
}


void dnx_gpu_cmdbuf_free(struct dnx_cmdbuf *buf)
{
	dev_dbg(buf->dnx->dev, "freeing cmdbuf %p\n", buf);
	kfree(buf);
}


int dnx_gpu_submit(struct dnx_device *dnx, struct dnx_cmdbuf *buf)
{
	mutex_lock(&dnx->lock);

	buf->fence = ++dnx->fence_next;

	dnx_buffer_queue(dnx, buf);

	list_add_tail(&buf->node, &dnx->active_cmd_list);
	++dnx->active_cmd_count;

	mutex_unlock(&dnx->lock);

	return 0;
}


int dnx_gpu_wait_fence_interruptible(struct dnx_device *dnx, u32 fence, struct timespec *timeout)
{
	int ret;

	if(fence_after(fence, dnx->fence_next)) {
		dev_err(dnx->dev, "waiting on invalid fence: %u (of %u)\n", fence, dnx->fence_next);
		return -EINVAL;
	}

	if(!timeout) {
		ret = fence_completed(dnx, fence) ? 0 : -EBUSY;
	}
	else {
		unsigned long remaining = dnx_timeout_to_jiffies(timeout);
		struct timespec t;
		jiffies_to_timespec(jiffies - INITIAL_JIFFIES, &t);

//		dev_info(dnx->dev, "timeout: %lu jiffies\n", remaining);

		ret = wait_event_interruptible_timeout(dnx->fence_waitq, fence_completed(dnx, fence), remaining);

		if(ret == 0) {
			dev_err(dnx->dev, "timeout waiting for fence: %u (completed: %u)\n", fence, dnx->fence_completed);
			ret = -ETIMEDOUT;

			if(dnx->recover) {
				dev_err(dnx->dev, "core hang up! recovering...\n");
				dnx_gpu_recover_hangup(dnx);
			}
		}
		else if(ret != -ERESTARTSYS) {
			ret = 0;
		}
	}

	return ret;
}



