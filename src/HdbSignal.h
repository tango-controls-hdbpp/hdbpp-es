#ifndef HDBSIGNAL_H
#define HDBSIGNAL_H

#include <deque>
#include <ctime>
#include <string>
#include <memory>
#include <vector>
#include "Consts.h"
#include <tango.h>

namespace HdbEventSubscriber_ns
{
    class ArchiveCB;
    class HdbSignal
    {
        public:
            static double stats_window;

            std::string name;
        private:
        class EventCounter
        {
            unsigned int counter;
            std::deque<timespec> timestamps;

            void increment()
            {
                ++counter;
                timespec now{};
                clock_gettime(CLOCK_MONOTONIC, &now);

                auto first = timestamps.front();
                double interval = now.tv_sec - first.tv_sec + (now.tv_nsec - first.tv_nsec)/s_to_ns_factor;
                if(interval > stats_window)
                    timestamps.pop_front();

                timestamps.push_back(now);
            }

            double get_freq()
            {
                return timestamps.size()/stats_window;
            }

            double get_freq_inst()
            {
                if(timestamps.size() > 1)
                {
                    timespec last_val = timestamps.back();
                    timespec second_last = timestamps[timestamps.size() - 2];

                    return 1./(last_val.tv_sec-second_last.tv_sec +(last_val.tv_nsec-second_last.tv_nsec)/s_to_ns_factor);
                }
                return 0.;
            }

            void reset()
            {
                counter = 0;
            };
        };

        std::string devname;
        std::string attname;
        std::string status;
        int data_type;
        Tango::AttrDataFormat data_format;
        int write_type;
        int max_dim_x;
        int max_dim_y;
        std::unique_ptr<Tango::AttributeProxy> attr;
        Tango::DevState evstate;
        bool first;
        bool first_err;
        std::unique_ptr<ArchiveCB> archive_cb;
        int event_id;
        int event_conf_id;
        bool isZMQ;
        EventCounter ok_events;
        EventCounter nok_events;
        timespec last_ev;
        int periodic_ev;
        bool running;
        bool paused;
        bool stopped;
        std::vector<std::string> contexts;
        std::vector<std::string> contexts_upper;
        unsigned int ttl;
        ReadersWritersLock siglock;
    };
};
#endif // HDBSIGNAL_H
