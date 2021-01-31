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
    greenscreen(video_format format_ = video_format::rgb);
    ~greenscreen();

    auto get_format() const { return format; }

    bool get_frame(uint8_t*);

    auto get_width() const { return width; }
    auto get_height() const { return height; }
    auto get_bpp() const { return bpp; }
    auto get_framesize() const { return width * height * bpp; }

    uint32_t get_color() const { return (uint32_t(green_bytes[0]) << 16) | (uint32_t(green_bytes[1]) << 8) | uint32_t(green_bytes[2]);  }
    float get_max_distance() const { return depth_clipping_max_distance; }

    void set_color(uint32_t newcol);
    void set_transparency(unsigned char newa) { green_bytes[3] = newa; }
    void set_max_distance(float newmax);

  private:
    rs2::frameset wait();
    bool valid_distance(size_t pixels_distance) const;
    void remove_background(uint8_t* dest, rs2::video_frame& other_frame, const rs2::depth_frame& depth_frame);

    video_format format;

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
