//============================================================================
// Name        : ServerPos.cpp
// Author      : wangqi
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <stdio.h>
//#include <unistd.h>
//#include <string.h>
#include "BATNetSDKRawAPI.h"
#include "SZTBGPSAPI.h"
#include <vector>
#include <boost/bind.hpp>
#include "base/Thread.h"
#include "base/Mutex.h"
#include "base/Condition.h"
using namespace std;


int handler_Server = -1;
vector<T_CCMS_POSITION_DATA>  PosVector;
muduo::MutexLock m_mutex;
muduo::Condition m_condMsg(m_mutex);


void SendToServer(void)
{
	vector<T_CCMS_POSITION_DATA> newVec;
	vector<T_CCMS_POSITION_DATA>::iterator it;
	T_CCMS_POSITION_DATA Position;
	while (1)
	{
		{
			muduo::MutexLockGuard m_mutexguard(m_mutex);
			if (PosVector.empty())
			{
				m_condMsg.waitForMillSeconds(10);
				continue;
			}
			else
			{
				newVec.swap(PosVector);
			}
		}

		  for(it = newVec.begin();it != newVec.end();++it)
		  {
		   Position = *it;
		   //printf("count=%llu 牌号：%s\t\tSIM卡号：%s\t\t纬度：%u\t\t经度：%u\t\t高度：%u\t\t速度：%u\t\t方向：%u\t\t状态：%u\t\t报警类型：%u\t\t车牌颜色：%u\t\t时间：%u--%u--%u--%u--%u--%u\n",count,
		   //	Position.chVehicleID,Position.chSimCard,Position.nLatitude,Position.nLongitude,Position.nHeight,Position.nSpeed,Position.nDirect,Position.nStatus,Position.nAlarmType,
		   //	Position.nVehicleColor,Position.stDateTime.nYear,Position.stDateTime.nMonth,Position.stDateTime.nDay,Position.stDateTime.nHour,Position.stDateTime.nMinute,Position.stDateTime.nSecond);
		   char buf[sizeof(Position)] = {0};
		   int Length = Msg_Packet(buf,&Position,sizeof(Position));
		   BATNetSDKRaw_Send(handler_Server,0,buf,Length);
		  }
                  newVec.clear();
		//break;
	}


}


int OnRecv(int sessionID,const char* buf,int len, void* userdata)
{
	int length = 0;
	T_CCMS_POSITION_DATA Position;

	memset(&Position,0,sizeof(T_CCMS_POSITION_DATA));
	length = ParsePositionInfo((UInt8*)buf,len,Position);
	bool isEmpty = true;
	for (int i = 0; i < 16; i++)
	{
		if (Position.chVehicleID[i] != 0)
		{
			isEmpty = false;
			break;
		}
	}
	if (isEmpty)
	{
		if (Position.nDeviceID != 0)
		{
			isEmpty = false;
		}
	}
	if (length > 0 && !isEmpty)
	{
		static UInt64 count = 0;
        	count++;
		if (strlen((char*)Position.chVehicleID) != 9 || Position.nLatitude > 90000000 || Position.nLongitude > 180000000 || Position.nDirect > 360 || Position.nDeviceID == 0
			|| Position.stDateTime.nYear != 2016 || Position.stDateTime.nMonth <= 0 || Position.stDateTime.nMonth > 12 || Position.stDateTime.nDay <= 0 || Position.stDateTime.nDay > 31
                	|| Position.stDateTime.nHour >= 24 || Position.stDateTime.nMinute >= 60 || Position.stDateTime.nSecond >= 60)
        	{
                	printf("count=%-9llu len=%-3d 牌号：%-11s 设备号：%-7d SIM卡号：%-11s 纬度：%-9u 经度：%-9u 高度：%u 速度：%-3u 方向：%-3u 状态：%u 报警类型：%u 车牌颜色：%u 时间：%04u-%02u-%02u %02u:%02u:%02u\n",count,length,
	                   	Position.chVehicleID,Position.nDeviceID,Position.chSimCard,Position.nLatitude,Position.nLongitude,Position.nHeight,Position.nSpeed,Position.nDirect,Position.nStatus,Position.nAlarmType,
        	           	Position.nVehicleColor,Position.stDateTime.nYear,Position.stDateTime.nMonth,Position.stDateTime.nDay,Position.stDateTime.nHour,Position.stDateTime.nMinute,Position.stDateTime.nSecond);
                }
		muduo::MutexLockGuard m_mutexguard(m_mutex);
		PosVector.push_back(Position);
		m_condMsg.notify();
	}
	//sleep(10000);
	return length;
}



int main() {
	int handle_local = -1;
	CCMS_NETADDR LocalAddr = {{0}, 0};
	string ip_local = "192.168.40.40";
	memcpy(LocalAddr.chIP,ip_local.c_str(),ip_local.length());
	LocalAddr.nPort = 4456;

	string DevSN = "100100000001";
	char strDevSN[13] = {0};
	memcpy(strDevSN,DevSN.c_str(),DevSN.length());
	BATNetSDK_Init(0, strDevSN);

	handle_local = BATNetSDKRaw_CreateServerObj(&LocalAddr);
	BATNetSDKRaw_SetMsgCallBack(handle_local,OnRecv,NULL);


	CCMS_NETADDR ServerAddr = {{0}, 0};
	string ip_Server = "192.168.40.42";
	memcpy(ServerAddr.chIP,ip_Server.c_str(),ip_Server.length());
	ServerAddr.nPort = 10001;
	handler_Server = BATNetSDKRaw_CreateClientObj(&ServerAddr);

	muduo::Thread m_Thread(boost::bind(SendToServer));
	m_Thread.start();
	m_Thread.join();

	pause();
	BATNetSDK_Release();
	return 0;
}




