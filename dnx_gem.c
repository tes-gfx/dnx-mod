#include "dnx_gem.h"
#include "dnx_gpu.h"

#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_mm.h>


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


struct dnx_bo *dnx_gem_create(struct drm_device *dev, size_t unaligned_size, dma_addr_t *paddr, u32 flags)
{
	struct dnx_device *dnx = dev->dev_private;
	struct dnx_bo *bo;
	size_t size;
	int ret;

	if(unaligned_size == 0)
			return ERR_PTR(-EINVAL);
	size = round_up(unaligned_size, DNX_GEM_ALIGN_SIZE);

	dev_dbg(dev->dev, "%s size=0x%08zx\n", __func__, unaligned_size);
	dev_dbg(dev->dev, "%s actual_size=0x%08zx\n", __func__, size);
	bo = __dnx_bo_create(dev, size);
	if (IS_ERR(bo))
		return bo;

	if(flags & DNX_GEM_FLAG_ARENA_VIDEO) {
		bo->vaddr = dma_alloc_wc(dev->dev, size, &bo->paddr,
						GFP_KERNEL | __GFP_NOWARN);
		if (!bo->vaddr) {
			dev_err(dev->dev, "failed to allocate buffer with size %zu\n",
				size);
			ret = -ENOMEM;
			goto error_alloc;
		}
		dev_dbg(dev->dev, "%s arena=video\n", __func__);
	}
	else if(flags & DNX_GEM_FLAG_ARENA_PROGRAM) {
		bo->mm_node = kzalloc(sizeof(*bo->mm_node), GFP_KERNEL | __GFP_NOWARN);
		if (!bo->mm_node) {
			dev_err(dev->dev, "failed to allocate mm node\n");
			ret = -ENOMEM;
			goto error_alloc;
		}
		dev_dbg(dev->dev, "%s arena.mm=%p\n", __func__, &dnx->program_arena.mm);
		ret = drm_mm_insert_node(&dnx->program_arena.mm, bo->mm_node, size >> DNX_GEM_ALIGN_SHIFT);
		if(ret) {
			dev_err(dev->dev, "failed to insert mm node (%d)\n", ret);
			goto error_alloc;
		}
		
		bo->paddr = dnx->program_arena.paddr + (bo->mm_node->start << DNX_GEM_ALIGN_SHIFT);
		bo->vaddr = dnx->program_arena.vaddr + (bo->mm_node->start << DNX_GEM_ALIGN_SHIFT);
		dev_dbg(dev->dev, "%s arena=program\n", __func__);
	}

	*paddr = bo->paddr;

	return bo;

error_alloc:
	drm_gem_object_put_unlocked(&bo->base);
	return ERR_PTR(ret);
}


static int dnx_gem_mmap_obj(struct dnx_bo *bo,
				struct vm_area_struct *vma)
{
	int ret;

	/*
	 * Clear the VM_PFNMAP flag that was set by drm_gem_mmap(), and set the
	 * vm_pgoff (used as a fake buffer offset by DRM) to 0 as we want to map
	 * the whole buffer.
	 */
	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;

	ret = dma_mmap_wc(bo->base.dev->dev, vma, bo->vaddr,
			  bo->paddr, vma->vm_end - vma->vm_start);
	if (ret)
		drm_gem_vm_close(vma);

	return ret;
}


int dnx_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct dnx_bo *bo;
	struct drm_gem_object *gem_obj;
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if (ret)
		return ret;

	gem_obj = vma->vm_private_data;
	bo = to_dnx_bo(gem_obj);

	return dnx_gem_mmap_obj(bo, vma);
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


void dnx_gem_free_object(struct drm_gem_object *gem_obj)
{
	struct dnx_bo *bo;

	bo = to_dnx_bo(gem_obj);

	dev_dbg(gem_obj->dev->dev, "freeing bo 0x%p kref=%d\n", gem_obj, gem_obj->refcount.refcount.refs.counter);

	if (bo->vaddr) {
		if(bo->mm_node) {
			dev_dbg(gem_obj->dev->dev, "freeing mm node 0x%p\n", bo->mm_node);
			dev_dbg(gem_obj->dev->dev, "  mm 0x%p\n", bo->mm_node->mm);
			drm_mm_remove_node(bo->mm_node);
			kfree(bo->mm_node);
		}
		else {
			dma_free_wc(gem_obj->dev->dev, bo->base.size,
					bo->vaddr, bo->paddr);
		}
	} else if (gem_obj->import_attach) {
		drm_prime_gem_destroy(gem_obj, bo->sgt);
	}

	drm_gem_object_release(gem_obj);

	kfree(bo);
}
