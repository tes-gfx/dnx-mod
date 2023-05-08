#include "dnx_gem.h"

#include <drm/drm_gem.h>
#include <drm/drm_gem_cma_helper.h>

#include "dnx_gpu.h"


int dnx_ioctl_gem_submit(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct dnx_device *dnx = dev->dev_private;
	struct drm_dnx_stream_submit *args = data;
	u32 *handles;
	struct dnx_bo **bos = NULL;
	struct dnx_cmdbuf *cmdbuf;
	struct dnx_bo *last_page;
	dma_addr_t stream_addr;
	void* stream_jmpaddr;
	int ret, i;

	dev_dbg(dev->dev, "Submitting stream:\n");
	dev_dbg(dev->dev, " paddr=0x%08llx\n", args->stream);
	dev_dbg(dev->dev, " pjmpaddr=0x%08llx\n", args->jump);
	dev_dbg(dev->dev, " nr_bo=%d\n", args->nr_bos);
	dev_dbg(dev->dev, " bos=0x%08llx\n", args->bos);

	if(args->nr_bos == 0)
		return -EINVAL;

	handles = kvmalloc_array(args->nr_bos, sizeof(*handles), GFP_KERNEL);
	bos = kvmalloc_array(args->nr_bos, sizeof(*bos), GFP_KERNEL);
	cmdbuf = dnx_gpu_cmdbuf_new(dnx, args->nr_bos);
	if(!handles || !bos || !cmdbuf) {
		ret = -ENOMEM;
		goto error_handles;
	}

	ret = copy_from_user(handles, u64_to_user_ptr(args->bos),
			args->nr_bos * sizeof(*handles));
	if(ret) {
		ret = -EFAULT;
		goto error_handles;
	}

	ret = dnx_gpu_cmdbuf_lookup_objects(cmdbuf, file, handles, args->nr_bos);
	if(ret)
		goto error_handles;

	/* todo: remove when offset is computed in userspace */
	stream_addr = args->stream;

	/* Check if address of last jump lies within stream */
	for(i = 0; i < cmdbuf->nr_bos; ++i) {
		if((args->jump > cmdbuf->bos[i]->paddr) &&
		   (args->jump < (cmdbuf->bos[i]->paddr + cmdbuf->bos[i]->base.size))) {
			break;
		}
	}
	if(i == cmdbuf->nr_bos) {
		dev_err(dev->dev,
			"Error in stream data. Given jump address 0x%llx is not"
			" within stream.\n",
			args->jump);
		ret = -EFAULT;
		goto error_handles;
	}

	last_page = cmdbuf->bos[i];
	stream_jmpaddr = (void*) (last_page->vaddr + (args->jump - last_page->paddr));
	cmdbuf->paddr = stream_addr;
	cmdbuf->vjmpaddr = stream_jmpaddr;
	dev_dbg(dev->dev, " pstreamaddr=0x%08x vjmpaddr=0x%p\n", stream_addr, stream_jmpaddr);

	ret = dnx_gpu_submit(dev->dev_private, cmdbuf);
	args->fence = cmdbuf->fence;
	if(ret == 0)
		cmdbuf = NULL;

error_handles:
	/* if we still own the cmdbuf, we came here due to an error */
	if(cmdbuf)
		dnx_gpu_cmdbuf_free(cmdbuf);
	if(handles)
		kvfree(handles);
	if(bos)
		kvfree(bos);

	return ret;
}
