#include "sal.h"

struct sal_event
{
    spinlock_t lock;
    wait_queue_head_t wq;
    bool signaled;
    bool auto_reset;
};

sal_err_t
ctc_sal_event_create(sal_event_t** pevt, bool auto_reset)
{
    sal_event_t* event;

    SAL_MALLOC(event, sal_event_t*, sizeof(sal_event_t));
    if (!event)
    {
        return ENOMEM;
    }

    spin_lock_init(&event->lock);
    init_waitqueue_head(&event->wq);
    event->signaled = FALSE;
    event->auto_reset = auto_reset;

    *pevt = event;
    return 0;
}

void
ctc_sal_event_destroy(sal_event_t* event)
{
    SAL_FREE(event);
}

void
ctc_sal_event_set(sal_event_t* event)
{
    unsigned long flags;

    spin_lock_irqsave(&event->lock, flags);
    if (!event->signaled)
    {
        event->signaled = TRUE;
        spin_unlock_irqrestore(&event->lock, flags);
        wake_up_interruptible(&event->wq);

        return;
    }

    spin_unlock_irqrestore(&event->lock, flags);
}

void
ctc_sal_event_reset(sal_event_t* event)
{
    unsigned long flags;

    spin_lock_irqsave(&event->lock, flags);
    if (event->signaled)
    {
        event->signaled = FALSE;
    }

    spin_unlock_irqrestore(&event->lock, flags);
}

bool
ctc_sal_event_wait(sal_event_t* event, int timeout)
{
    DEFINE_WAIT(wait);
    unsigned long flags;

    if (timeout > 0)
    {
        timeout = msecs_to_jiffies(timeout);
    }

    spin_lock_irqsave(&event->lock, flags);

    while (!event->signaled)
    {
        prepare_to_wait(&event->wq, &wait, TASK_INTERRUPTIBLE);
        spin_unlock_irqrestore(&event->lock, flags);

        if (timeout < 0)
        {
            schedule();
        }
        else if (timeout > 0)
        {
            timeout = schedule_timeout(timeout);
        }

        if (timeout == 0)
        {
            finish_wait(&event->wq, &wait);

            return FALSE;
        }

        finish_wait(&event->wq, &wait);
        spin_lock_irqsave(&event->lock, flags);
    }

    if (event->auto_reset)
    {
        event->signaled = FALSE;
    }

    spin_unlock_irqrestore(&event->lock, flags);

    return TRUE;
}

