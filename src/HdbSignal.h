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
#include <atomic>
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

        std::atomic_uint idx;

        explicit HdbSignal(HdbDevice* dev, const std::string& name, const std::vector<std::string>& contexts);

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
            ReaderLock lk(dblock);
            return (evstate == Tango::ALARM && is_running()) || dbstate == Tango::ALARM;
        }

        auto is_on() -> bool
        {
            ReaderLock lock(siglock);
            return evstate == Tango::ON && is_running();
        }

        auto is_not_on_error() -> bool
        {
            ReaderLock lock(siglock);
            ReaderLock lk(dblock);
            return (evstate == Tango::ON || (evstate == Tango::ALARM && !is_running())) && dbstate != Tango::ALARM;
        }

        auto get_error() -> std::string;

        auto get_avg_store_time() -> std::chrono::duration<double>
        {
            ReaderLock lock(dblock);
            return store_time_avg;
        }

        auto get_min_store_time() -> std::chrono::duration<double>
        {
            ReaderLock lock(dblock);
            return store_time_min;
        }

        auto get_max_store_time() -> std::chrono::duration<double>
        {
            ReaderLock lock(dblock);
            return store_time_max;
        }

        auto get_avg_process_time() -> std::chrono::duration<double>
        {
            ReaderLock lock(dblock);
            return process_time_avg;
        }

        auto get_min_process_time() -> std::chrono::duration<double>
        {
            ReaderLock lock(dblock);
            return process_time_min;
        }

        auto get_max_process_time() -> std::chrono::duration<double>
        {
            ReaderLock lock(dblock);
            return process_time_max;
        }

        auto get_ok_events() -> unsigned int
        {
            return get_events(ok_events);
        }

        auto get_nok_events() -> unsigned int
        {
            return get_events(nok_events);
        }
        
        auto get_nok_db() -> unsigned int
        {
            return get_events(nokdb_events);
        }
        
        auto get_ok_db() -> unsigned int
        {
            return get_events(okdb_events);
        }

        auto get_total_events() -> unsigned int
        {
            return get_ok_events() + get_nok_events();
        }

        auto get_last_ok_event() -> std::chrono::time_point<std::chrono::system_clock>
        {
            return get_last_event(ok_events);
        }

        auto get_last_nok_event() -> std::chrono::time_point<std::chrono::system_clock>
        {
            return get_last_event(nok_events);
        }

        auto get_last_nok_db() -> std::chrono::time_point<std::chrono::system_clock>
        {
            return get_last_event(nokdb_events);
        }
        
        auto get_ok_events_freq() -> double
        {
            return get_events_freq(ok_events);
        }

        auto get_nok_events_freq() -> double
        {
            return get_events_freq(nok_events);
        }
        
        auto get_nok_db_freq() -> double
        {
            return get_events_freq(nokdb_events);
        }
        
        auto get_ok_db_freq() -> double
        {
            return get_events_freq(okdb_events);
        }

        auto set_ok_event() -> void;

        auto set_nok_event() -> void;

        auto set_nok_periodic_event() -> void;

        auto set_nok_db(const std::string& error) -> void;
        
        auto set_ok_db(std::chrono::duration<double> store_time, std::chrono::duration<double> process_time) -> void;

        auto get_status() -> std::string
        {
            ReaderLock lock(siglock);
            ReaderLock lk(dblock);
            std::stringstream ret;
            ret << status;
            if(dbstate == Tango::ALARM)
            {
                ret << std::endl << dberror;
            }
            return ret.str();
        }

        auto get_state() -> Tango::DevState
        {
            ReaderLock lock(siglock);
            ReaderLock lk(dblock);
            if(dbstate == Tango::ALARM)
                return Tango::ALARM;
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
            ok_events.reset();
            nok_events.reset();
            nokdb_events.reset();
            okdb_events.reset();
            
            WriterLock lock(dblock);
            store_time_avg = std::chrono::duration<double>::zero();
            store_time_min = std::chrono::duration<double>::min();
            store_time_max = std::chrono::duration<double>::min();
            process_time_avg = std::chrono::duration<double>::zero();
            process_time_min = std::chrono::duration<double>::min();
            process_time_max = std::chrono::duration<double>::min();
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

            std::atomic_uint counter;
            std::deque<std::chrono::time_point<std::chrono::system_clock>> timestamps;
            mutable std::mutex timestamps_mutex;

            EventCounter():counter(0)
            {
            }

            void increment()
            {
                ++counter;
                {
                    std::lock_guard<mutex> lk(timestamps_mutex);
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
            }

            public:
            double get_freq() const
            {
                std::lock_guard<mutex> lk(timestamps_mutex);
                return timestamps.size()/stats_window.count();
            }

            double get_freq_inst() const
            {
                std::lock_guard<mutex> lk(timestamps_mutex);
                if(timestamps.size() > 1)
                {
                    auto last_val = timestamps.back();
                    auto second_last = timestamps[timestamps.size() - 2];

                    return 1./(last_val - second_last).count();
                }
                return 0.;
            }

            auto get_last_event() const -> std::chrono::time_point<std::chrono::system_clock>
            {
                std::lock_guard<mutex> lk(timestamps_mutex);
                if(timestamps.size()>0)
                    return timestamps.back();
                struct std::chrono::time_point<std::chrono::system_clock> ret;
                return ret;
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
                                                         , abort(false)
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
                {
                    std::lock_guard<mutex> lk(m);
                    period = p;
                }
                notify();
            };

            HdbSignal& signal;
            std::unique_ptr<std::thread> periodic_check;
            std::chrono::milliseconds period;
            std::mutex m;
            std::condition_variable cv;
            std::atomic_bool abort;
            
            auto check_periodic_event() -> bool
            {
                return period > std::chrono::milliseconds::zero() && signal.is_on();
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
        
        // DB insertion stats and state
        EventCounter nokdb_events;
        EventCounter okdb_events;
        Tango::DevState dbstate;
        std::string dberror;
        std::chrono::duration<double> process_time_avg;
        std::chrono::duration<double> process_time_min;
        std::chrono::duration<double> process_time_max;
        std::chrono::duration<double> store_time_avg;
        std::chrono::duration<double> store_time_min;
        std::chrono::duration<double> store_time_max;
        
        bool running;
        bool paused;
        bool stopped;
        std::vector<std::string> contexts;
        std::vector<std::string> contexts_upper;
        unsigned int ttl;
        std::unique_ptr<PeriodicEventCheck> event_checker;

        std::function<void(unsigned int idx, bool ok, double freq)> update_freq_callback;
        std::function<void(unsigned int idx, bool ok, double freq)> update_freq_db_callback;
        std::function<void(unsigned int idx, std::chrono::duration<double> store, std::chrono::duration<double> process)> update_timing_callback;

        ReadersWritersLock siglock;
        ReadersWritersLock dblock;

        void unsubscribe_event(const int event_id);

        auto get_events(const EventCounter& c) -> unsigned int
        {
            return c.counter.load();
        }

        auto get_events_freq(const EventCounter& c) -> double
        {
            return c.get_freq();
        }

        auto get_events_freq_inst(const EventCounter& c) -> double
        {
            return c.get_freq_inst();
        }

        auto get_last_event(const EventCounter& c) -> std::chrono::time_point<std::chrono::system_clock>
        {
            return c.get_last_event();
        }

    };
};
#endif // HDBSIGNAL_H
