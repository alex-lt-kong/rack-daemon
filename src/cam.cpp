#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <unistd.h>


#include "cam.h"

using namespace std;
using namespace cv;

void thread_live_image() {

  Mat frame;
  bool result = false;
  VideoCapture cap;
  time_t now;
  
  char dt_buf[] = "19700101-000000";
  char image_path[1024];
  
  while (true) {
    sleep(300);
    time(&now);
    strftime(dt_buf, sizeof(dt_buf), "%Y%m%d-%H%M%S", localtime(&now));
    result = cap.open("/dev/video0");
    if (!result) {
      fprintf(stderr, "cap.open() failed\n");
      continue;
    }
    cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G')); 
    //cap.set(CAP_PROP_FRAME_WIDTH, 1920);
    //cap.set(CAP_PROP_FRAME_HEIGHT, 1080);
    result = cap.read(frame);
    if (!result) {
      fprintf(stderr, "read() failed\n");
      continue;
    }
    //rotate(currFrame, currFrame, this->frameRotation);
    strcpy(image_path, "/tmp/");
    strcpy(image_path + strlen(image_path), dt_buf);
    strcpy(image_path + strlen(image_path), ".jpg");
    printf("%s\n", image_path);
    imwrite(image_path, frame); 
    cap.release();
  }
}

/*int main() {
  thread_live_image();
  return 0;
}*/