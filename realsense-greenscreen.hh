#ifndef _REALSENSE_GREENSCREEN_HH
#define _REALSENSE_GREENSCREEN_HH 1

#include <cstdint>
#include <librealsense2/rs.hpp>


namespace realsense {

  struct greenscreen
  {
    greenscreen();

    bool get_frame(uint8_t*);

  private:
    void remove_background(uint8_t* dest, rs2::video_frame& other_frame, const rs2::depth_frame& depth_frame);

    // Create a pipeline to easily configure and start the camera
    rs2::pipeline pipe;

    rs2::pipeline_profile profile;

    rs2_stream align_to;

    rs2::align align;

    float depth_scale;

    // Define a variable for controlling the distance to clip
    float depth_clipping_distance = 1.f;

    // Computed limit for foreground;
    uint16_t upper_limit;
    uint16_t lower_limit;

    // // Support for averaging the depth values.
    // average_mask<1,uint16_t,uint32_t> av;

    // Greenscreen color.
    unsigned char green_bytes[3] = { 0xdd, 0x44, 0xff };
  };

} // namespace realsense

#endif // realsense-greenscreen.hh
