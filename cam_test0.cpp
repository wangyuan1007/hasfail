#include <wiringPi.h>
#include <cv.h>  
#include <cxcore.h>  
#include <highgui.h>
#include <iostream>
#include <signal.h>
#include <sys/time.h>
using namespace std;

IplImage* OriginalFrame = NULL;  //读取的原始祯
IplImage* bletimage= NULL;//截取的传送带区域图像
IplImage* binaimage= NULL;//传送带区域二值化后图像
CvCapture* pCapture = NULL;//摄像头
bool startshowcam = false;//显示图像标志位
CvRect rect = cvRect(0,0,0,0);//传送带区域
int outcount = 0;

//处理显示祯
void printMes(int signo)
{
	//初始化完成之前不显示
	if(startshowcam)
	{
		//获取图像
		OriginalFrame=cvQueryFrame( pCapture );  
//		cout<<"getimage"<<endl;
		if(!OriginalFrame)
		{
			cout<<"error"<<endl;
			return;
		}
		
		
		for(int i = rect.y,k = 0;i < rect.height + rect.y;i++)
		{
			for(int j = 0;j < rect.width;j++)
			{
				bletimage->imageData[k++] = OriginalFrame->imageData[3*(i * rect.width + j)];
				bletimage->imageData[k++] = OriginalFrame->imageData[3*(i * rect.width + j) + 1];
				bletimage->imageData[k++] = OriginalFrame->imageData[3*(i * rect.width + j) + 2];
			}
		}
		cvSetImageROI(OriginalFrame,rect);
//		cout<<"creat bletimage"<<endl;
//		cvCopy(OriginalFrame,bletimage);
//		cout<<"copy image"<<endl;
//		cvShowImage("video",bletimage);
		//转换成二值图像
//		cvCvtColor(bletimage,binaimage,CV_RGB2GRAY);
	//	cvThreshold(binaimage,binaimage,120,255,CV_THRESH_BINARY);
		cvShowImage("video",OriginalFrame);
		cvResetImageROI(OriginalFrame);
//		cvReleaseImage(&OriginalFrame);
		
//		

		if(outcount++ %20 == 0)
			cout<<"show image"<<outcount<<endl;
	}
}
bool initcam()
{
	pCapture = cvCreateCameraCapture(0);
	cout<<"set cam"<<endl;
    cvNamedWindow("video", 1); 
    cout<<"set windows"<<endl;
    for(int i = 0;i<10;i++)
    { 
		OriginalFrame = cvQueryFrame( pCapture ); 
		cvWaitKey(33);
	}

	OriginalFrame = cvQueryFrame( pCapture ); 
	if(!OriginalFrame)
	{
		cout<<"error"<<endl;
		return false;
	}

	binaimage = cvCreateImage(cvGetSize(OriginalFrame),8,1);
	cout<<"create binaimage init()"<<endl;
	cvCvtColor(OriginalFrame,binaimage,CV_RGB2GRAY);
	cvThreshold(binaimage,binaimage,120,255,CV_THRESH_BINARY);
	cout<<"init binaimage has bina"<<endl;
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
	rect = cvRect(0,maxpoint.y - maxcount,width,maxcount);
	cout<<rect.width<<endl<<rect.height<<endl;
//	cvSaveImage("1.bmp",binaimage);
	cvReleaseImage(&binaimage);
	bletimage = cvCreateImage(cvSize(rect.width,rect.height),OriginalFrame->depth,OriginalFrame->nChannels);
	binaimage = cvCreateImage(cvSize(rect.width,rect.height),8,1);
	startshowcam = true;
	return true;
}
int main()
{
	struct itimerval tick;
	memset(&tick, 0, sizeof(tick));
	tick.it_value.tv_sec = 1;
	tick.it_value.tv_usec = 0;
	tick.it_interval.tv_sec = 0;
	tick.it_interval.tv_usec = 53334;
	
/*	wiringPiSetup();
	pinMode(0,OUTPUT);
	digitalWrite(0,HIGH);*/
	signal(SIGALRM, printMes);
	if(setitimer(ITIMER_REAL, &tick, NULL) < 0)
		cout<<"Set timer failed!"<<endl;

    if(!initcam())
    {
		return 0;
	}
	
	for(;;)
    {
        if(cvWaitKey(33) >= 0)
        {
			startshowcam = false;
			break;
			
		}
    }
    cvWaitKey(100);
	cvReleaseCapture(&pCapture); 
	cvDestroyWindow("video");  
	return 0;
}
