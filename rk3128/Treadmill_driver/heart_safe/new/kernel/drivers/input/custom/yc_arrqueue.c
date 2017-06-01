#include "yc_arrqueue.h"
#include <linux/stddef.h>
#include <linux/slab.h>
struct yc_queue *queue_create(int size, int max)
{
    	struct yc_queue *handle = NULL;

	handle = kzalloc(sizeof(struct yc_queue), GFP_KERNEL);
	if(!handle)
		goto err0;
	handle->p = kzalloc(size * (max + 1), GFP_KERNEL);
	if(!handle->p)
		goto err1;

	handle->front = 0;
	handle->rear = 0;
	handle->max = max + 1;
	handle->size = size;

	//spin_lock_init(&handle->irq_lock); 

	return handle;

err1:
    kfree(handle);
err0:
    return NULL;
}

int queue_isempty(struct yc_queue *handle)
{
    return handle->front == handle->rear;
}

int queue_isfull(struct yc_queue *handle)
{
    return (handle->rear + 1) % handle->max == handle->front;
}

int enqueue(struct yc_queue *handle, void *data)
{
	//unsigned long irqflags;
	if(queue_isfull(handle))
		return -1;
	//spin_lock_irqsave(&handle->irq_lock, irqflags);
	memcpy(handle->p + handle->size * handle->rear++, data, handle->size);
	handle->rear %= handle->max;
	//spin_unlock_irqrestore(&handle->irq_lock, irqflags);
    	return 0;
}

int dequeue(struct yc_queue *handle, void *data)
{
	//unsigned long irqflags;
	if(queue_isempty(handle))
		return -1;

	//spin_lock_irqsave(&handle->irq_lock, irqflags);
	memcpy(data, handle->p + handle->size * handle->front++, handle->size);
	handle->front %= handle->max;
	//spin_unlock_irqrestore(&handle->irq_lock, irqflags);

	return 0;
}

int queue_front(struct yc_queue *handle, void *data)
{
    if(queue_isempty(handle))
        return -1;

    memcpy(data, handle->p + handle->size * handle->front,
            handle->size);

    return 0;
}

void queue_destroy(struct yc_queue *handle)
{
    kfree(handle->p);
    kfree(handle);
}

