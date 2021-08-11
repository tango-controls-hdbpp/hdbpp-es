#include "HdbSignal.h"

namespace HdbEventSubscriber_ns
{

    void HdbSignal::remove_callback()
    {
        unsubscribe_event(event_id);
        unsubscribe_event(event_conf_id);

        siglock.writerIn();
        event_id = ERR;
        event_conf_id = ERR;
        archive_cb.reset(nullptr);
        attr.reset(nullptr);
        siglock.writerOut();
    }

    void HdbSignal::unsubscribe_event(const int event_id)
    {
        if(event_id != ERR && attr != nullptr)
        {
            //unlocking, locked in SharedData::stop but possible deadlock if unsubscribing remote attribute with a faulty event connection
            //siglock.writerOut();
            attr->unsubscribe_event(event_id);
            //siglock.writerIn();
        }
    }
}
