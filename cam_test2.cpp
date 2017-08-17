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
	delaytime.tv_usec = 500000;//输出继电器信号时间，单位us
	digitalWrite(popt.type,HIGH);
	select(0,NULL,NULL,NULL,&delaytime);
	digitalWrite(popt.type,LOW);
	cout<<"out"<<endl<<popt.type<<endl;
	dge_time *pdealtime_temp = pdealtime_head;
	while(pdealtime_temp)
	{
		pdealtime_temp->settime -= 500;
		pdealtime_temp = pdealtime_temp->nextdge;
		}
	return;
}

//线程处理弹出硬币
void* newthread_deal(void *)
{
	timeval temp;
	//100ms检测一次
	temp.tv_sec = 0;
	temp.tv_usec = 20000;
	dge_time *pdealtime_temp = pdealtime_head;
	dge_time *pdealtime_temp2 = NULL;
	while(1)
	{
		select(0,NULL,NULL,NULL,&temp);
		temp.tv_sec = 0;
		temp.tv_usec = 20000;
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
					pdealtime_temp->predge->nextdge = pdealtime_temp->nextdge;
				if(pdealtime_temp->nextdge)
					pdealtime_temp->nextdge->predge = pdealtime_temp->predge;
				if(pdealtime_temp == pdealtime_head)
				{
					pdealtime_head = NULL;
					pdealtime_end = NULL;
				}
				if(pdealtime_temp == pdealtime_end)
					pdealtime_end = pdealtime_temp->predge;
				delete pdealtime_temp;
				pdealtime_temp = pdealtime_temp2;
			}
			else
			{
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

//定时器处理函数
void timedeal(int signo)
{
	dge_time *pdealtime_temp = pdealtime_head;
	dge_time *pdealtime_temp2 = NULL;
	pthread_mutex_lock(&mutex);
	while(pdealtime_temp)
	{
		pdealtime_temp->settime -= 100;
		if(pdealtime_temp->settime <= 0)
		{
			
			popcoin(*pdealtime_temp);
			
			pdealtime_temp2 = pdealtime_temp->nextdge;
			if(pdealtime_temp->predge)
				pdealtime_temp->predge->nextdge = pdealtime_temp->nextdge;
			if(pdealtime_temp->nextdge)
				pdealtime_temp->nextdge->predge = pdealtime_temp->predge;
			if(pdealtime_temp == pdealtime_head)
			{
				pdealtime_head = NULL;
				pdealtime_end = NULL;
			}
			if(pdealtime_temp == pdealtime_end)
				pdealtime_end = pdealtime_temp->predge;
			delete pdealtime_temp;
			pdealtime_temp = pdealtime_temp2;
		}
		else
		{
			pdealtime_temp = pdealtime_temp->nextdge;
		}
	}
	pthread_mutex_unlock(&mutex);
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
//	cout<<"create binaimage init()"<<endl;
	cvCvtColor(OriginalFrame,binaimage,CV_RGB2GRAY);
	cvThreshold(binaimage,binaimage,125,255,CV_THRESH_BINARY);
//	cout<<"init binaimage has bina"<<endl;
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
	rect = cvRect(100,maxpoint.y - maxcount,width-340,maxcount);
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
	/*dge_time *pdealtime_temp = NULL;//临时指针
	double scale;//纵横轴比例
	bool ifdrawcon;//是否画轮廓
	double contourradius;//opencv计算的直径
	double xcontourradius;//横轴直径
	double ycontourradius;//纵轴直径
	CvPoint *temppoint;//临时指针
	CvPoint centerpoint = cvPoint(0,0);//质心
	long long int centerx,centery;*/
	CvPoint topleft,bottomright;//手动计算的轮廓区域
	float * poi = NULL;
	CvPoint pt;
	for(;;)
    {
		//获取图像
		OriginalFrame=cvQueryFrame( pCapture ); 
		
		//计时开始
		gettimeofday(&start,0);
//		cout<<"getimage"<<endl;
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
			outcount = 0;
			//转换成二值图像
			cvCvtColor(bletimage,binaimage,CV_RGB2GRAY);
			cvThreshold(binaimage,binaimage,150,255,CV_THRESH_BINARY);
		//寻找轮廓
			cont = cvHoughCircles(binaimage,stor,CV_HOUGH_GRADIENT,20,300);
			for (;cont;cont=cont->h_next)
			{
				poi = (float*) cvGetSeqElem()
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
