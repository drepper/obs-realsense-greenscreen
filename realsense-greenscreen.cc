#include <cassert>
#include <cstring>
#include <set>
#include <stdexcept>

#include "realsense-greenscreen.hh"

// XYZ Debug
// #include <iostream>

// Parts of this file are derived from Intel's rs-align-advanced example
// which carries this copyright nore:
//
//     License: Apache 2.0. See LICENSE file in root directory.
//     Copyright(c) 2017 Intel Corporation. All Rights Reserved.


namespace realsense {

  namespace {

    float get_depth_scale(const rs2::device& dev)
    {
      // Go over the device's sensors
      for (rs2::sensor& sensor : dev.query_sensors())
        // Check if the sensor if a depth sensor
        if (rs2::depth_sensor dpt = sensor.as<rs2::depth_sensor>())
          return dpt.get_depth_scale();

      throw std::runtime_error("Device does not have a depth sensor");
    }


    rs2_stream find_stream_to_align(const std::vector<rs2::stream_profile>& streams)
    {
      //Given a vector of streams, we try to find a depth stream and another stream to align depth with.
      //We prioritize color streams to make the view look better.
      //If color is not available, we take another stream that (other than depth)
      rs2_stream align_to = RS2_STREAM_ANY;
      bool depth_stream_found = false;
      bool color_stream_found = false;
      for (rs2::stream_profile sp : streams) {
        rs2_stream profile_stream = sp.stream_type();
        if (profile_stream != RS2_STREAM_DEPTH) {
          if (!color_stream_found)         //Prefer color
            align_to = profile_stream;

          if (profile_stream == RS2_STREAM_COLOR)
            color_stream_found = true;
        } else
          depth_stream_found = true;
      }

      if(!depth_stream_found)
        throw std::runtime_error("No Depth stream available");

      if (align_to == RS2_STREAM_ANY)
        throw std::runtime_error("No stream found to align with Depth");

      return align_to;
    }


    bool profile_changed(const std::vector<rs2::stream_profile>& current, const std::vector<rs2::stream_profile>& prev)
    {
      for (auto&& sp : prev) {
        auto itr = std::find_if(std::begin(current), std::end(current), [&sp](const rs2::stream_profile& current_sp) { return sp.unique_id() == current_sp.unique_id(); });
        if (itr == std::end(current))
          return true;
      }
      return false;
    }

  }// anonymous namespace


  device::device(video_format format_, float min_distance, float max_distance, unsigned char* color, rs2::config& config)
  : format(format_),
    // Create the pipeline object.
    pipe(std::make_unique<rs2::pipeline>()),
    // Calling pipeline's start() without any additional parameters will start the first device
    // with its default streams.
    // The start function returns the pipeline profile which the pipeline used to start the device
    profile(pipe->start(config)),
    // Pipeline could choose a device that does not have a color stream
    // If there is no color stream, choose to align depth to another stream
    align_to(find_stream_to_align(profile.get_streams())),
    // Create a rs2::align object.
    // rs2::align allows us to perform alignment of depth frames to others frames
    // The "align_to" is the stream type to which we plan to align depth frames.
    align(rs2::align(align_to)),
    // Each depth camera might have different units for depth pixels, so we get it here
    // Using the pipeline's profile, we can retrieve the device that the pipeline uses
    depth_scale(get_depth_scale(profile.get_device())),
    // From the caller.
    depth_clipping_min_distance(min_distance),
    depth_clipping_max_distance(max_distance),
    // Compute the foreground limit
    upper_limit(depth_clipping_max_distance / depth_scale),
    lower_limit(depth_clipping_min_distance / depth_scale)
  {
    // Get one frame to determine the size.
    auto frameset = wait();
    auto processed = align.process(frameset);
    rs2::video_frame other_frame = processed.first(align_to);

    name = std::string(profile.get_device().get_info(RS2_CAMERA_INFO_NAME));
    serial = std::string(profile.get_device().get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));

    width = other_frame.get_width();
    height = other_frame.get_height();
    bpp = format == video_format::rgb ? 3 : 4;
    
    std::copy_n(color, sizeof(green_bytes), green_bytes);
  }


  device::~device()
  {
    pipe->stop();
  }


  inline bool device::valid_distance(size_t pixels_distance) const
  {
    return pixels_distance > lower_limit && pixels_distance <= upper_limit;
  }


  void device::remove_background(uint8_t* dest, size_t framesize, rs2::video_frame& other_frame, const rs2::depth_frame& depth_frame_)
  {
    const uint16_t* depth_frame = reinterpret_cast<const uint16_t*>(depth_frame_.get_data());

    uint8_t* p_other_frame = reinterpret_cast<uint8_t*>(const_cast<void*>(other_frame.get_data()));

    assert(width == size_t(other_frame.get_width()));
    assert(height == size_t(other_frame.get_height()));
    size_t other_bpp = other_frame.get_bytes_per_pixel();
    assert(other_bpp == 3);

    size_t copy_height = width * height * bpp <= framesize ? height : (framesize / (width * bpp));

    if (bpp == other_bpp) {
      for (size_t y = 0; y < copy_height; y++) {
        auto depth_pixel_index = y * width;
        auto offset = depth_pixel_index * other_bpp;
        for (size_t x = 0; x < width; x++, ++depth_pixel_index, offset += other_bpp) {
          // Get the depth value of the current pixel
          auto pixels_distance = depth_frame[depth_pixel_index];

          std::memcpy(&dest[offset], valid_distance(pixels_distance) ? &p_other_frame[offset] : green_bytes, other_bpp);
        }
      }
    } else {
      for (size_t y = 0; y < copy_height; y++) {
        auto depth_pixel_index = y * width;
        auto src_offset = depth_pixel_index * other_bpp;
        auto dst_offset = depth_pixel_index * bpp;
        for (size_t x = 0; x < width; x++, ++depth_pixel_index, src_offset += other_bpp, dst_offset += bpp) {
          // Get the depth value of the current pixel
          auto pixels_distance = depth_frame[depth_pixel_index];

          if (valid_distance(pixels_distance)) {
            std::memcpy(&dest[dst_offset], &p_other_frame[src_offset], other_bpp);
            dest[dst_offset + 3] = 0xff;
          } else
            std::memcpy(&dest[dst_offset], green_bytes, bpp);
        }
      }
    }
  }


  rs2::frameset device::wait()
  {
    // Using the align object, we block the application until a frameset is available
    rs2::frameset frameset = pipe->wait_for_frames();

    // rs2::pipeline::wait_for_frames() can replace the device it uses in case of device error or disconnection.
    // Since rs2::align is aligning depth to some other stream, we need to make sure that the stream was not changed
    //  after the call to wait_for_frames();
    if (profile_changed(pipe->get_active_profile().get_streams(), profile.get_streams())) {
      //If the profile was changed, update the align object, and also get the new device's depth scale
      profile = pipe->get_active_profile();
      // Pipeline could choose a device that does not have a color stream
      // If there is no color stream, choose to align depth to another stream
      align_to = find_stream_to_align(profile.get_streams());
      // Create a rs2::align object.
      // rs2::align allows us to perform alignment of depth frames to others frames
      // The "align_to" is the stream type to which we plan to align depth frames.
      align = rs2::align(align_to);
      // Each depth camera might have different units for depth pixels, so we get it here
      // Using the pipeline's profile, we can retrieve the device that the pipeline uses
      depth_scale = get_depth_scale(profile.get_device());
    }

    return frameset;    
  }


  bool device::get_frame(uint8_t* dest, size_t framesize)
  {
    auto frameset = wait();

    // Get processed aligned frame
    auto processed = align.process(frameset);

    // Trying to get both other and aligned depth frames
    rs2::video_frame other_frame = processed.first(align_to);
    rs2::depth_frame aligned_depth_frame = processed.get_depth_frame();

    // If one of them is unavailable, continue iteration
    if (!aligned_depth_frame || !other_frame)
      return false;

    // Passing both frames to remove_background so it will "strip" the background
    remove_background(dest, framesize, other_frame, aligned_depth_frame);

    return true;
  }


  void device::set_color(uint32_t newcol)
  {
    green_bytes[0] = (newcol >> 16) & 0xff;
    green_bytes[1] = (newcol >> 8) & 0xff;
    green_bytes[2] = newcol & 0xff;
  }


  void device::set_max_distance(float newmax)
  {
    depth_clipping_max_distance = newmax;
    upper_limit = depth_clipping_max_distance / depth_scale;
  }


  greenscreen::greenscreen(video_format format_)
  : format(format_), max_width(0), max_height(0)
  {
    rs2::config config;

    dev = std::make_unique<device>(format, depth_clipping_min_distance, depth_clipping_max_distance, green_bytes, config);

    available.emplace_back(dev->name + " [" + dev->serial + "]", dev->width, dev->height, std::to_string(dev->width) + " × " + std::to_string(dev->height), dev->serial);

    rs2::context ctx;
    for (auto&& d : ctx.query_devices()) {
      auto serial = d.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
      auto devname = std::string(d.get_info(RS2_CAMERA_INFO_NAME)) + " [" + serial + "]";

      std::set<std::tuple<size_t,size_t>> resolutions;

      auto sensors = d.query_sensors();
      for (const auto& s : sensors) {
        if (s.as<rs2::color_sensor>()) {
          auto profiles = s.get_stream_profiles();
          for (const auto& p : profiles)
            if (const auto& vp = p.as<rs2::video_stream_profile>())
              resolutions.insert(std::make_tuple<size_t,size_t>(vp.width(), vp.height()));
        }
      }

      for (auto&& res : resolutions) {
        if (dev->serial != serial || dev->width != std::get<0>(res) || dev->height != std::get<1>(res)) {
          auto resstr = std::to_string(std::get<0>(res)) + " × " + std::to_string(std::get<1>(res));
          available.emplace_back(devname, std::get<0>(res), std::get<1>(res), resstr, std::string(serial));
        }

        max_width = std::max(max_width, size_t(std::get<0>(res)));
        max_height = std::max(max_height, size_t(std::get<1>(res)));
      }
    }

    // Don't sort the first element.
    std::sort(available.begin() + 1, available.end(), [](const auto& l, const auto& r){
      auto cr = std::get<0>(l).compare(std::get<0>(r));
      if (cr != 0)
        return cr < 0;
      if (std::get<1>(l) != std::get<1>(r))
        return std::get<1>(l) > std::get<1>(r);
      return std::get<2>(l) > std::get<2>(r);
    });
  }


  bool greenscreen::new_config(const std::string& serial, const std::string& resolution)
  {
    auto it = std::find_if(available.begin(), available.end(), [&serial, &resolution](const auto& e){
      return std::get<4>(e) == serial && std::get<3>(e) == resolution;
    });
    if (it == available.end())
      return false;

    if (dev->serial == serial && dev->width == std::get<1>(*it) && dev->height == std::get<2>(*it))
      // Nothing changed.
      return false;

    rs2::config config;
    config.enable_device(serial);
    config.enable_stream(RS2_STREAM_DEPTH);
    config.enable_stream(RS2_STREAM_COLOR, int(std::get<1>(*it)), int(std::get<2>(*it)));

    const std::lock_guard<std::mutex> guard(devlock);

    dev.reset(nullptr);
    dev = std::make_unique<device>(format, depth_clipping_min_distance, depth_clipping_max_distance, green_bytes, config);

    return true;
  }


  bool greenscreen::get_frame(uint8_t* dest, size_t framesize)
  {
    const std::lock_guard<std::mutex> guard(devlock);

    return dev->get_frame(dest, framesize);
  }


  size_t greenscreen::get_width() const{
    return dev->get_width();
  }
  size_t greenscreen::get_height() const
  {
    return dev->get_height();
  }
  size_t greenscreen::get_bpp() const
  {
    return dev->get_bpp();
  }
  size_t greenscreen::get_framesize() const
  {
    return max_width * max_height * (format == video_format::rgb ? 3 : 4);
  }


  void greenscreen::set_color(uint32_t newcol)
  {
    green_bytes[0] = (newcol >> 16) & 0xff;
    green_bytes[1] = (newcol >> 8) & 0xff;
    green_bytes[2] = newcol & 0xff;

    dev->set_color(newcol);
  }

  void greenscreen::set_transparency(unsigned char newa)
  {
    green_bytes[3] = newa;

    dev->set_transparency(newa);
  }

  void greenscreen::set_max_distance(float newmax)
  {
    depth_clipping_max_distance = newmax;

    dev->set_max_distance(newmax);
  }

} // namespace realsense
