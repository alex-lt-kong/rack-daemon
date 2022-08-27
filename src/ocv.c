#include <opencv2/imgcodecs/imgcodecs_c.h>
#include <opencv2/videoio/videoio_c.h>
#include <unistd.h>

int main(int argc, char** argv)
{
  CvCapture* capture = cvCreateFileCapture("/dev/video0");
  IplImage* frame;
  while(1) {
   frame = cvQueryFrame(capture);
   if(!frame) break;
   cvSaveImage("/tmp/image.jpg", frame, 0);
   sleep(10);
  }
  return 0;
}