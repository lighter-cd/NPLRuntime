//-----------------------------------------------------------------------------
// Class:	File System Watcher
// Authors:	LiXizhi
// Emails:	LiXizhi@yeah.net
// Company: ParaEngine
// Date:	2010.3.29
// Desc: 
//-----------------------------------------------------------------------------
#include "ParaEngine.h"

#ifndef PARAENGINE_MOBILE

#include "FileSystemWatcher.h"
#include <boost/bind.hpp>

using namespace ParaEngine;


//////////////////////////////////////////////////////////////////////////
//
// CFileSystemWatcherService
//
//////////////////////////////////////////////////////////////////////////

ParaEngine::CFileSystemWatcherService::CFileSystemWatcherService()
 : m_io_service(new boost::asio::io_service()), m_io_service_work(new boost::asio::io_service::work(*m_io_service)), m_bIsStarted(false)
{
	
}

ParaEngine::CFileSystemWatcherService::~CFileSystemWatcherService()
{
}

CFileSystemWatcherService* ParaEngine::CFileSystemWatcherService::GetInstance()
{
	static CFileSystemWatcherService g_sington;
	return &g_sington;
}

CFileSystemWatcherPtr ParaEngine::CFileSystemWatcherService::GetDirWatcher( const std::string& name )
{
	file_watcher_map_t::iterator itCur =  m_file_watchers.find(name);
	if(itCur != m_file_watchers.end())
	{
		return itCur->second;
	}
	else
	{
		CFileSystemWatcherPtr pWatcher(new CFileSystemWatcher(name));
		m_file_watchers[name] = pWatcher;
		return pWatcher;
	}
}

int ParaEngine::CFileSystemWatcherService::DispatchEvents()
{
	int nCount = 0;
	if(IsStarted())
	{
		file_watcher_map_t::iterator itCur, itEnd = m_file_watchers.end();
		for (itCur = m_file_watchers.begin(); itCur != itEnd; ++itCur)
		{
			nCount += itCur->second->DispatchEvents();
		}
	}
	return nCount;
}

void ParaEngine::CFileSystemWatcherService::DeleteDirWatcher( const std::string& name )
{
	file_watcher_map_t::iterator itCur =  m_file_watchers.find(name);
	if(itCur != m_file_watchers.end())
	{
		if(itCur->second.use_count() == 1)
		{
			m_file_watchers.erase(itCur);
		}
		else
		{
			OUTPUT_LOG("warning: CFileSystemWatcherService::DeleteDirWatcher can not delete %s because there is some external references\n", name.c_str());
		}
	}
}

void ParaEngine::CFileSystemWatcherService::Clear()
{
	if(m_io_service_work)
	{
		m_io_service_work.reset();
		//m_io_service.stop();

		m_file_watchers.clear();
		if(m_work_thread)
		{
			m_work_thread->join();
		}
		m_io_service.reset();
	}
}

bool ParaEngine::CFileSystemWatcherService::Start()
{
	if(!IsStarted())
	{
		m_bIsStarted = true;
		if(!m_work_thread)
		{
			m_work_thread.reset(new boost::thread(boost::bind(&boost::asio::io_service::run, m_io_service.get())));
		}
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////
//
// CFileSystemWatcher
//
//////////////////////////////////////////////////////////////////////////

ParaEngine::CFileSystemWatcher::CFileSystemWatcher(const std::string& filename)
: boost::asio::dir_monitor(CFileSystemWatcherService::GetInstance()->GetIOService()), m_bDispatchInMainThread(true)
{
	async_monitor(boost::bind(&ParaEngine::CFileSystemWatcher::FileHandler, this, _1, _2));
	CFileSystemWatcherService::GetInstance()->Start();
	SetName(filename);
	OUTPUT_LOG("FileSystemWatcher: %s created\n", filename.c_str());
}

ParaEngine::CFileSystemWatcher::CFileSystemWatcher()
	:CFileSystemWatcher("default")
{

}

ParaEngine::CFileSystemWatcher::~CFileSystemWatcher()
{
	OUTPUT_LOG("FileSystemWatcher removed\n");
}

void ParaEngine::CFileSystemWatcher::FileHandler( const boost::system::error_code &ec, const boost::asio::dir_monitor_event &ev )
{
	if(!ec)
	{
		if(IsDispatchInMainThread())
		{
			m_file_event(ev);
		}
		else
		{
			ParaEngine::Lock lock_(m_mutex);
			m_msg_queue.push(ev);
		}
		// continuously polling
		async_monitor(boost::bind(&ParaEngine::CFileSystemWatcher::FileHandler, this, _1, _2));
	}
	else
	{
		std::string sError = ec.message();
		OUTPUT_LOG("warning: in ParaEngine::CFileSystemWatcher::FileHandler. msg is %s\n", sError.c_str());
	}
}

void ParaEngine::CFileSystemWatcher::SetDispatchInMainThread( bool bMainThread )
{
	m_bDispatchInMainThread = bMainThread;
}

bool ParaEngine::CFileSystemWatcher::IsDispatchInMainThread()
{
	return m_bDispatchInMainThread;
}

int ParaEngine::CFileSystemWatcher::DispatchEvents()
{
	int nCount = 0;
	
	if(IsDispatchInMainThread())
	{
		ParaEngine::Lock lock_(m_mutex);
		while (!m_msg_queue.empty())
		{
			m_file_event(m_msg_queue.front());
			m_msg_queue.pop();
			nCount ++;
		}
	}
	else
	{
		while (!m_msg_queue.empty()){
			m_msg_queue.pop();
		}
	}
	return nCount;
}

CFileSystemWatcher::FileSystemEvent_Connection_t ParaEngine::CFileSystemWatcher::AddEventCallback( FileSystemEvent_t::slot_type callback )
{
	return m_file_event.connect(callback);
}

bool ParaEngine::CFileSystemWatcher::add_directory( const std::string &dirname )
{
	bool bRes = true;
	try
	{
		boost::asio::dir_monitor::add_directory(dirname);
		OUTPUT_LOG("FileSystemWatcher %s begins monitoring all files in %s\n", m_name.c_str(), dirname.c_str());
	}
	catch (...)
	{
		OUTPUT_LOG("warning: failed adding dir %s to monitor \n", dirname.c_str());
		bRes = false;
	}
	return bRes;
}

bool ParaEngine::CFileSystemWatcher::remove_directory( const std::string &dirname )
{
	bool bRes = true;
	try
	{
		boost::asio::dir_monitor::remove_directory(dirname);
		OUTPUT_LOG("FileSystemWatcher %s stops monitoring in %s\n", m_name.c_str(), dirname.c_str());
	}
	catch (...)
	{
		OUTPUT_LOG("warning: failed removing dir %s to monitor \n", dirname.c_str());
		bRes = false;
	}
	return bRes;
}

const std::string& ParaEngine::CFileSystemWatcher::GetName() const
{
	return m_name;
}

void ParaEngine::CFileSystemWatcher::SetName(const std::string& val)
{
	m_name = val;
}

#endif