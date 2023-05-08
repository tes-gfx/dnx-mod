#ifndef _DNX_GEM_H_
#define _DNX_GEM_H_


#include <drm/drmP.h>
#include <drm/drm_gem_cma_helper.h>


struct dnx_bo {
    struct drm_gem_object base;
    
    /* Only set if this BO is backed by memory from the shader program arena.*/
    struct drm_mm_node *mm;

    dma_addr_t paddr;
	void *vaddr;
};

static inline struct dnx_bo *
to_dnx_bo(struct drm_gem_object *gem_obj)
{
	return container_of(gem_obj, struct dnx_bo, base);
}

struct dnx_bo *dnx_gem_new(struct drm_device *dev, size_t unaligned_size, dma_addr_t *paddr);
int dnx_gem_mmap_offset(struct drm_gem_object *obj, u64 *offset);


#endif /* _DNX_GEM_H_ */
