#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "stdint.h"
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "librtmp/rtmp.h"
#include "myflv.h"

#define USE_RTMPPACKET 0

//RTMP_XXX()����0��ʾʧ�ܣ�1��ʾ�ɹ�

#if 0
int main(){
	const char* rtmpurl="rtmp://192.168.1.100:1935/live/test";//���ӵ�URL
	const char* flvfilename="test.flv";//��ȡ��flv�ļ�
	RTMP_LogLevel loglevel=RTMP_LOGINFO;//����RTMP��Ϣ�ȼ�
	RTMP_LogSetLevel(loglevel);//������Ϣ�ȼ�
//	RTMP_LogSetOutput(FILE*fp);//������Ϣ����ļ�
	PublishFlv(rtmpurl,flvfilename);
	return 0;
}
#endif

void RtmpPublishStream(const char*flvfilename,const char*rtmpurl){
	RTMP*rtmp=NULL;//rtmpӦ��ָ��
#if USE_RTMPPACKET
	RTMPPacket*packet=NULL;//rtmp���ṹ
#else
    char* buffer=NULL;
#endif
	uint32_t start=0;
	uint32_t lasttime=0;
	uint32_t maxbuffsize=1024;
	char url[256]={0};
	MyFLV*myflv=MyFlvOpen(flvfilename);
	MyFrame myframe={0};

    printf("rtmpurl:%s\nflvfile:%s\nsend data ...\n",rtmpurl,flvfilename);
	if(myflv==NULL){
		printf("OpenFlvFile Err:%s\n",flvfilename);
		goto end;
	}
	printf("duration:%u\n",myflv->duration);

	rtmp=RTMP_Alloc();//����rtmp�ռ�
	RTMP_Init(rtmp);//��ʼ��rtmp����
	rtmp->Link.timeout=5;//�������ӳ�ʱ����λ�룬Ĭ��30��
////////////////////////////////����//////////////////
	strcpy(url,rtmpurl);
	RTMP_SetupURL(rtmp,url);//����url
	RTMP_EnableWrite(rtmp);//���ÿ�д״̬
	//���ӷ�����
	if (!RTMP_Connect(rtmp,NULL)){
		printf("Connect Err\n");
		goto end;
	}
	//������������(ȡ����rtmp->Link.lFlags)
	if (!RTMP_ConnectStream(rtmp,0)){
		printf("ConnectStream Err\n");
		goto end;
	}
#if USE_RTMPPACKET
	packet=(RTMPPacket*)malloc(sizeof(RTMPPacket));//������
	memset(packet,0,sizeof(RTMPPacket));	
	RTMPPacket_Alloc(packet,maxbuffsize);//��packet�������ݿռ�
	RTMPPacket_Reset(packet);//����packet״̬
	packet->m_hasAbsTimestamp = 0; //����ʱ���
	packet->m_nChannel = 0x04; //ͨ��
	packet->m_nInfoField2 = rtmp->m_stream_id;
#else
	buffer=calloc(1,maxbuffsize);
#endif
////////////////////////////////////////��������//////////////////////
	start=time(NULL)-1;
	myframe.bkeyframe=1;
	while(TRUE){
		if(myflv->beof)	break;			
		if(((time(NULL)-start)<(myflv->currenttime/1000))&&myframe.bkeyframe){	
			//����̫��͵�һ��
			if(myflv->currenttime>lasttime){
				printf("TimeStamp:%8lu ms\n",myflv->currenttime);
				lasttime=myflv->currenttime;
			}
#ifdef WIN32
			Sleep(1000);
#else			
			sleep(1);
#endif
			continue;
		}	
#if USE_RTMPPACKET
		myframe=MyFlvGetFrameInfo(myflv,packet->m_body,maxbuffsize);
		if(myframe.breadbuf==0){
			if(maxbuffsize<myframe.datalength){
			    printf("ChangeMaxBuffSize %u->%u\n",maxbuffsize,myframe.datalength);
				maxbuffsize=myframe.datalength;
				RTMPPacket_Alloc(packet,maxbuffsize);//��packet�������ݿռ�
				RTMPPacket_Reset(packet);//����packet״̬
				packet->m_hasAbsTimestamp = 0; //����ʱ���
				packet->m_nChannel = 0x04; //ͨ��
				packet->m_nInfoField2 = rtmp->m_stream_id;
				myframe=MyFlvGetFrameInfo(myflv,packet->m_body,maxbuffsize);
			}
			if(myframe.breadbuf==0){
				printf("ReadData Err Is Not Enought Buffer:%d  %u %u\n",
				maxbuffsize<myframe.datalength,	maxbuffsize,myframe.datalength);
				goto end;
			}
		}				
		if(myframe.type==0x08||myframe.type==0x09){
			packet->m_nTimeStamp = myframe.timestamp; 
			packet->m_packetType=myframe.type;
			packet->m_nBodySize=myframe.datalength;
			if (!RTMP_IsConnected(rtmp)){
				printf("rtmp is not connect\n");
				break;
			}
			if (!RTMP_SendPacket(rtmp,packet,0)){
				printf("Send Err\n");
				break;
			}
		}
#else
		myframe=MyFlvGetFrameInfo(myflv,NULL,0);
		if(maxbuffsize<myframe.alldatalength){
			printf("ChangeMaxBuffSize %u->%u\n",maxbuffsize,myframe.alldatalength);
			maxbuffsize=myframe.alldatalength;
			buffer=calloc(1,maxbuffsize);
		}
		fread(buffer,1,myframe.alldatalength,myflv->fp);
		if(myframe.type==0x08||myframe.type==0x09){
			if (!RTMP_IsConnected(rtmp)){
				printf("rtmp is not connect\n");
				break;
			}
			if (!RTMP_Write(rtmp,buffer,myframe.alldatalength)){
				printf("Send Err\n");
				break;
			}
		}
		if(ftell(myflv->fp)>=myflv->totalsize){
			break;
		}
#endif
		myframe=MyFlvGetFrameInfo(myflv,NULL,0);		
	}
	printf("\nSend Data Over\n");
end:
	if(rtmp!=NULL){
		RTMP_Close(rtmp);//�Ͽ�����
		RTMP_Free(rtmp);//�ͷ��ڴ�
		rtmp=NULL;
	}
#if USE_RTMPPACKET
	if(packet!=NULL){
		RTMPPacket_Free(packet);//�ͷ��ڴ�
		free(packet);
		packet=NULL;
	}
#else
	if(buffer){
		free(buffer);
		buffer=NULL;
	}
#endif
	myflv=MyFlvClose(myflv);
}
