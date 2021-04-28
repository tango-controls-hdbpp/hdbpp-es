//=============================================================================
//
// file :        PollerThread.tpp
//
// description : PollerThread template methods implementation.
//
// project :	Tango Device Server
//
// $Author: graziano $
//
// $Revision: 1.5 $
//
// $Log: PollerThread.h,v $
//
//
//
// copyleft :    European Synchrotron Radiation Facility
//               BP 220, Grenoble 38043
//               FRANCE
//
//=============================================================================

#ifndef _POLLER_THREAD_TPP
#define _POLLER_THREAD_TPP

#include "HdbDevice.h"
#include "Consts.h"

/**
 * @author	$Author: graziano $
 * @version	$Revision: 1.5 $
 */

//	constants definitions here.
//-----------------------------------------------

namespace HdbEventSubscriber_ns
{

    template<typename T>
    void PollerThread::push_events(const std::string attr_name, T* data)
    {
        try
        {
            (hdb_dev->_device)->push_change_event(attr_name, data);
            (hdb_dev->_device)->push_archive_event(attr_name, data);
        }
        catch(Tango::DevFailed &e){}

        // TODO is this needed ?
        usleep(1 * ms_to_us_factor);
    }

    template<typename T>
    void PollerThread::push_events(const std::string attr_name, T* data, long size)
    {
        try
        {
            (hdb_dev->_device)->push_change_event(attr_name, data, size);
            (hdb_dev->_device)->push_archive_event(attr_name, data, size);
        }
        catch(Tango::DevFailed &e){}

        // TODO is this needed ?
        usleep(1 * ms_to_us_factor);
    }

}// namespace_ns

#endif	// _POLLER_THREAD_TPP
