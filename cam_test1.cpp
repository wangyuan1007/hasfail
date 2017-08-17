/*程序使用wiringi控制gpio输出引脚，程序运行时应使用root权限运行
 * 使用gpio0弹出一角，gpio1弹出五角，gpio2弹出一元
 * gpio的所有引脚示意图在终端中输入gpio readall读取
 * 程序主函数运行摄像头采集识别程序，图像采集前的摄像头初始化
 * 需要自动识别传送带区域，识别方法是将采集的图像二值化后读取十列
 * 像素，黑色区域宽度最宽的那部分是传送带，因为摄像头刚开机时读入
 * 图像不稳定，因此在识别传送带区域前有段延时。主函数采集摄像头图像
 * 并识别硬币的算法如下：摄像头桢率为30,每隔10副图对采集到的图像进行
 * 一次识别，识别时首先将图像二值化，提取区域，计算每个区域的半径，
 * 根据半径大小和颜色确定硬币类型。另开一线程运行时间检测程序，当时
 * 间到达时弹出硬币同时删除时间存储结构*/
#include <wiringPi.h>
#include <cv.h>  
#include <cxcore.h>  
#include <highgui.h>
#include <iostream>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
using namespace std;
//硬币类型
enum cointype  {YIJIAO = 0,WUJIAO = 1,YIYUAN = 2,FALSECOIN =3};
//定时弹出硬币
struct dge_time
{
	int settime;//定时时间
	enum cointype type;//硬币类型
	struct dge_time *predge;//上一个时间
	struct dge_time *nextdge;//下一个时间
};

struct countofcoin
{
	unsigned long long yijiaocount;
	unsigned long long wujiaocount;
	unsigned long long yiyuancount;
	unsigned long long falsecoincount;
};

IplImage* OriginalFrame = NULL;  //读取的原始祯
IplImage* bletimage = NULL;//截取的传送带区域图像
IplImage* binaimage = NULL;//传送带区域二值化后图像
CvCapture* pCapture = NULL;//摄像头
IplImage * templatimage = NULL;//匹配时模板
IplImage * searchimage = NULL;
IplImage* result = NULL;
CvRect rect = cvRect(0,0,0,0);//传送带区域
int outcount = 0;//图像间隔数
countofcoin coinnum = {0,0,0,0};

//线程锁
pthread_mutex_t mutex;
//弹出硬币设置时间头
dge_time *pdealtime_head = NULL;
//弹出硬币设置时间尾
dge_time *pdealtime_end = NULL;


//弹出硬币
void popcoin(dge_time popt)
{
	timeval delaytime;
	delaytime.tv_sec = 0;
	delaytime.tv_usec = 50000;//输出继电器信号时间，单位us
	digitalWrite(popt.type,HIGH);
	select(0,NULL,NULL,NULL,&delaytime);
	digitalWrite(popt.type,LOW);
	cout<<"out"<<popt.type<<endl;
	dge_time *pdealtime_tem = pdealtime_head;
	while(pdealtime_tem)
	{
		pdealtime_tem->settime -= 50;
		pdealtime_tem = pdealtime_tem->nextdge;
	}/**/
	return;
}

//线程处理弹出硬币
void* newthread_deal(void *)
{
	timeval temp;
	dge_time *pdealtime_temp = pdealtime_head;
	dge_time *pdealtime_temp2 = NULL;
	while(1)
	{
		temp.tv_sec = 0;
		temp.tv_usec = 20000;
		select(0,NULL,NULL,NULL,&temp);
		pthread_mutex_lock(&mutex);
		pdealtime_temp = pdealtime_head;
		while(pdealtime_temp)
		{
			pdealtime_temp->settime -= 20;
//			cout<<pdealtime_temp->settime<<endl;
			if(pdealtime_temp->settime <= 0)
			{
				//弹出硬币
				popcoin(*pdealtime_temp);
				//删除时间存储结构
				pdealtime_temp2 = pdealtime_temp->nextdge;
				if(pdealtime_temp->predge)
				{
					pdealtime_temp->predge->nextdge = pdealtime_temp->nextdge;
//					cout<<"predge is true"<<endl;
				}
				else
				{
					if(pdealtime_head->nextdge)
					{
//						cout<<"change head"<<endl;
						pdealtime_head = pdealtime_head->nextdge;
					}
					else
					{
//						cout<<"delete head"<<endl;
						pdealtime_head = NULL;
						pdealtime_end = NULL;
					}
				}
				if(pdealtime_temp->nextdge)
				{
					pdealtime_temp->nextdge->predge = pdealtime_temp->predge;
//					cout<<"nextdge is true"<<endl;
				}
				else
				{
					pdealtime_end = pdealtime_temp->predge;
//					cout<<"change end"<<endl;
				}
//				cout<<"delete temp"<<endl;
				delete pdealtime_temp;
				pdealtime_temp = pdealtime_temp2;
//				cout<<"new temp0"<<endl;
			}
			else
			{
//				cout<<"new temp1"<<endl;
				pdealtime_temp = pdealtime_temp->nextdge;
			}
		}
		pthread_mutex_unlock(&mutex);
	}
}

//计算时间
int time_substract(struct timeval *result, struct timeval *begin,struct timeval *end)
{

    if(begin->tv_sec > end->tv_sec)    return -1;
    if((begin->tv_sec == end->tv_sec) && (begin->tv_usec > end->tv_usec))    return -2;
    result->tv_sec    = (end->tv_sec - begin->tv_sec);
    result->tv_usec    = (end->tv_usec - begin->tv_usec);
    if(result->tv_usec < 0)
    {
        result->tv_sec--;
        result->tv_usec += 1000000;
    }
    return 0;

}

//摄像头初始化
bool initcam()
{
	pCapture = cvCreateCameraCapture(0);
    cvNamedWindow("video", 1); 
//    cvNamedWindow("000", 1); 
    for(int i = 0;i<30;i++)
    { 
		OriginalFrame = cvQueryFrame( pCapture );
		if(OriginalFrame)
			cvShowImage("video",OriginalFrame); 
		cvWaitKey(33);
	}

	OriginalFrame = cvQueryFrame( pCapture ); 
	if(!OriginalFrame)
	{
		cout<<"error"<<endl;
		return false;
	}

	binaimage = cvCreateImage(cvGetSize(OriginalFrame),8,1);
	cvCvtColor(OriginalFrame,binaimage,CV_RGB2GRAY);
	cvThreshold(binaimage,binaimage,125,255,CV_THRESH_BINARY);
	int width = binaimage->width;
	int height = binaimage->height;
	CvPoint maxpoint = cvPoint(0,0);
	int maxcount = 0;
	for(int i = 0;i < width;i += width/10)
	{
		bool countstate = false;
		int count = 0;
		int j = 0;
		for(j = 0;j < height;j++)
		{
			if((unsigned char)(binaimage->imageData[j * width + i]) < 125)
			{
				count++;
				countstate = true;
			}
			else
			{
				if(countstate)
				{
					countstate = false;
					if(count > maxcount)
					{
						maxcount = count;
						maxpoint.y = j;
					}
					count = 0;
				}
			}
		}
		if(countstate)
		{
			if(count > maxcount)
			{
				maxcount = count;
				maxpoint.y = j;
			}
		}
	}
	rect = cvRect(100,maxpoint.y - maxcount,width-380,maxcount);
	cout<<rect.width<<endl<<rect.height<<endl;
//	cvSaveImage("1.bmp",binaimage);
	cvReleaseImage(&binaimage);
	bletimage = cvCreateImage(cvSize(rect.width,rect.height),OriginalFrame->depth,OriginalFrame->nChannels);
	binaimage = cvCreateImage(cvSize(rect.width,rect.height),8,1);
//	binaimage2 = cvCreateImage(cvSize(rect.width,rect.height),8,1);
	return true;
}

//显示处理视频
void showanddealcam()
{
	//计算运算时间确定opencv延时时间
	struct timeval start,stop,diff;
	memset(&start,0,sizeof(struct timeval));
    memset(&stop,0,sizeof(struct timeval));
    memset(&diff,0,sizeof(struct timeval));
	int image_delay=0;//opencv延时时间
	CvMemStorage* stor=cvCreateMemStorage(0);//轮廓
	CvSeq * cont=cvCreateSeq(CV_SEQ_ELTYPE_POINT,sizeof(CvSeq),sizeof(CvPoint),stor);
	dge_time *pdealtime_temp = NULL;//临时指针
	double scale;//纵横轴比例
	bool ifdrawcon;//是否画轮廓
	double contourradius;//opencv计算的直径
	double xcontourradius;//横轴直径
	double ycontourradius;//纵轴直径
	CvPoint topleft,bottomright;//手动计算的轮廓区域
	CvPoint *temppoint;//临时指针
	CvPoint centerpoint = cvPoint(0,0);//质心
	long long int centerx,centery;
	CvRect searchrect;
	CvRect temprect;
	for(;;)
    {
		//计时开始
		gettimeofday(&start,0);
		//获取图像
		OriginalFrame=cvQueryFrame( pCapture ); 
		if(!OriginalFrame)
		{
			cout<<"error"<<endl;
			break;
		}
		
		//将传送带区域图像复制到新的变量中
		for(int i = rect.y,k = 0;i < rect.height + rect.y;i++)
		{
			for(int j = 100;j < rect.width+100;j++)
			{
				bletimage->imageData[k++] = OriginalFrame->imageData[3*(i * OriginalFrame->width + j)];
				bletimage->imageData[k++] = OriginalFrame->imageData[3*(i * OriginalFrame->width + j) + 1];
				bletimage->imageData[k++] = OriginalFrame->imageData[3*(i * OriginalFrame->width + j) + 2];
			}
		}
		if(outcount++/12 == 1)//每隔15副图处理一次
		{
//			cout<<outcount<<endl;
			outcount = 0;
			//转换成二值图像
			cvCvtColor(bletimage,binaimage,CV_RGB2GRAY);
//			cvShowImage("000",binaimage);
			cvThreshold(binaimage,binaimage,150,255,CV_THRESH_BINARY);
//			cvCopy(binaimage,binaimage2);
		//寻找轮廓
			cvFindContours(binaimage,stor,&cont,sizeof(CvContour),CV_RETR_EXTERNAL,CV_CHAIN_APPROX_NONE,cvPoint(0,0));
			for (;cont;cont=cont->h_next)
			{
				//计算直径（函数计算）
				contourradius = fabs(cvContourArea(cont,CV_WHOLE_SEQ)/cvArcLength(cont))*4;
				if(contourradius > 50)
				{
					//计算横轴和纵轴直径
					topleft = cvPoint(bletimage->width,bletimage->height);
					bottomright = cvPoint(0,0);
					temppoint = NULL;
					centerx = 0;
					centery = 0;
					for(int i =0;i < cont->total;i++)
					{
						temppoint = (CvPoint *)cvGetSeqElem(cont,i);
						centerx += temppoint->x;
						centery += temppoint->y;
						topleft.x = topleft.x < temppoint->x ? topleft.x : temppoint->x;
						topleft.y = topleft.y < temppoint->y ? topleft.y : temppoint->y;
						bottomright.x = bottomright.x > temppoint->x ? bottomright.x : temppoint->x;
						bottomright.y = bottomright.y > temppoint->y ? bottomright.y : temppoint->y;
					}
					centerpoint = cvPoint(centerx/cont->total,centery/cont->total);//质心
					
					xcontourradius = bottomright.x - topleft.x;//横轴
					ycontourradius = bottomright.y - topleft.y;//纵轴
					scale = ycontourradius/xcontourradius;//横轴和纵轴比例
					
					if(templatimage&&xcontourradius + 8 > templatimage->width&&ycontourradius + 8 > templatimage->height)
					{
						searchrect = cvRect(topleft.x - 4,topleft.y - 4,xcontourradius+4-(int)xcontourradius%4 + 8,ycontourradius + 10); 
						searchimage = cvCreateImage(cvSize(searchrect.width,ycontourradius + 8),OriginalFrame->depth,OriginalFrame->nChannels);
						result = cvCreateImage(cvSize(searchimage->width - templatimage->width + 1,
							searchimage->height - templatimage->height + 1),32,1);
						for(int i = searchrect.y,k = 0;i < searchrect.height + searchrect.y;i++)
						{
							for(int j = searchrect.x;j < searchrect.x+searchrect.width;j++)
							{
								searchimage->imageData[k++] = bletimage->imageData[3*(i * bletimage->width + j)];
								searchimage->imageData[k++] = bletimage->imageData[3*(i * bletimage->width + j) + 1];
								searchimage->imageData[k++] = bletimage->imageData[3*(i * bletimage->width + j) + 2];
							}
						}
//						cvSaveImage("searchimage.bmp",searchimage);
						cvMatchTemplate(searchimage,templatimage,result,CV_TM_SQDIFF);
						double minvalue,maxvalue;
						CvPoint minloc,maxloc;
						cvMinMaxLoc(result,&minvalue,&maxvalue,&minloc,&maxloc);
						cout<<minvalue<<endl<<maxvalue<<endl;
						cvReleaseImage(&searchimage);
						cvReleaseImage(&result);
						if(minvalue< 1)
						{
							cvReleaseImage(&templatimage);
							templatimage = NULL;
							ifdrawcon = false;
						}
						else
							ifdrawcon = (scale > 0.91)&&(scale < 1.1)&&
								((bottomright.x - topleft.x + bottomright.y - topleft.y)/contourradius/2 > 0.9)&&
								((bottomright.x - topleft.x + bottomright.y - topleft.y)/contourradius/2 < 1.11);
					}
					else
					{
						ifdrawcon = (scale > 0.91)&&(scale < 1.1)&&
								((bottomright.x - topleft.x + bottomright.y - topleft.y)/contourradius/2 > 0.9)&&
								((bottomright.x - topleft.x + bottomright.y - topleft.y)/contourradius/2 < 1.11);
					}
					
					if(ifdrawcon)
					{
					/*	temprect = cvRect(topleft.x ,topleft.y  ,xcontourradius+4-(int)xcontourradius%4,ycontourradius);
						templatimage = cvCreateImage(cvSize(temprect.width,ycontourradius),
							OriginalFrame->depth,OriginalFrame->nChannels);
						for(int i = temprect.y,k = 0;i < temprect.height + temprect.y;i++)
						{
							for(int j = temprect.x;j < temprect.x+temprect.width;j++)
							{
								templatimage->imageData[k++] = bletimage->imageData[3*(i * bletimage->width + j)];
								templatimage->imageData[k++] = bletimage->imageData[3*(i * bletimage->width + j) + 1];
								templatimage->imageData[k++] = bletimage->imageData[3*(i * bletimage->width + j) + 2];
							}
						}
					//	cvShowImage("000",templatimage); 
						cout<<topleft.x - 4<<endl<<topleft.y -4<<endl;
						cout<<xcontourradius+8<<endl<<ycontourradius+8<<endl;
						cvSaveImage("templatimage.bmp",templatimage);
						cout<<"scale:"<<scale<<endl<<"y:"<<ycontourradius<<endl<<"x:"<<xcontourradius;
						//画轮廓
						cout<<"topleft.x:"<<topleft.x<<endl;
						cout<<"topleft.y:"<<topleft.y<<endl;
						cout<<"bottomright.x:"<<bottomright.x<<endl;
						cout<<"bottomright.y:"<<bottomright.y<<endl;
						cout<<"centerpoint.x:"<<centerpoint.x<<endl;
						cout<<"centerpoint.y:"<<centerpoint.y<<endl;*/
						cout<<"contourradius:"<<contourradius<<endl;
						cout<<"xcontourradius:"<<xcontourradius<<endl;
						cout<<"ycontourradius:"<<ycontourradius<<endl;
						cvDrawContours(bletimage,cont,CV_RGB(255,0,0),CV_RGB(255,0,0),0,1,8,cvPoint(0,0));
						pthread_mutex_lock(&mutex);
						//创建弹出硬币的变量
						if(!pdealtime_head)
						{
//							cout<<"new head"<<endl;
							pdealtime_head = new dge_time();			
							pdealtime_temp = pdealtime_head;
						}
						else
						{
//							cout<<"new time"<<endl;
							pdealtime_temp = new dge_time();
							pdealtime_end->nextdge = pdealtime_temp;
						}
						pdealtime_temp->predge = pdealtime_end;
						pdealtime_temp->nextdge = NULL;
						pdealtime_end = pdealtime_temp;
						
						if(contourradius > 180&&contourradius<200/**/)
						{
							pdealtime_temp->type = YIYUAN;//硬币类型
							pdealtime_temp->settime = 3000;//设置硬币弹出时间
							coinnum.yiyuancount++;
						}
						else if(contourradius > 145&&contourradius<165/**/)
						{
							if(abs((unsigned int)bletimage->imageData[(centerpoint.y*bletimage->width+centerpoint.x)*3]-
							(unsigned int)bletimage->imageData[(centerpoint.y*bletimage->width+centerpoint.x)*3+1])+
							abs((unsigned int)bletimage->imageData[(centerpoint.y*bletimage->width+centerpoint.x)*3+1]-
							(unsigned int)bletimage->imageData[(centerpoint.y*bletimage->width+centerpoint.x)*3+2])<80)
							{
								pdealtime_temp->type = YIJIAO;
								pdealtime_temp->settime = 4000;//设置硬币弹出时间
								coinnum.yijiaocount++;
							}
							else
							{
								pdealtime_temp->type = WUJIAO;
								pdealtime_temp->settime = 5000;//设置硬币弹出时间
								coinnum.wujiaocount++;
							}
						}
						else
						{
							pdealtime_temp->type = FALSECOIN;//硬币类型
							pdealtime_temp->settime = 6000;//设置硬币弹出时间
							coinnum.falsecoincount++;
						}
						
						pthread_mutex_unlock(&mutex);
				}
				
				}
			}
			cvClearMemStorage(stor);
		}
		cvShowImage("video",bletimage);
		//计时结束
		gettimeofday(&stop,0);
		time_substract(&diff,&start,&stop);
		if(diff.tv_usec/1000 >= 33)
			image_delay=1;
		else
			image_delay = 33 - diff.tv_usec/1000;
        if(cvWaitKey(image_delay) == 27)
        {
			break;
		}
    }
}

int main()
{
	wiringPiSetup();
	pinMode(0,OUTPUT);
	pinMode(1,OUTPUT);
	pinMode(2,OUTPUT);
	pinMode(3,OUTPUT);
	pthread_t coindeal;
	pthread_create(&coindeal,NULL,&newthread_deal,NULL);
	pthread_mutex_init(&mutex,NULL);
	//摄像头初始化
    if(!initcam())
    {
		return 0;
	}
	//显示摄像头
	showanddealcam();
    cvWaitKey(100);
	cvReleaseImage(&bletimage);
	cvReleaseImage(&binaimage);
	cvReleaseCapture(&pCapture); 
	cvDestroyWindow("video");  
	return 0;
}
