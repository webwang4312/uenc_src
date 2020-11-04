#ifndef _WORK_THREAD_H_
#define _WORK_THREAD_H_

#include <thread>
#include <vector>
#include <list>
#include <deque>
#include "./net_api.h"
#include "../utils/singleton.h"
#include "./msg_queue.h"
#include "../common/config.h"

class WorkThreads
{
public:
	WorkThreads() = default;
	~WorkThreads() = default;
	
	
	static int handle_net_read(const MsgData& data);
	static int handle_network_event(MsgData& data);
	static bool handle_net_write(const MsgData& data);
	static void work(int num);
	static void work_read(int id);
	static void work_write(int id);
	void start();

	
private:
	std::vector<std::thread> m_threads_work_list;
	std::vector<std::thread> m_threads_read_list;
	std::vector<std::thread> m_threads_trans_list;
};

#endif