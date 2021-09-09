#ifndef HDBSIGNAL_H
#define HDBSIGNAL_H

#include <deque>
#include <chrono>
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <tango.h>


namespace HdbEventSubscriber_ns
{
    class ArchiveCB;
    class HdbDevice;

    class HdbSignal : public Tango::LogAdapter
    {
        static constexpr int ERR = -1;

        public:
        static std::chrono::duration<double> stats_window;
        static std::chrono::milliseconds delay_periodic_event;

        std::string name;

        explicit HdbSignal(Tango::DeviceImpl* dev, const std::string& name, const std::vector<std::string>& contexts);

        struct SignalConfig
        {
            int write_type;
            int max_dim_x;
            int max_dim_y;
            int data_type;
            Tango::AttrDataFormat data_format;
        };

        void remove_callback();

        void subscribe_events(HdbDevice* dev);

        auto is_current_context(const std::string& context) -> bool;        

        auto init() -> void;

        auto start() -> void;

        auto update_contexts(const std::vector<std::string>& ctxts) -> void;

        auto get_ttl() -> unsigned int
        {
            ReaderLock lock(siglock);
            return ttl;
        }

        auto set_ttl(unsigned int ttl) -> void
        {
            WriterLock lock(siglock);
            this->ttl = ttl;
        }

        auto is_running() -> bool
        {
            ReaderLock lock(siglock);
            return running; 
        }

        auto is_not_subscribed() -> bool
        {
            ReaderLock lock(siglock);
            return event_id == ERR && !is_stopped();
        }

        auto get_config() -> std::string;

        auto is_ZMQ() -> bool
        {
            ReaderLock lock(siglock);
            return isZMQ;
        }

        auto is_on_error() -> bool
        {
            ReaderLock lock(siglock);
            return evstate == Tango::ALARM && is_running();
        }

        auto is_on() -> bool
        {
            ReaderLock lock(siglock);
            return evstate == Tango::ON && is_running();
        }

        auto is_not_on_error() -> bool
        {
            ReaderLock lock(siglock);
            return evstate == Tango::ON || (evstate == Tango::ALARM && !is_running());
        }

        auto get_error() -> std::string;

        auto get_ok_events() -> unsigned int
        {
            return get_events(ok_events);
        }

        auto get_nok_events() -> unsigned int
        {
            return get_events(nok_events);
        }

        auto get_total_events() -> unsigned int
        {
            ReaderLock lock(siglock);
            return ok_events.counter + nok_events.counter;
        }

        auto get_last_ok_event() -> std::chrono::time_point<std::chrono::system_clock>
        {
            return get_last_event(ok_events);
        }

        auto get_last_nok_event() -> std::chrono::time_point<std::chrono::system_clock>
        {
            return get_last_event(nok_events);
        }

        auto get_ok_events_freq() -> double
        {
            return get_events_freq(ok_events);
        }

        auto get_nok_events_freq() -> double
        {
            return get_events_freq(nok_events);
        }

        auto set_ok_event() -> void;

        auto set_nok_event() -> void;

        auto set_nok_periodic_event() -> void;

        auto get_status() -> std::string
        {
            ReaderLock lock(siglock);
            return status;
        }

        auto get_state() -> Tango::DevState
        {
            ReaderLock lock(siglock);
            return evstate;
        }

        auto get_contexts() -> std::string;

        auto set_periodic_event(int p) -> void
        {
            WriterLock lock(siglock);
            event_checker->set_period(std::chrono::milliseconds(p));
        }

        auto reset_statistics() -> void
        {
            WriterLock lock(siglock);
            ok_events.reset();
            nok_events.reset();
        }

        auto reset_freq_statistics() -> void
        {
            WriterLock lock(siglock);
            ok_events.timestamps.clear();
            nok_events.timestamps.clear();
        }

        auto get_signal_config() -> SignalConfig;

        auto is_paused() -> bool
        {
            ReaderLock lock(siglock);
            return paused; 
        }

        auto is_stopped() -> bool
        {
            ReaderLock lock(siglock);
            return stopped; 
        }

        auto set_running() -> void
        {
            {
                WriterLock lock(siglock);
                running = true;
                paused = false;
                stopped = false;
            }
            event_checker->notify();
        }

        auto set_paused() -> void
        {
            {
                WriterLock lock(siglock);
                running = false;
                paused = true;
                stopped = false;
            }
            event_checker->notify();
        }

        auto set_stopped() -> void
        {
            {
                WriterLock lock(siglock);
                running = false;
                paused = false;
                stopped = true;
            }
            event_checker->notify();
        }

        auto is_first() -> bool
        {
            ReaderLock lock(siglock);
            return first;
        }

        auto is_first_err() -> bool
        {
            ReaderLock lock(siglock);
            return first_err;
        }

        auto set_first() -> void
        {
            WriterLock lock(siglock);
            first = false;
        }

        auto set_first_err() -> void
        {
            WriterLock lock(siglock);
            first_err = false;
        }

        auto set_error(const std::string& err) -> void
        {
            {
                WriterLock lock(siglock);
                evstate = Tango::ALARM;
                status = err;
            }
            event_checker->notify();
        }

        auto set_on() -> void
        {
            {
                WriterLock lock(siglock);
                evstate = Tango::ON;
                status = "Subscribed";
            }
            event_checker->notify();
        }

        private:
        class EventCounter
        {
            friend class HdbSignal;

            unsigned int counter;
            std::deque<std::chrono::time_point<std::chrono::system_clock>> timestamps;

            void increment()
            {
                ++counter;
                auto now = std::chrono::system_clock::now();

                if(timestamps.size()>0)
                {
                    auto first = timestamps.front();
                    std::chrono::duration<double> interval = now - first;
                    if(interval > stats_window)
                        timestamps.pop_front();
                }
                timestamps.push_back(now);
            }

            public:
            double get_freq() const
            {
                return timestamps.size()/stats_window.count();
            }

            double get_freq_inst() const
            {
                if(timestamps.size() > 1)
                {
                    auto last_val = timestamps.back();
                    auto second_last = timestamps[timestamps.size() - 2];

                    return 1./(last_val - second_last).count();
                }
                return 0.;
            }

            void reset()
            {
                counter = 0;
            };
        };

        class PeriodicEventCheck
        {

            friend class HdbSignal;

            public:
            explicit PeriodicEventCheck(HdbSignal& sig): signal(sig)
            {
                period = std::chrono::milliseconds::min();
                periodic_check = std::make_unique<std::thread>(&PeriodicEventCheck::check_periodic_event_timeout, this);
                periodic_check->detach();
            }

            ~PeriodicEventCheck()
            {
                abort = true;
                cv.notify_one();
                if(periodic_check)
                    periodic_check->join();
            }
            
            void check_periodic_event_timeout();

            void notify()
            {
                cv.notify_one();
            };

            auto set_period(std::chrono::milliseconds p) -> void
            {
                period = p;
                notify();
            };

            HdbSignal& signal;
            std::unique_ptr<std::thread> periodic_check;
            std::chrono::milliseconds period;
            std::mutex m;
            std::condition_variable cv;
            bool abort = false;
            auto check_periodic_event() -> bool
            {
                return period > std::chrono::milliseconds::min() && signal.is_on();
            };
        };

        std::string devname;
        std::string attname;
        SignalConfig config;
        bool config_set;
        std::string status;
        std::unique_ptr<Tango::AttributeProxy> attr;
        Tango::DevState evstate;
        bool first_err;
        bool first;
        std::unique_ptr<ArchiveCB> archive_cb;
        int event_id;
        int event_conf_id;
        bool isZMQ;
        EventCounter ok_events;
        EventCounter nok_events;
        bool running;
        bool paused;
        bool stopped;
        std::vector<std::string> contexts;
        std::vector<std::string> contexts_upper;
        unsigned int ttl;
        std::unique_ptr<PeriodicEventCheck> event_checker;
        ReadersWritersLock siglock;

        void unsubscribe_event(const int event_id);

        auto get_events(const EventCounter& c) -> unsigned int
        {
            ReaderLock lock(siglock);
            return c.counter;
        }

        auto get_events_freq(const EventCounter& c) -> double
        {
            ReaderLock lock(siglock);
            return c.get_freq();
        }

        auto get_events_freq_inst(const EventCounter& c) -> double
        {
            ReaderLock lock(siglock);
            return c.get_freq_inst();
        }

        auto get_last_event(const EventCounter& c) -> std::chrono::time_point<std::chrono::system_clock>
        {
            ReaderLock lock(siglock);
            if(c.timestamps.size()>0)
                return c.timestamps.back();
            struct std::chrono::time_point<std::chrono::system_clock> ret;
            return ret;
        }

    };
};
#endif // HDBSIGNAL_H
