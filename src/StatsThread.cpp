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


#include <HdbDevice.h>


namespace HdbEventSubscriber_ns
{


//=============================================================================
//=============================================================================
StatsThread::StatsThread(HdbDevice *dev)
{
	hdb_dev = dev;
	abortflag = false;
	period  = dev->period;
	last_stat.tv_sec = 0;
	last_stat.tv_usec = 0;
}
//=============================================================================
//=============================================================================
void *StatsThread::run_undetached(void *ptr)
{
	hdb_dev->AttributeRecordFreq = -1;
	hdb_dev->AttributeFailureFreq = -1;
	while(abortflag==false)
	{
		if(period > 0)
			abort_sleep((double)period);
		else
			abort_sleep(60.0);

		long ok_ev=0;
		long nok_ev=0;
		long nok_db=0;

		vector<string> attribute_list_tmp = hdb_dev->get_sig_list();

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
			if(!hdb_dev->shared->is_running(signame))
				continue;
			long ok_ev_t=0;
			long nok_ev_t=0;
			long nok_db_t=0;
			ok_ev_t = hdb_dev->shared->get_ok_event_freq(signame);
			ok_ev += ok_ev_t;
			nok_ev_t = hdb_dev->shared->get_nok_event_freq(signame);
			nok_ev += nok_ev_t;
			nok_db_t = hdb_dev->push_shared->get_nok_db_freq(signame);
			nok_db += nok_db_t;
			hdb_dev->AttributeRecordFreqList[i] = ok_ev_t - nok_db_t;
			hdb_dev->AttributeFailureFreqList[i] = nok_ev_t + nok_db_t;
		}
		hdb_dev->AttributeRecordFreq = ok_ev - nok_db;
		hdb_dev->AttributeFailureFreq = nok_ev + nok_db;

		(hdb_dev->_device)->push_change_event("AttributeRecordFreq",&hdb_dev->AttributeRecordFreq);
		(hdb_dev->_device)->push_change_event("AttributeFailureFreq",&hdb_dev->AttributeFailureFreq);
		(hdb_dev->_device)->push_archive_event("AttributeRecordFreq",&hdb_dev->AttributeRecordFreq);
		(hdb_dev->_device)->push_archive_event("AttributeFailureFreq",&hdb_dev->AttributeFailureFreq);

		gettimeofday(&last_stat, NULL);
		hdb_dev->reset_freq_statistics();
	}
	cout <<"StatsThread::"<< __func__<<": exiting..."<<endl;
	return NULL;
}
//=============================================================================
//=============================================================================
void StatsThread::abort_sleep(double time)
{
	for (int i = 0; i < (time/0.1); i++) {
		if (abortflag)
			break;
		omni_thread::sleep(0,100000000);
	}
}



}	//	namespace
