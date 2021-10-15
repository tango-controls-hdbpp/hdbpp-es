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
    class SharedData;

    class HdbSignal : public Tango::LogAdapter
    {
        friend class SharedData;

        static constexpr int ERR = -1;

        public:
        std::string name;

        explicit HdbSignal(HdbDevice* dev, SharedData& vec, const std::string& name, const std::vector<std::string>& contexts, unsigned int ttl);

        ~HdbSignal();

        struct SignalConfig
        {
            int write_type;
            int max_dim_x;
            int max_dim_y;
            int data_type;
            Tango::AttrDataFormat data_format;
        };

        enum class SignalState
        {
            RUNNING, STOPPED, PAUSED
        };

        void remove_callback();

        void subscribe_events();

        auto is_current_context(const std::string& context) -> bool;        

        auto init() -> void;

        auto start() -> void;

        auto update_contexts(const std::vector<std::string>& ctxts) -> void;

        auto get_ttl() const -> unsigned int
        {
            ReaderLock lock(siglock);
            return ttl;
        }

        auto set_ttl(unsigned int ttl) -> void;

        auto is_running() const -> bool
        {
            ReaderLock lock(siglock);
            return state == SignalState::RUNNING; 
        }

        auto is_not_subscribed() const -> bool
        {
            ReaderLock lock(siglock);
            return event_id == ERR && !is_stopped();
        }

        auto get_config() const -> std::string;

        auto is_ZMQ() const -> bool
        {
            ReaderLock lock(siglock);
            return isZMQ;
        }

        auto is_on_error() const -> bool
        {
            ReaderLock lock(siglock);
            ReaderLock lk(dblock);
            return (evstate == Tango::ALARM && is_running()) || dbstate == Tango::ALARM;
        }

        auto is_on() const -> bool
        {
            ReaderLock lock(siglock);
            return evstate == Tango::ON && is_running();
        }

        auto is_not_on_error() const -> bool
        {
            ReaderLock lock(siglock);
            ReaderLock lk(dblock);
            return (evstate == Tango::ON || (evstate == Tango::ALARM && !is_running())) && dbstate != Tango::ALARM;
        }

        auto get_error() const -> std::string;

        auto get_avg_store_time() const -> std::chrono::duration<double>
        {
            ReaderLock lock(dblock);
            return store_time_avg;
        }

        auto get_min_store_time() const -> std::chrono::duration<double>
        {
            ReaderLock lock(dblock);
            return store_time_min;
        }

        auto get_max_store_time() const -> std::chrono::duration<double>
        {
            ReaderLock lock(dblock);
            return store_time_max;
        }

        auto get_avg_process_time() const -> std::chrono::duration<double>
        {
            ReaderLock lock(dblock);
            return process_time_avg;
        }

        auto get_min_process_time() const -> std::chrono::duration<double>
        {
            ReaderLock lock(dblock);
            return process_time_min;
        }

        auto get_max_process_time() const -> std::chrono::duration<double>
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

        auto get_ok_events_freq_inst() -> double
        {
            return get_events_freq_inst(ok_events);
        }

        auto get_nok_events_freq_inst() -> double
        {
            return get_events_freq_inst(nok_events);
        }
        
        auto get_nok_db_freq_inst() -> double
        {
            return get_events_freq_inst(nokdb_events);
        }
        
        auto get_ok_db_freq_inst() -> double
        {
            return get_events_freq_inst(okdb_events);
        }
        auto set_ok_event() -> void;

        auto set_nok_event() -> void;

        auto set_nok_periodic_event() -> void;

        auto set_nok_db(const std::string& error) -> void;
        
        auto set_ok_db(std::chrono::duration<double> store_time, std::chrono::duration<double> process_time) -> void;

        auto get_status() const -> std::string
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

        auto get_state() const -> Tango::DevState
        {
            ReaderLock lock(siglock);
            ReaderLock lk(dblock);
            if(dbstate == Tango::ALARM)
                return Tango::ALARM;
            return evstate;
        }

        auto get_contexts() const -> std::string;

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

        auto is_paused() const -> bool
        {
            ReaderLock lock(siglock);
            return state == SignalState::PAUSED; 
        }

        auto is_stopped() const -> bool
        {
            ReaderLock lock(siglock);
            return state == SignalState::STOPPED; 
        }

        auto set_running() -> void;

        auto set_paused() -> void;

        auto set_stopped() -> void;

        auto is_first() const -> bool
        {
            ReaderLock lock(siglock);
            return first;
        }

        auto is_first_err() const -> bool
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

        auto set_error(const std::string& err) -> void;

        auto set_on() -> void;

        auto check_periodic_event_timeout(std::chrono::milliseconds delay) -> void
        {
            if(is_on())
            {
                auto now = std::chrono::system_clock::now();
                auto last_ev = std::max(get_last_ok_event(), get_last_nok_event());
                if(now - last_ev > delay)
                {
                    set_nok_periodic_event();
                }
            }
        }

        private:

        class EventCounter
        {
            friend class HdbSignal;

            std::atomic_uint counter;
            std::deque<std::chrono::time_point<std::chrono::system_clock>> timestamps;
            mutable std::mutex timestamps_mutex;

            const HdbSignal& signal;

            EventCounter(const HdbSignal& sig):counter(0), signal(sig)
            {
            }

            void increment()
            {
                ++counter;
                auto now = check_timestamps_in_window();

                std::lock_guard<mutex> lk(timestamps_mutex);
                timestamps.push_back(now);
            }

            auto check_timestamps_in_window() -> std::chrono::time_point<std::chrono::system_clock>;

            public:
            double get_freq();

            double get_freq_inst()
            {
                check_timestamps_in_window();
                std::lock_guard<mutex> lk(timestamps_mutex);
                if(timestamps.size() > 1)
                {
                    auto last_val = timestamps.back();
                    auto second_last = timestamps[timestamps.size() - 2];

                    return 1./std::chrono::duration_cast<std::chrono::duration<double>>(last_val - second_last).count();
                }
                return 0.;
            }

            auto get_last_event() -> std::chrono::time_point<std::chrono::system_clock>
            {
                check_timestamps_in_window();
                std::lock_guard<mutex> lk(timestamps_mutex);
                if(timestamps.size()>0)
                    return timestamps.back();
                struct std::chrono::time_point<std::chrono::system_clock> ret;
                return ret;
            }

            void reset()
            {
                counter = 0;
                std::lock_guard<mutex> lk(timestamps_mutex);
                timestamps.clear();
            };


        };

        mutable ReadersWritersLock siglock;
        mutable ReadersWritersLock dblock;
        
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
        
        SignalState state;
        
        std::vector<std::string> contexts;
        std::vector<std::string> contexts_upper;
        unsigned int ttl;
        
        HdbDevice* dev;
        SharedData& container;


        void unsubscribe_event(const int event_id);

        auto get_events(const EventCounter& c) -> unsigned int
        {
            return c.counter.load();
        }

        auto get_events_freq(EventCounter& c) -> double
        {
            return c.get_freq();
        }

        auto get_events_freq_inst(EventCounter& c) -> double
        {
            return c.get_freq_inst();
        }

        auto get_last_event(EventCounter& c) -> std::chrono::time_point<std::chrono::system_clock>
        {
            return c.get_last_event();
        }

        /** Update the state if needed.
         *  Returns the previous state.
         * 
         **/
        auto update_state(const SignalState& new_state) -> SignalState;
    };
};
#endif // HDBSIGNAL_H
