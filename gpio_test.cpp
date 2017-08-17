/*
#include <stdio.h>
#include <wiringPi.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <cv.h>  
#include <cxcore.h>  
#include <highgui.h>
#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <string.h> 
#include <signal.h>
#include <unistd.h>

using namespace std;



void printMes(int signo)
{
//	cout<<"printMesbegin"<<endl;
    
//	cvSaveImage("1.bmp",pFrame);
//	cout<<"printfend"<<endl;
}

int main(int argc, char **argv)
{
	IplImage* pFrame = NULL;  
	CvCapture* pCapture = NULL;
    struct itimerval tick;
	
	memset(&tick, 0, sizeof(tick));
	tick.it_value.tv_sec = 1;
	tick.it_value.tv_usec = 0;
	tick.it_interval.tv_sec = 1;
	tick.it_interval.tv_usec = 0;
	
	wiringPiSetup();
	pinMode(0,OUTPUT);
	digitalWrite(0,HIGH);
	pCapture = cvCreateCameraCapture(0);  
	cvNamedWindow("video", 1);  
	pFrame=cvQueryFrame( pCapture ); 
	cvShowImage("video",pFrame);*/
/*	signal(SIGALRM, printMes);
	if(setitimer(ITIMER_REAL, &tick, NULL) < 0)
		cout<<"Set timer failed!"<<endl;*/
/*	while(1)
   	 {
		 pFrame=cvQueryFrame( pCapture );  
		if(!pFrame)
		{
			cout<<"error"<<endl;
			return 0;
		}
        cvShowImage("video",pFrame); 
   	 }
	cvReleaseCapture(&pCapture); 
	cvDestroyWindow("video");  
	return 0;
}*/

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/videoio.hpp"
#include <iostream>
#include <signal.h>
#include <wiringPi.h>
#include <sys/time.h>


using namespace cv;
using namespace std;

Mat image;
VideoCapture capture;
int num = 0;
void drawText(Mat & image);
void printMes(int signo)
{
	capture >> image;
    if(image.empty())
        return;
    imshow("Sample", image);
//    cout<<num++<<endl;
}
int main()
{
	struct itimerval tick;
	memset(&tick, 0, sizeof(tick));
	tick.it_value.tv_sec = 1;
	tick.it_value.tv_usec = 0;
	tick.it_interval.tv_sec = 0;
	tick.it_interval.tv_usec = 53334;
	
	wiringPiSetup();
	pinMode(0,OUTPUT);
	digitalWrite(0,HIGH);
	signal(SIGALRM, printMes);
	if(setitimer(ITIMER_REAL, &tick, NULL) < 0)
		cout<<"Set timer failed!"<<endl;
    cout << "Built with OpenCV " << CV_VERSION << endl;
    
    capture.open(0);
    if(capture.isOpened())
    {
        cout << "Capture is opened" << endl;
        for(;;)
        {
            if(waitKey(1000) >= 0)
                break;
        }
    }
    else
    {
        cout << "No capture" << endl;
        image = Mat::zeros(480, 640, CV_8UC1);
//        drawText(image);
        imshow("Sample", image);
        waitKey(0);
    }
    return 0;
}

/*void drawText(Mat & image)
{
    putText(image, "Hello OpenCV",
            Point(20, 50),
            FONT_HERSHEY_COMPLEX, 1, // font face and scale
            Scalar(255, 255, 255), // white
            1, LINE_AA); // line thickness and type
}
*/
