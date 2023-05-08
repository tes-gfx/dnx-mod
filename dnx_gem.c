#include "dnx_gem.h"

#include <drm/drm_gem_cma_helper.h>


/**
 * __dnx_bo_create - Create a DNX BO without allocating memory
 * @drm: DRM device
 * @size: size of the object to allocate
 *
 * This function creates and initializes a DNX BO of the given size,
 * but doesn't allocate any memory to back the object.
 *
 * Returns:
 * A struct dnx_bo_object * on success or an ERR_PTR()-encoded negative
 * error code on failure.
 */
static struct dnx_bo *
__dnx_bo_create(struct drm_device *drm, size_t size)
{
	struct dnx_bo *dnx_obj;
	struct drm_gem_object *gem_obj;
	int ret;

	if (drm->driver->gem_create_object)
		gem_obj = drm->driver->gem_create_object(drm, size);
	else
		gem_obj = kzalloc(sizeof(*dnx_obj), GFP_KERNEL);
	if (!gem_obj)
		return ERR_PTR(-ENOMEM);
	dnx_obj = container_of(gem_obj, struct dnx_bo, base);

	ret = drm_gem_object_init(drm, gem_obj, size);
	if (ret)
		goto error;

	ret = drm_gem_create_mmap_offset(gem_obj);
	if (ret) {
		drm_gem_object_release(gem_obj);
		goto error;
	}

	return dnx_obj;

error:
	kfree(dnx_obj);
	return ERR_PTR(ret);
}


struct dnx_bo *dnx_gem_new(struct drm_device *dev, size_t unaligned_size, dma_addr_t *paddr)
{
	struct dnx_bo *bo;
	size_t size;
	int ret;

	if(unaligned_size == 0)
			return ERR_PTR(-EINVAL);

	size = round_up(unaligned_size, PAGE_SIZE);
	dev_dbg(dev->dev, "%s size=0x%08zx\n", __func__, unaligned_size);
	dev_dbg(dev->dev, "%s actual_size=0x%08zx\n", __func__, size);
	bo = __dnx_bo_create(dev, size);
	if (IS_ERR(bo))
		return bo;

	bo->vaddr = dma_alloc_wc(dev->dev, size, &bo->paddr,
				      GFP_KERNEL | __GFP_NOWARN);
	if (!bo->vaddr) {
		dev_err(dev->dev, "failed to allocate buffer with size %zu\n",
			size);
		ret = -ENOMEM;
		goto error;
	}

	*paddr = bo->paddr;

	return bo;

error:
	drm_gem_object_put_unlocked(&bo->base);
	return ERR_PTR(ret);
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
