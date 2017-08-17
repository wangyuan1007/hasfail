// findregion.cpp : 定义控制台应用程序的入口点。
//


#include "cv.h"
#include "cxcore.h"
#include "highgui.h"
#include <iostream>
using namespace std;

int main(int argc, _TCHAR* argv[])
{
	IplImage* pSrc;
	char readPath[200];
	cin>>readPath;
	pSrc =cvLoadImage(readPath, 0);
	int width = pSrc->width;
	int height = pSrc->height;
	CvPoint maxpoint = {0, 0};
	int maxcount = 0;
	for(int i = 0;i < width;i += width/10)
	{
		bool countstate = false;
		int count = 0;
		CvPoint maxpointtemp = {0 ,0};
		int j = 0;
		for(j = 0;j < height;j++)
		{
			if(unsigned char(pSrc->imageData[j * width + i]) < 125)
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
						maxpoint.y = j-1;
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
				maxpoint.y = j-1;
			}
		}
	}
	cvNamedWindow("test",1);
	cvSetImageROI(pSrc,cvRect(0,maxpoint.y - maxcount,width,maxcount));
	cvShowImage("test",pSrc);
	cvWaitKey();
	cvReleaseImage(&pSrc);
	cvDestroyWindow("test");
	return 0;
}

