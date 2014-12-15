static const char *RcsId = "$Header: /home/cvsadm/cvsroot/fermi/servers/hdb++/hdb++es/src/CheckPeriodicThread.cpp,v 1.6 2014-03-06 15:21:43 graziano Exp $";
//+=============================================================================
//
// file :         CheckPeriodicThread.cpp
//
// description :  C++ source for thread management
// project :      TANGO Device Server
//
// $Author: graziano $
//
// $Revision: 1.6 $
//
// $Log: CheckPeriodicThread.cpp,v $
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
CheckPeriodicThread::CheckPeriodicThread(HdbDevice *dev)
{
	hdb_dev = dev;
	abortflag = false;
	last_check.tv_sec = 0;
	last_check.tv_nsec = 0;
	clock_gettime(CLOCK_MONOTONIC, &last_check);
}
//=============================================================================
//=============================================================================
void *CheckPeriodicThread::run_undetached(void *ptr)
{
	cout << "CheckPeriodicThread id="<<omni_thread::self()->id()<<endl;
	while(abortflag==false)
	{
		double min_time_to_timeout_ms = 0;
		try
		{
			min_time_to_timeout_ms = hdb_dev->shared->check_periodic_event_timeout(delay_tolerance_ms);
		}catch(Tango::DevFailed &e)
		{
		}
		//sleep 1 second more than the first expiration time calculated
		abort_sleep(1.0 + 0.001*min_time_to_timeout_ms);
	}
	cout <<"CheckPeriodicThread::"<< __func__<<": exiting..."<<endl;
	return NULL;
}
//=============================================================================
//=============================================================================
void CheckPeriodicThread::abort_sleep(double time)
{
	for (int i = 0; i < (time/0.1); i++) {
		if (abortflag)
			break;
		omni_thread::sleep(0,100000000);
	}
}



}	//	namespace
