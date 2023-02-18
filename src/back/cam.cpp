#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <syslog.h>
#include <unistd.h>
#include <libgen.h>

#include "cam.h"

using namespace std;
using namespace cv;

volatile int cam_th_done = 0;

struct Payload {
    int32_t temps[4];
    float fans_load;
};

void stop_capture_live_image() {
    cam_th_done = 1;
}

void* thread_capture_live_image(void*) {
    syslog(LOG_INFO, "thread_capture_live_image() started.");
    Mat frame;
    bool result = false;
    VideoCapture cap;
    time_t now;


    char dt_buf[] = "19700101-000000";
    char image_path[PATH_MAX];
    const uint16_t interval = 3600;
    uint32_t iter = interval;
    while (!cam_th_done) {
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
        Size textSize = getTextSize(dt_buf, FONT_HERSHEY_DUPLEX, font_scale,
            8 * font_scale, nullptr);
        putText(frame, dt_buf, Point(5, textSize.height * 1.05),
            FONT_HERSHEY_DUPLEX, font_scale, Scalar(0,  0,  0  ),
            8 * font_scale, LINE_AA, false);
        putText(frame, dt_buf, Point(5, textSize.height * 1.05),
            FONT_HERSHEY_DUPLEX, font_scale, Scalar(255,255,255),
            2 * font_scale, LINE_AA, false);
        strcpy(image_path, getenv("RD_IMAGE_DIR"));
        strcpy(image_path + strlen(image_path), dt_buf);
        strcpy(image_path + strlen(image_path), ".jpg");
        if (!imwrite(image_path, frame)) {
        syslog(LOG_ERR, "imwrite() failed");
        }
        cap.release();
    }
    char exit_msg[] = "thread_capture_live_image() quits gracefully\n";
    syslog(LOG_INFO, exit_msg);
    printf(exit_msg);
    return NULL;
}

/*int main() {
  thread_capture_live_image();
  return 0;
}*/
