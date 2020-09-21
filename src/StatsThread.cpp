static const char *RcsId = "$Header: /home/cvsadm/cvsroot/fermi/servers/hdb++/hdb++es/src/StatsThread.cpp,v 1.6 2014-03-06 15:21:43 graziano Exp $";
//+=============================================================================
//
// file :         StatsThread.cpp
//
// description :  C++ source for thread management
// project :      TANGO Device Server
//
// $Author: graziano $
//
// $Revision: 1.6 $
//
// $Log: StatsThread.cpp,v $
//
//
//
// copyleft :     European Synchrotron Radiation Facility
//                BP 220, Grenoble 38043
//                FRANCE
//
//-=============================================================================

#include "StatsThread.h"
#include <HdbDevice.h>
#include "SubscribeThread.h"
#include "PushThread.h"


namespace HdbEventSubscriber_ns
{

    const unsigned int default_period = 60000;

    //=============================================================================
    //=============================================================================
    StatsThread::StatsThread(HdbDevice *dev): AbortableThread(dev->_device)
    {
        hdb_dev = dev;
        set_period(dev->period);
    }

    //=============================================================================
    //=============================================================================
    void StatsThread::init_abort_loop()
    {
        DEBUG_STREAM << "StatsThread id="<<omni_thread::self()->id()<<endl;
        hdb_dev->AttributeRecordFreq = -1;
        hdb_dev->AttributeFailureFreq = -1;
    }

    //=============================================================================
    //=============================================================================
    void StatsThread::run_thread_loop()
    {
        long ok_ev=0;
        long nok_ev=0;
        long nok_db=0;

        vector<string> attribute_list_tmp;
        hdb_dev->get_sig_list(attribute_list_tmp);

        //TODO: allocate AttributeRecordFreqList and AttributeFailureFreqList dynamically, but be careful to race conditions with read attribute
        /*if(hdb_dev->AttributeRecordFreqList != NULL)
          delete [] hdb_dev->AttributeRecordFreqList;
          hdb_dev->AttributeRecordFreqList = new Tango::DevDouble[attribute_list_tmp.size()];
          if(hdb_dev->AttributeFailureFreqList != NULL)
          delete [] hdb_dev->AttributeFailureFreqList;

          hdb_dev->AttributeFailureFreqList = new Tango::DevDouble[attribute_list_tmp.size()];*/

        for (size_t i=0 ; i<attribute_list_tmp.size() ; i++)
        {
            string signame(attribute_list_tmp[i]);
            /*try
              {
              hdb_dev->shared->veclock.readerIn();
              bool is_running = hdb_dev->shared->is_running(signame);
              hdb_dev->shared->veclock.readerOut();
              if(!is_running)
              continue;
              }catch(Tango::DevFailed &e)
              {
              continue;
              }*/

            long ok_ev_t=0;
            long nok_ev_t=0;
            long nok_db_t=0;
            ok_ev_t = hdb_dev->shared->get_ok_event_freq(signame);
            ok_ev += ok_ev_t;
            nok_ev_t = hdb_dev->shared->get_nok_event_freq(signame);
            nok_ev += nok_ev_t;
            nok_db_t = hdb_dev->push_thread->get_nok_db_freq(signame);
            nok_db += nok_db_t;
            hdb_dev->AttributeRecordFreqList[i] = ok_ev_t - nok_db_t;
            hdb_dev->AttributeFailureFreqList[i] = nok_ev_t + nok_db_t;
        }
        hdb_dev->AttributeRecordFreq = ok_ev - nok_db;
        hdb_dev->AttributeFailureFreq = nok_ev + nok_db;

        hdb_dev->push_events("AttributeRecordFreq", &hdb_dev->AttributeRecordFreq, true);
        hdb_dev->push_events("AttributeFailureFreq", &hdb_dev->AttributeFailureFreq, true);
        hdb_dev->push_events("AttributeRecordFreqList", &hdb_dev->AttributeRecordFreqList[0], attribute_list_tmp.size(), true);
        hdb_dev->push_events("AttributeFailureFreqList", &hdb_dev->AttributeFailureFreqList[0], attribute_list_tmp.size(), false);

        hdb_dev->reset_freq_statistics();
    }

    //=============================================================================
    //=============================================================================
    void StatsThread::finalize_abort_loop()
    {
        DEBUG_STREAM <<"StatsThread::"<< __func__<<": exiting..."<<endl;
    }

    //=============================================================================
    //=============================================================================
    auto StatsThread::get_abort_loop_period_ms() -> unsigned int
    {
        return default_period;
    }

}	//	namespace
