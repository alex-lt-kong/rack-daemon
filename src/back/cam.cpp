#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <syslog.h>
#include <unistd.h>
#include <libgen.h>

#include "cam.h"

#define IMAGES_PATH_SIZE 5432

using namespace std;
using namespace cv;

volatile int done = 0;

struct Payload {
  int32_t temps[4];
  float fans_load;
};

void stop_capture_live_image() {
  done = 1;
}

void* thread_capture_live_image(void*) {
  syslog(LOG_INFO, "thread_capture_live_image() started.");
  Mat frame;
  bool result = false;
  VideoCapture cap;
  time_t now;
  

  char images_dir[IMAGES_PATH_SIZE];
  readlink("/proc/self/exe", images_dir, IMAGES_PATH_SIZE);
  char* db_dir = dirname(images_dir); // doc exlicitly says we shouldnt free() it.
  strncpy(images_dir, db_dir, IMAGES_PATH_SIZE - 1);
  // If the length of src is less than n, strncpy() writes an additional NULL characters to dest to ensure that
  // a total of n characters are written.
  // HOWEVER, if there is no null character among the first n character of src, the string placed in dest will
  // not be null-terminated. So strncpy() does not guarantee that the destination string will be NULL terminated.
  // Ref: https://www.geeksforgeeks.org/why-strcpy-and-strncpy-are-not-safe-to-use/
  strncpy(images_dir + strnlen(images_dir, IMAGES_PATH_SIZE - 30), "/images/", IMAGES_PATH_SIZE - 1);

  char dt_buf[] = "19700101-000000";
  char image_path[IMAGES_PATH_SIZE];
  const uint16_t interval = 3600;
  uint32_t iter = interval;
  while (!done) {
    ++iter;
    sleep(1);
    if (iter < interval) { continue; }
    iter = 0;
    time(&now);
    strftime(dt_buf, sizeof(dt_buf), "%Y%m%d-%H%M%S", localtime(&now));
    result = cap.open("/dev/video0");
    if (!result) {
      syslog(LOG_ERR,"cap.open() failed");
      continue;
    }
    cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G')); 
    cap.set(CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(CAP_PROP_FRAME_HEIGHT, 720);
    result = cap.read(frame);
    if (!result) {
      syslog(LOG_ERR, "cap.read() failed");
      continue;
    }
    rotate(frame, frame, ROTATE_90_COUNTERCLOCKWISE);
    uint16_t font_scale = 1;
    Size textSize = getTextSize(dt_buf, FONT_HERSHEY_DUPLEX, font_scale, 8 * font_scale, nullptr);
    putText(
      frame, dt_buf, Point(5, textSize.height * 1.05), FONT_HERSHEY_DUPLEX, font_scale, Scalar(0,  0,  0  ), 8 * font_scale, LINE_AA, false
    );
    putText(
      frame, dt_buf, Point(5, textSize.height * 1.05), FONT_HERSHEY_DUPLEX, font_scale, Scalar(255,255,255), 2 * font_scale, LINE_AA, false
    );
    strcpy(image_path, images_dir);
    strcpy(image_path + strlen(image_path), dt_buf);
    strcpy(image_path + strlen(image_path), ".jpg");
    if (!imwrite(image_path, frame)) {
      syslog(LOG_ERR, "imwrite() failed");
    }
    cap.release();
  }
  syslog(LOG_INFO, "thread_capture_live_image() quits gracefully\n");
  return NULL;
}

/*int main() {
  thread_capture_live_image();
  return 0;
}*/
