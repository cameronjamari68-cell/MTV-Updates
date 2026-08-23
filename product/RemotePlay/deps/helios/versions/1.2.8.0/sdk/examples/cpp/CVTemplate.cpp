#include <helios/HeliosCVSDK.h>

class CVWorker {
public:
    CVWorker(std::uint32_t, std::uint32_t) {}
    
    void process(Helios::Frame& frame) {
        Helios::Overlay::circle(
            cv::Point(50, 50),
            20,
            cv::Scalar(0, 255, 0),
            -1,
            Helios::Overlay::Target::Both);
        (void)frame;
    }
};

HELIOS_CV_SCRIPT(CVWorker)

