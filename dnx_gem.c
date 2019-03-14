#include "dnx_gem.h"

#include <drm/drm_gem_cma_helper.h>


struct drm_gem_object *dnx_gem_new(struct drm_device *dev, size_t unaligned_size, dma_addr_t *paddr)
{
	struct drm_gem_cma_object *obj;

	if(unaligned_size == 0)
			return ERR_PTR(-EINVAL);

	obj = drm_gem_cma_create(dev, unaligned_size);
	if(IS_ERR(obj)) {
		dev_err(dev->dev, "Failed to allocate from CMA\n");
		return ERR_PTR(-ENOMEM);
	}

	*paddr = obj->paddr;

	return &obj->base;
}


int dnx_gem_mmap_offset(struct drm_gem_object *obj, u64 *offset)
{
	int ret;

	/* Make it mmapable */
	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		dev_err(obj->dev->dev, "could not allocate mmap offset\n");
	else
		*offset = drm_vma_node_offset_addr(&obj->vma_node);

	return ret;
}
