#ifndef _REALSENSE_GREENSCREEN_HH
#define _REALSENSE_GREENSCREEN_HH 1

#include <cstdint>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>
#include <librealsense2/rs.hpp>


namespace realsense {

  enum struct video_format {
    rgb,
    rgba,
  };


  struct device
  {
    device(video_format format_, float min_distance, float max_distance, size_t ndepth_history, unsigned char* color, rs2::config& config);
    ~device();

    bool get_frame(uint8_t*, size_t framesize);

    auto get_width() const { return width; }
    auto get_height() const { return height; }
    auto get_bpp() const { return bpp; }

    void set_color(uint32_t newcol);
    void set_transparency(unsigned char newa) { green_bytes[3] = newa; }
    void set_max_distance(float newmax);
    void set_ndepth_history(size_t newsize);

    rs2::frameset wait();
    bool valid_distance(size_t pixels_distance) const;
    void remove_background(uint8_t* dest, size_t framesize, rs2::video_frame& other_frame, const rs2::depth_frame& depth_frame);

    const video_format format;

    // Create a pipeline to easily configure and start the camera
    std::unique_ptr<rs2::pipeline> pipe;

    rs2::pipeline_profile profile;

    rs2_stream align_to;

    rs2::align align;

    float depth_scale;

    // Serial number of currently used device.
    std::string name;
    std::string serial;

    // Define a variable for controlling the distance to clip
    float depth_clipping_min_distance;
    float depth_clipping_max_distance;

    // Computed limit for foreground;
    size_t upper_limit;
    size_t lower_limit;

    size_t width;
    size_t height;
    size_t bpp;

    std::vector<std::vector<uint16_t>> depth_history;
    size_t last_depth_frame = 0;

    // device color.
    unsigned char green_bytes[4];
  };


  struct greenscreen {
    greenscreen(video_format format_ = video_format::rgb);

    bool new_config(const std::string& serial, const std::string& resolution);

    video_format get_format() const { return format; }

    bool get_frame(uint8_t* dest, size_t framesize);

    size_t get_width() const;
    size_t get_height() const;
    size_t get_bpp() const;
    size_t get_framesize() const;

    uint32_t get_color() const { return (uint32_t(green_bytes[0]) << 16) | (uint32_t(green_bytes[1]) << 8) | uint32_t(green_bytes[2]);  }
    float get_max_distance() const { return depth_clipping_max_distance; }
    size_t get_ndepth_history() const { return ndepth_history; }

    void set_color(uint32_t newcol);
    void set_transparency(unsigned char newa);
    void set_max_distance(float newmax);
    void set_ndepth_history(size_t newsize);

    const video_format format;

    // Define a variable for controlling the distance to clip
    float depth_clipping_min_distance = 0.10f;
    float depth_clipping_max_distance = 1.00f;

    size_t ndepth_history = 4;

    unsigned char green_bytes[4] = { 0xdd, 0x44, 0xff, 0x00 };

    size_t max_width;
    size_t max_height;

    using available_type = std::tuple<std::string,size_t,size_t,std::string,std::string>;
    std::vector<available_type> available;

    std::mutex devlock;
    std::unique_ptr<device> dev;
  };

} // namespace realsense

#endif // realsense-greenscreen.hh
