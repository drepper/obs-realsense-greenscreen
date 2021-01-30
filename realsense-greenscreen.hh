#ifndef _REALSENSE_GREENSCREEN_HH
#define _REALSENSE_GREENSCREEN_HH 1

#include <cstdint>
#include <librealsense2/rs.hpp>


namespace realsense {

  enum struct video_format {
    rgb,
    rgba,
  };


  struct greenscreen
  {
    greenscreen(video_format format = video_format::rgb);
    ~greenscreen();

    bool get_frame(uint8_t*);

    auto get_width() const { return width; }
    auto get_height() const { return height; }
    auto get_bpp() const { return bpp; }
    auto get_framesize() const { return width * height * bpp; }

  private:
    rs2::frameset wait();
    bool valid_distance(size_t pixels_distance) const;
    void remove_background(uint8_t* dest, rs2::video_frame& other_frame, const rs2::depth_frame& depth_frame);

    // Create a pipeline to easily configure and start the camera
    rs2::pipeline pipe;

    rs2::pipeline_profile profile;

    rs2_stream align_to;

    rs2::align align;

    float depth_scale;

    // Define a variable for controlling the distance to clip
    float depth_clipping_min_distance = 0.10f;
    float depth_clipping_max_distance = 1.00f;

    // Computed limit for foreground;
    uint16_t upper_limit;
    uint16_t lower_limit;

    size_t width;
    size_t height;
    size_t bpp;

    // Greenscreen color.
    unsigned char green_bytes[4] = { 0xdd, 0x44, 0xff, 0xff };
  };

} // namespace realsense

#endif // realsense-greenscreen.hh
