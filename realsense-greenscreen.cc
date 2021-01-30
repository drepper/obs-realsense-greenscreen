#include <cassert>
#include <cstring>
#include <stdexcept>

#include "realsense-greenscreen.hh"

// XYZ Debug
#include <iostream>


namespace realsense {

  namespace {

    float get_depth_scale(rs2::device dev)
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
        //If previous profile is in current (maybe just added another)
        auto itr = std::find_if(std::begin(current), std::end(current), [&sp](const rs2::stream_profile& current_sp) { return sp.unique_id() == current_sp.unique_id(); });
        if (itr == std::end(current)) //If it previous stream wasn't found in current
          return true;
      }
      return false;
    }

  }// anonymous namespace


  greenscreen::greenscreen()
    // Calling pipeline's start() without any additional parameters will start the first device
    // with its default streams.
    // The start function returns the pipeline profile which the pipeline used to start the device
  : profile(pipe.start()),
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
    // Compute the foreground limit
    upper_limit(depth_clipping_max_distance / depth_scale),
    lower_limit(depth_clipping_min_distance / depth_scale)
  {
    // Get one frame to determine the size.
    auto frameset = wait();
    auto processed = align.process(frameset);
    rs2::video_frame other_frame = processed.first(align_to);
    width = other_frame.get_width();
    height = other_frame.get_height();
  }


  void greenscreen::remove_background(uint8_t* dest, rs2::video_frame& other_frame, const rs2::depth_frame& depth_frame_)
  {
    const uint16_t* depth_frame = reinterpret_cast<const uint16_t*>(depth_frame_.get_data());

    uint8_t* p_other_frame = reinterpret_cast<uint8_t*>(const_cast<void*>(other_frame.get_data()));

    assert(width == size_t(other_frame.get_width()));
    assert(height == size_t(other_frame.get_height()));
    size_t other_bpp = other_frame.get_bytes_per_pixel();
    assert(other_bpp == 3);

    for (size_t y = 0; y < height; y++) {
      auto depth_pixel_index = y * width;
      auto offset = depth_pixel_index * other_bpp;
      for (size_t x = 0; x < width; x++, ++depth_pixel_index, offset += other_bpp) {
        // Get the depth value of the current pixel
        auto pixels_distance = depth_frame[depth_pixel_index];

        std::memcpy(&dest[offset], pixels_distance <= lower_limit || pixels_distance > upper_limit ? green_bytes : &p_other_frame[offset], other_bpp);
      }
    }
  }


  rs2::frameset greenscreen::wait()
  {
    // Using the align object, we block the application until a frameset is available
    rs2::frameset frameset = pipe.wait_for_frames();

    // rs2::pipeline::wait_for_frames() can replace the device it uses in case of device error or disconnection.
    // Since rs2::align is aligning depth to some other stream, we need to make sure that the stream was not changed
    //  after the call to wait_for_frames();
    if (profile_changed(pipe.get_active_profile().get_streams(), profile.get_streams())) {
      //If the profile was changed, update the align object, and also get the new device's depth scale
      profile = pipe.get_active_profile();
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


  bool greenscreen::get_frame(uint8_t* dest)
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
    remove_background(dest, other_frame, aligned_depth_frame);

    return true;
  }

} // namespace realsense
