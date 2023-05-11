#ifndef _DNX_GEM_H_
#define _DNX_GEM_H_


#include <drm/drmP.h>
#include <drm/drm_gem_cma_helper.h>


#define DRM_GEM_PROGRAM_ARENA_ALIGN_SHIFT (7)
#define DRM_GEM_PROGRAM_ARENA_ALIGN_MASK ((1 << DRM_GEM_PROGRAM_ARENA_ALIGN_SHIFT) - 1)

/**
 * struct drm_gem_cma_object - GEM object backed by CMA memory allocations
 * @base: base GEM object
 * @mm_node: range allocator node of backing memory in shader program arena
 *   (only set if this BO is backed by memory from the shader program arena)
 * @paddr: physical address of the backing memory
 * @vaddr: kernel virtual address of the backing memory
 * @sgt: scatter/gather table for imported PRIME buffers
 */
struct dnx_bo {
    struct drm_gem_object base;
    struct drm_mm_node *mm_node;
    dma_addr_t paddr;
	void *vaddr;
	struct sg_table *sgt;
};

static inline struct dnx_bo *
to_dnx_bo(struct drm_gem_object *gem_obj)
{
	return container_of(gem_obj, struct dnx_bo, base);
}

#define DNX_GEM_FLAG_ARENA_VIDEO   (0x1)
#define DNX_GEM_FLAG_ARENA_PROGRAM (0x2)
struct dnx_bo *dnx_gem_new(struct drm_device *dev, size_t unaligned_size, dma_addr_t *paddr, u32 flags);
void dnx_gem_free_object(struct drm_gem_object *gem_obj);
int dnx_gem_mmap(struct file *filp, struct vm_area_struct *vma);
int dnx_gem_mmap_offset(struct drm_gem_object *obj, u64 *offset);


#endif /* _DNX_GEM_H_ */
