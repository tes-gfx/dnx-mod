#ifndef __DNX_GPU_H__
#define __DNX_GPU_H__


#include <linux/types.h>
#include <linux/spinlock.h>
#include <drm/drm_mm.h>
#include <drm/drm_gem_cma_helper.h>

#include "dnx_drv.h"


#define DNX_RINGBUFFER_SIZE PAGE_SIZE
#define DNX_RINGBUFFER_MAX_SLOTS (128)
#define MB(x) (x * 1024 * 1024)
#define DNX_PROGRAM_ARENA_SIZE (MB(4))


struct dnx_cmdbuf;
struct dnx_bo;

struct dnx_arena {
	struct drm_mm mm;
	void *vaddr;
	dma_addr_t paddr;
	u32 size;
};

struct dnx_device {
	struct device     *dev;
	struct drm_device *drm;
	struct mutex lock;


	void __iomem      *mmio;
	resource_size_t    mmio_size;

	int irq;

	phys_addr_t base_reg;

	/* Register shadows */
	u32 reg_irqmask;
	u32 reg_irq_state;

	/* ring-buffer */
	struct dnx_ringbuf *buffer;
	bool stc_running;
	spinlock_t stc_lock; /* synchronization of user/irq context STC triggering */

	/* Shader program arena */
	struct dnx_arena program_arena;

	/* list of currently in-flight command buffers */
	struct list_head active_cmd_list;
	u32 active_cmd_count;

	/* worker for handling active-list retiring: */
	struct work_struct retire_work;
	struct workqueue_struct *wq;

	/* Fencing */
	u32 fence_completed;
	u32 fence_next;
	u32 fence_active;
	u32 fence_retired;
	wait_queue_head_t fence_waitq;

	/* Debug */
	volatile u32 debug_irq;
	spinlock_t debug_irq_slck; /* to wait for soft irq */
	wait_queue_head_t debug_irq_waitq;
	bool recover;
};

struct dnx_ringbuf {
	void *vaddr;
	dma_addr_t paddr;
	u32 size;
	u32 user_size;
	u32 fence;
};

struct dnx_cmdbuf {
	struct dnx_device *dnx;
	dma_addr_t paddr; /* start address of stream */
	void* vjmpaddr; /* jump command kernel space address to patch in stream */
	u32 fence; /* fence after which this buffer is to be disposed */
	struct list_head node; /* GPU in-flight list */
	unsigned int nr_bos;
	struct dnx_bo *bos[0];
};


static inline void dnx_queue_work(struct drm_device *dev,
	struct work_struct *w)
{
	struct dnx_device *dnx = dev->dev_private;

	queue_work(dnx->wq, w);
}


int dnx_gpu_init(struct dnx_device *dnx);
void dnx_gpu_release(struct dnx_device *dnx);
void dnx_hw_reset(struct dnx_device *dnx);

struct dnx_cmdbuf *dnx_gpu_cmdbuf_new(struct dnx_device *dnx, size_t nr_bo);
int dnx_gpu_cmdbuf_lookup_objects(struct dnx_cmdbuf *buf,
	struct drm_file *file, u32 *handles, unsigned nr_bos);
void dnx_gpu_cmdbuf_free(struct dnx_cmdbuf *buf);

int dnx_gpu_submit(struct dnx_device *dnx, struct dnx_cmdbuf *buf);
int dnx_gpu_wait_fence_interruptible(struct dnx_device *dnx, u32 fence, struct timespec *timeout);

void dnx_gpu_recover_hangup(struct dnx_device *dnx);


static inline bool fence_completed(struct dnx_device *dnx, u32 fence)
{
	return fence_after_eq(dnx->fence_completed, fence);
}


#endif
