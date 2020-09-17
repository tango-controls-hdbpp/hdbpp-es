static const char *RcsId = "$Header: /home/cvsadm/cvsroot/fermi/servers/hdb++/hdb++es/src/PollerThread.cpp,v 1.6 2014-03-06 15:21:43 graziano Exp $";
//+=============================================================================
//
// file :         PollerThread.cpp
//
// description :  C++ source for thread management
// project :      TANGO Device Server
//
// $Author: graziano $
//
// $Revision: 1.6 $
//
// $Log: PollerThread.cpp,v $
//
//
//
// copyleft :     European Synchrotron Radiation Facility
//                BP 220, Grenoble 38043
//                FRANCE
//
//-=============================================================================

#include "PollerThread.h"
#include <HdbDevice.h>
#include <HdbEventSubscriber.h>
#include "SubscribeThread.h"

namespace HdbEventSubscriber_ns
{
    const unsigned int default_period = 3000;

    //=============================================================================
    //=============================================================================
    PollerThread::PollerThread(HdbDevice *dev): AbortableThread(dev->_device)
    {
        hdb_dev = dev;
        set_period(dev->poller_period);
        DEBUG_STREAM <<__func__<< "period="<< get_period() <<" dev->poller_period="<<dev->poller_period<<endl;
    }
    
    //=============================================================================
    //=============================================================================
    void PollerThread::init_abort_loop()
    {
        INFO_STREAM << "PollerThread id="<<omni_thread::self()->id()<<endl;
        hdb_dev->AttributeRecordFreq = -1;
        hdb_dev->AttributeFailureFreq = -1;
    }

    //=============================================================================
    //=============================================================================
    void PollerThread::run_thread_loop()
    {
        //DEBUG_STREAM << "PollerThread awake!"<<endl;

        //vector<string> attribute_list_tmp = hdb_dev->get_sig_list();

        //TODO: allocate AttributeRecordFreqList and AttributeFailureFreqList dynamically, but be careful to race conditions with read attribute
        /*if(hdb_dev->AttributeRecordFreqList != NULL)

          delete [] hdb_dev->AttributeRecordFreqList;
          hdb_dev->AttributeRecordFreqList = new Tango::DevDouble[attribute_list_tmp.size()];
          if(hdb_dev->AttributeFailureFreqList != NULL)
          delete [] hdb_dev->AttributeFailureFreqList;
          hdb_dev->AttributeFailureFreqList = new Tango::DevDouble[attribute_list_tmp.size()];*/
        
        hdb_dev->push_events("AttributePendingNumber", &hdb_dev->AttributePendingNumber, true);
        hdb_dev->push_events("AttributeNumber", &hdb_dev->attr_AttributeNumber_read, true);
        hdb_dev->push_events("AttributeStartedNumber", &hdb_dev->attr_AttributeStartedNumber_read, true);
        hdb_dev->push_events("AttributePausedNumber", &hdb_dev->attr_AttributePausedNumber_read, true);
        hdb_dev->push_events("AttributeStoppedNumber", &hdb_dev->attr_AttributeStoppedNumber_read, true);
        hdb_dev->push_events("AttributeMaxPendingNumber", &hdb_dev->AttributeMaxPendingNumber, true);
        hdb_dev->push_events("AttributeMaxStoreTime", &hdb_dev->attr_AttributeMaxStoreTime_read, true);
        hdb_dev->push_events("AttributeMinStoreTime", &hdb_dev->attr_AttributeMinStoreTime_read, true);
        hdb_dev->push_events("AttributeMaxProcessingTime", &hdb_dev->attr_AttributeMaxProcessingTime_read, true);
        hdb_dev->push_events("AttributeMinProcessingTime", &hdb_dev->attr_AttributeMinProcessingTime_read, true);
        hdb_dev->push_events("Context", dynamic_cast<HdbEventSubscriber *>(hdb_dev->_device)->attr_Context_read, true);

        if (hdb_dev->shared->is_initialized())
        {
            hdb_dev->attr_AttributeOkNumber_read = hdb_dev->get_sig_not_on_error_num();
            hdb_dev->attr_AttributeNokNumber_read = hdb_dev->get_sig_on_error_num();
        }
        else
        {
            hdb_dev->attr_AttributeOkNumber_read = 0;
            hdb_dev->attr_AttributeNokNumber_read = 0;
        }
        
        hdb_dev->push_events("AttributeOkNumber", &hdb_dev->attr_AttributeOkNumber_read, true);
        hdb_dev->push_events("AttributeNokNumber", &hdb_dev->attr_AttributeNokNumber_read, true);

        bool changed = hdb_dev->get_lists(hdb_dev->attribute_list_str, hdb_dev->attribute_started_list_str, hdb_dev->attribute_paused_list_str, hdb_dev->attribute_stopped_list_str, hdb_dev->attribute_context_list_str, hdb_dev->attr_AttributeTTLList_read);
        if(changed)
        {
            update_array(hdb_dev->attr_AttributeList_read, hdb_dev->attribute_list_str_size, hdb_dev->attribute_list_str);
            update_array(hdb_dev->attr_AttributeStartedList_read, hdb_dev->attribute_started_list_str_size, hdb_dev->attribute_started_list_str);
            
            update_array(hdb_dev->attr_AttributePausedList_read, hdb_dev->attribute_paused_list_str_size, hdb_dev->attribute_paused_list_str);
            
            update_array(hdb_dev->attr_AttributeStoppedList_read, hdb_dev->attribute_stopped_list_str_size, hdb_dev->attribute_stopped_list_str);
            
            update_array(hdb_dev->attr_AttributeContextList_read, hdb_dev->attribute_context_list_str_size, hdb_dev->attribute_context_list_str);
        }


        hdb_dev->push_events("AttributeList", &hdb_dev->attr_AttributeList_read[0], hdb_dev->attribute_list_str_size, true);
        hdb_dev->push_events("AttributeStartedList", &hdb_dev->attr_AttributeStartedList_read[0], hdb_dev->attribute_started_list_str_size, true);
        hdb_dev->push_events("AttributePausedList", &hdb_dev->attr_AttributePausedList_read[0], hdb_dev->attribute_paused_list_str_size, true);
        hdb_dev->push_events("AttributeStoppedList", &hdb_dev->attr_AttributeStoppedList_read[0], hdb_dev->attribute_stopped_list_str_size, true);
        hdb_dev->push_events("AttributeStrategyList", &hdb_dev->attr_AttributeContextList_read[0], hdb_dev->attribute_context_list_str_size, true);

        hdb_dev->push_events("AttributeTTLList", &hdb_dev->attr_AttributeTTLList_read[0], hdb_dev->attribute_list_str_size, true);

        hdb_dev->get_sig_not_on_error_list(hdb_dev->attribute_ok_list_str);

        update_array(hdb_dev->attr_AttributeOkList_read, hdb_dev->attribute_ok_list_str_size, hdb_dev->attribute_ok_list_str);
        
        hdb_dev->push_events("AttributeOkList", &hdb_dev->attr_AttributeOkList_read[0], hdb_dev->attribute_ok_list_str_size, true);

        hdb_dev->get_sig_on_error_list(hdb_dev->attribute_nok_list_str);
       
        update_array(hdb_dev->attr_AttributeNokList_read, hdb_dev->attribute_nok_list_str_size, hdb_dev->attribute_nok_list_str);
        
        hdb_dev->push_events("AttributeNokList", &hdb_dev->attr_AttributeNokList_read[0], hdb_dev->attribute_nok_list_str_size, true);

        hdb_dev->get_sig_list_waiting(hdb_dev->attribute_pending_list_str);
        
        update_array(hdb_dev->attr_AttributePendingList_read, hdb_dev->attribute_pending_list_str_size, hdb_dev->attribute_pending_list_str);
        
        hdb_dev->push_events("AttributePendingList", &hdb_dev->attr_AttributePendingList_read[0], hdb_dev->attribute_pending_list_str_size, true);

        changed = hdb_dev->get_error_list(hdb_dev->attribute_error_list_str);
        
        if(changed)
        {
            update_array(hdb_dev->attr_AttributeErrorList_read, hdb_dev->attribute_error_list_str_size, hdb_dev->attribute_error_list_str);
        }
        
        hdb_dev->push_events("AttributeErrorList", &hdb_dev->attr_AttributeErrorList_read[0], hdb_dev->attribute_error_list_str_size, true);

        hdb_dev->get_event_number_list();
        
        hdb_dev->push_events("AttributeEventNumberList", &hdb_dev->AttributeEventNumberList[0], hdb_dev->attr_AttributeNumber_read, true);
    }

    //=============================================================================
    //=============================================================================
    void PollerThread::finalize_abort_loop()
    {
        INFO_STREAM <<"PollerThread::"<< __func__<<": exiting..."<<endl;
    }

    //=============================================================================
    //=============================================================================
    auto PollerThread::get_abort_loop_period_ms() -> unsigned int
    {
        return default_period;
    }

    //=============================================================================
    auto PollerThread::is_list_changed(const vector<string> & newlist, vector<string> &oldlist) -> bool
    {
        bool ret=false;
        if(newlist.size() != oldlist.size())
        {
            oldlist = newlist;
            return true;
        }
        for(size_t i=0; i < newlist.size(); i++)
        {
            if(newlist[i] != oldlist[i])
            {
                ret = true;
                oldlist = newlist;
                break;
            }

        }
        return ret;
    }

    void PollerThread::update_array(Tango::DevString (&out)[MAX_ATTRIBUTES], size_t& out_size, const vector<string>& in)
    {
        for (size_t i=0 ; i < in.size() && i < MAX_ATTRIBUTES; i++)
            out[i] = const_cast<char*>(in[i].c_str());
        out_size = in.size();
    }
}	//	namespace
