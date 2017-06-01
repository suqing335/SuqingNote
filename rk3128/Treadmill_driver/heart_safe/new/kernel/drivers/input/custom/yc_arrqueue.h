#ifndef __YC_ARRQUEUE_H__
#define __YC_ARRQUEUE_H__
#include <linux/spinlock.h>
struct yc_queue {
	void *p;
	int front;
	int rear;
	int max;
	int size;
	spinlock_t irq_lock;
};

//API

struct yc_queue *queue_create(int size, int max);
int queue_isempty(struct yc_queue *handle);
int queue_isfull(struct yc_queue *handle);
int enqueue(struct yc_queue *handle, void *data);
int dequeue(struct yc_queue *handle, void *data);
int queue_front(struct yc_queue *handle, void *data);
void queue_destroy(struct yc_queue *handle);

#endif /* __ARRQUEUE_H */
