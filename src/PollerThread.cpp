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


#include <HdbDevice.h>
#include <HdbEventSubscriber.h>


namespace HdbEventSubscriber_ns
{


//=============================================================================
//=============================================================================
PollerThread::PollerThread(HdbDevice *dev): Tango::LogAdapter(dev->_device)
{
	hdb_dev = dev;
	abortflag = false;
	period  = dev->poller_period;
	DEBUG_STREAM <<__func__<< "period="<<period<<" dev->poller_period="<<dev->poller_period<<endl;
	last_stat.tv_sec = 0;
	last_stat.tv_usec = 0;
}
//=============================================================================
//=============================================================================
void *PollerThread::run_undetached(void *ptr)
{
	INFO_STREAM << "PollerThread id="<<omni_thread::self()->id()<<endl;
	hdb_dev->AttributeRecordFreq = -1;
	hdb_dev->AttributeFailureFreq = -1;
	while(abortflag==false)
	{
		//DEBUG_STREAM << "PollerThread sleeping period="<<period<<endl;
		if(period > 0)
			abort_sleep((double)period);
		else
			abort_sleep(3.0);
		if(abortflag)
			break;
		//DEBUG_STREAM << "PollerThread awake!"<<endl;

		//vector<string> attribute_list_tmp = hdb_dev->get_sig_list();

		//TODO: allocate AttributeRecordFreqList and AttributeFailureFreqList dynamically, but be careful to race conditions with read attribute
		/*if(hdb_dev->AttributeRecordFreqList != NULL)
			delete [] hdb_dev->AttributeRecordFreqList;
		hdb_dev->AttributeRecordFreqList = new Tango::DevDouble[attribute_list_tmp.size()];
		if(hdb_dev->AttributeFailureFreqList != NULL)
			delete [] hdb_dev->AttributeFailureFreqList;
		hdb_dev->AttributeFailureFreqList = new Tango::DevDouble[attribute_list_tmp.size()];*/

		try
		{
			(hdb_dev->_device)->push_change_event("AttributePendingNumber",&hdb_dev->AttributePendingNumber);
			(hdb_dev->_device)->push_archive_event("AttributePendingNumber",&hdb_dev->AttributePendingNumber);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeNumber",&hdb_dev->attr_AttributeNumber_read);
			(hdb_dev->_device)->push_archive_event("AttributeNumber",&hdb_dev->attr_AttributeNumber_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeStartedNumber",&hdb_dev->attr_AttributeStartedNumber_read);
			(hdb_dev->_device)->push_archive_event("AttributeStartedNumber",&hdb_dev->attr_AttributeStartedNumber_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		try
		{
			(hdb_dev->_device)->push_change_event("AttributePausedNumber",&hdb_dev->attr_AttributePausedNumber_read);
			(hdb_dev->_device)->push_archive_event("AttributePausedNumber",&hdb_dev->attr_AttributePausedNumber_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeStoppedNumber",&hdb_dev->attr_AttributeStoppedNumber_read);
			(hdb_dev->_device)->push_archive_event("AttributeStoppedNumber",&hdb_dev->attr_AttributeStoppedNumber_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeMaxPendingNumber",&hdb_dev->AttributeMaxPendingNumber);
			(hdb_dev->_device)->push_archive_event("AttributeMaxPendingNumber",&hdb_dev->AttributeMaxPendingNumber);
		}
		catch(Tango::DevFailed &e){}
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeMaxStoreTime",&hdb_dev->attr_AttributeMaxStoreTime_read);
			(hdb_dev->_device)->push_archive_event("AttributeMaxStoreTime",&hdb_dev->attr_AttributeMaxStoreTime_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeMinStoreTime",&hdb_dev->attr_AttributeMinStoreTime_read);
			(hdb_dev->_device)->push_archive_event("AttributeMinStoreTime",&hdb_dev->attr_AttributeMinStoreTime_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeMaxProcessingTime",&hdb_dev->attr_AttributeMaxProcessingTime_read);
			(hdb_dev->_device)->push_archive_event("AttributeMaxProcessingTime",&hdb_dev->attr_AttributeMaxProcessingTime_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeMinProcessingTime",&hdb_dev->attr_AttributeMinProcessingTime_read);
			(hdb_dev->_device)->push_archive_event("AttributeMinProcessingTime",&hdb_dev->attr_AttributeMinProcessingTime_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		try
		{
			(hdb_dev->_device)->push_change_event("Context",static_cast<HdbEventSubscriber *>(hdb_dev->_device)->attr_Context_read);
			(hdb_dev->_device)->push_archive_event("Context",static_cast<HdbEventSubscriber *>(hdb_dev->_device)->attr_Context_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);

		if (hdb_dev->shared->is_initialized())
		{
			hdb_dev->attr_AttributeOkNumber_read = hdb_dev->get_sig_not_on_error_num();
		}
		else
			hdb_dev->attr_AttributeOkNumber_read = 0;
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeOkNumber",&hdb_dev->attr_AttributeOkNumber_read);
			(hdb_dev->_device)->push_archive_event("AttributeOkNumber",&hdb_dev->attr_AttributeOkNumber_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);


		if (hdb_dev->shared->is_initialized())
		{
			hdb_dev->attr_AttributeNokNumber_read = hdb_dev->get_sig_on_error_num();
		}
		else
			hdb_dev->attr_AttributeNokNumber_read = 0;

		try
		{
			(hdb_dev->_device)->push_change_event("AttributeNokNumber",&hdb_dev->attr_AttributeNokNumber_read);
			(hdb_dev->_device)->push_archive_event("AttributeNokNumber",&hdb_dev->attr_AttributeNokNumber_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);

		bool changed = hdb_dev->get_lists(hdb_dev->attribute_list_str, hdb_dev->attribute_started_list_str, hdb_dev->attribute_paused_list_str, hdb_dev->attribute_stopped_list_str, hdb_dev->attribute_context_list_str);
		if(changed)
		{
			for (size_t i=0 ; i<hdb_dev->attribute_list_str.size() && i < MAX_ATTRIBUTES; i++)
				hdb_dev->attr_AttributeList_read[i] = const_cast<char*>(hdb_dev->attribute_list_str[i].c_str());
			hdb_dev->attribute_list_str_size = hdb_dev->attribute_list_str.size();
		}
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeList",&hdb_dev->attr_AttributeList_read[0], hdb_dev->attribute_list_str_size);
			(hdb_dev->_device)->push_archive_event("AttributeList",&hdb_dev->attr_AttributeList_read[0], hdb_dev->attribute_list_str_size);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		if(changed)
		{
			for (size_t i=0 ; i<hdb_dev->attribute_started_list_str.size() && i < MAX_ATTRIBUTES ; i++)
				hdb_dev->attr_AttributeStartedList_read[i] = const_cast<char*>(hdb_dev->attribute_started_list_str[i].c_str());
			hdb_dev->attribute_started_list_str_size = hdb_dev->attribute_started_list_str.size();
		}
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeStartedList",&hdb_dev->attr_AttributeStartedList_read[0], hdb_dev->attribute_started_list_str_size);
			(hdb_dev->_device)->push_archive_event("AttributeStartedList",&hdb_dev->attr_AttributeStartedList_read[0], hdb_dev->attribute_started_list_str_size);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		if(changed)
		{
			for (size_t i=0 ; i<hdb_dev->attribute_paused_list_str.size() && i < MAX_ATTRIBUTES ; i++)
				hdb_dev->attr_AttributePausedList_read[i] = const_cast<char*>(hdb_dev->attribute_paused_list_str[i].c_str());
			hdb_dev->attribute_paused_list_str_size = hdb_dev->attribute_paused_list_str.size();
		}
		try
		{
			(hdb_dev->_device)->push_change_event("AttributePausedList",&hdb_dev->attr_AttributePausedList_read[0], hdb_dev->attribute_paused_list_str_size);
			(hdb_dev->_device)->push_archive_event("AttributePausedList",&hdb_dev->attr_AttributePausedList_read[0], hdb_dev->attribute_paused_list_str_size);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		if(changed)
		{
			for (size_t i=0 ; i<hdb_dev->attribute_stopped_list_str.size() && i < MAX_ATTRIBUTES ; i++)
				hdb_dev->attr_AttributeStoppedList_read[i] = const_cast<char*>(hdb_dev->attribute_stopped_list_str[i].c_str());
			hdb_dev->attribute_stopped_list_str_size = hdb_dev->attribute_stopped_list_str.size();
		}
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeStoppedList",&hdb_dev->attr_AttributeStoppedList_read[0], hdb_dev->attribute_stopped_list_str_size);
			(hdb_dev->_device)->push_archive_event("AttributeStoppedList",&hdb_dev->attr_AttributeStoppedList_read[0], hdb_dev->attribute_stopped_list_str_size);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);
		if(changed)
		{
			for (size_t i=0 ; i<hdb_dev->attribute_context_list_str.size() && i < MAX_ATTRIBUTES ; i++)
				hdb_dev->attr_AttributeContextList_read[i] = const_cast<char*>(hdb_dev->attribute_context_list_str[i].c_str());
			hdb_dev->attribute_context_list_str_size = hdb_dev->attribute_context_list_str.size();
		}
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeStrategyList",&hdb_dev->attr_AttributeContextList_read[0], hdb_dev->attribute_context_list_str_size);
			(hdb_dev->_device)->push_archive_event("AttributeStrategyList",&hdb_dev->attr_AttributeContextList_read[0], hdb_dev->attribute_context_list_str_size);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);

		hdb_dev->get_sig_not_on_error_list(hdb_dev->attribute_ok_list_str);
		//changed = is_list_changed(hdb_dev->attribute_ok_list_str, hdb_dev->old_attribute_ok_list_str);
		//if(changed)
		{
			for (size_t i=0 ; i<hdb_dev->attribute_ok_list_str.size() && i < MAX_ATTRIBUTES ; i++)
				hdb_dev->attr_AttributeOkList_read[i] = const_cast<char*>(hdb_dev->attribute_ok_list_str[i].c_str());
			hdb_dev->attribute_ok_list_str_size = hdb_dev->attribute_ok_list_str.size();
		}
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeOkList",&hdb_dev->attr_AttributeOkList_read[0], hdb_dev->attribute_ok_list_str_size);
			(hdb_dev->_device)->push_archive_event("AttributeOkList",&hdb_dev->attr_AttributeOkList_read[0], hdb_dev->attribute_ok_list_str_size);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);

		hdb_dev->get_sig_on_error_list(hdb_dev->attribute_nok_list_str);
		//changed = is_list_changed(hdb_dev->attribute_nok_list_str, hdb_dev->old_attribute_nok_list_str);
		//if(changed)
		{
			for (size_t i=0 ; i<hdb_dev->attribute_nok_list_str.size() && i < MAX_ATTRIBUTES ; i++)
				hdb_dev->attr_AttributeNokList_read[i] = const_cast<char*>(hdb_dev->attribute_nok_list_str[i].c_str());
			hdb_dev->attribute_nok_list_str_size = hdb_dev->attribute_nok_list_str.size();
		}
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeNokList",&hdb_dev->attr_AttributeNokList_read[0], hdb_dev->attribute_nok_list_str_size);
			(hdb_dev->_device)->push_archive_event("AttributeNokList",&hdb_dev->attr_AttributeNokList_read[0], hdb_dev->attribute_nok_list_str_size);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);

		hdb_dev->get_sig_list_waiting(hdb_dev->attribute_pending_list_str);
		//changed = is_list_changed(hdb_dev->attribute_pending_list_str, hdb_dev->old_attribute_pending_list_str);
		//if(changed)
		{
			for (size_t i=0 ; i<hdb_dev->attribute_pending_list_str.size() && i < MAX_ATTRIBUTES; i++)
				hdb_dev->attr_AttributePendingList_read[i] = const_cast<char*>(hdb_dev->attribute_pending_list_str[i].c_str());
			hdb_dev->attribute_pending_list_str_size = hdb_dev->attribute_pending_list_str.size();
		}
		try
		{
			(hdb_dev->_device)->push_change_event("AttributePendingList",&hdb_dev->attr_AttributePendingList_read[0], hdb_dev->attribute_pending_list_str_size);
			(hdb_dev->_device)->push_archive_event("AttributePendingList",&hdb_dev->attr_AttributePendingList_read[0], hdb_dev->attribute_pending_list_str_size);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);

		changed = hdb_dev->get_error_list(hdb_dev->attribute_error_list_str);
		if(changed)
		{
			for (size_t i=0 ; i<hdb_dev->attribute_error_list_str.size() && i < MAX_ATTRIBUTES ; i++)
				hdb_dev->attr_AttributeErrorList_read[i] = const_cast<char*>(hdb_dev->attribute_error_list_str[i].c_str());
			hdb_dev->attribute_error_list_str_size = hdb_dev->attribute_error_list_str.size();
		}
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeErrorList",&hdb_dev->attr_AttributeErrorList_read[0], hdb_dev->attribute_error_list_str_size);
			(hdb_dev->_device)->push_archive_event("AttributeErrorList",&hdb_dev->attr_AttributeErrorList_read[0], hdb_dev->attribute_error_list_str_size);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);


		hdb_dev->get_event_number_list();
		try
		{
			(hdb_dev->_device)->push_change_event("AttributeEventNumberList",&hdb_dev->AttributeEventNumberList[0], hdb_dev->attr_AttributeNumber_read);
			(hdb_dev->_device)->push_archive_event("AttributeEventNumberList",&hdb_dev->AttributeEventNumberList[0], hdb_dev->attr_AttributeNumber_read);
		}
		catch(Tango::DevFailed &e){}
		usleep(1000);

	}
	INFO_STREAM <<"PollerThread::"<< __func__<<": exiting..."<<endl;
	return NULL;
}
//=============================================================================
bool PollerThread::is_list_changed(const vector<string> & newlist, vector<string> &oldlist)
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
//=============================================================================
void PollerThread::abort_sleep(double time)
{
	for (int i = 0; i < (time/0.1); i++) {
		if (abortflag)
			break;
		omni_thread::sleep(0,100000000);
	}
}



}	//	namespace
