#include <cstdint>
#include <thread>

#include <obs/obs.h>
#include <obs/obs-module.h>
#include <obs/util/platform.h>
#include <obs/util/threading.h>

#include "realsense-greenscreen.hh"

// XYZ DEBUG
// #include <iostream>


namespace {

  struct plugin_context {
    plugin_context(obs_source_t* source_);
    ~plugin_context();

    static void call_video_thread(plugin_context* p) { p->video_thread(); }
    void video_thread();

    obs_source_t* source;
    realsense::greenscreen cam;
    std::thread thread;
    std::atomic<bool> terminate = false;

    // Camera frequency (picture per second)
    static constexpr uint64_t freq = 30;
    // Derived delay between picture transfers.
    static constexpr uint64_t delay = 1'000'000'000 / freq;
  };


  plugin_context::plugin_context(obs_source_t* source_)
  : source(source_),
    cam(realsense::video_format::rgba),
    thread(call_video_thread, this)
  {
  }


  plugin_context::~plugin_context()
  {
    terminate = true;
    thread.join();
  }


  void plugin_context::video_thread()
  {
    std::unique_ptr<uint8_t[]> mem(new uint8_t[cam.get_framesize()]);

    obs_source_frame obs_frame;
    memset(&obs_frame, '\0', sizeof(obs_frame));
    obs_frame.data[0] = mem.get();
    obs_frame.linesize[0] = cam.get_width() * cam.get_bpp();
    obs_frame.width = cam.get_width();
    obs_frame.height = cam.get_height();
    obs_frame.format = VIDEO_FORMAT_RGBA;

    auto cur_time = os_gettime_ns();

    while (! terminate) {
      cam.get_frame(mem.get());
      obs_frame.timestamp = cur_time;
      obs_source_output_video(source, &obs_frame);
      //
      os_sleepto_ns(cur_time += delay);
    }
  }


  const char *plugin_getname(void *)
  {
    return "RealSense Greenscreen";
  }


  void* plugin_create(obs_data_t* settings, obs_source_t* source)
  {
    auto res = new plugin_context(source);

    obs_data_set_default_int(settings, "backgroundcolor", res->cam.get_color());
    obs_data_set_default_double(settings, "maxdistance", res->cam.get_max_distance());

    return res;
  }


  void plugin_destroy(void* data)
  {
    auto ctx = static_cast<plugin_context*>(data);

    if (ctx)
      delete ctx;
  }


  obs_properties_t* plugin_properties(void *)
  {
    auto props = obs_properties_create();

    obs_properties_add_float_slider(props, "maxdistance", obs_module_text("Cutoff distance"), 0.25, 3.0, 0.0625);

    obs_properties_add_color(props, "backgroundcolor", obs_module_text("Background Color"));

    return props;
  }


  void plugin_update(void* data, obs_data_t* settings)
  {
    auto ctx = static_cast<plugin_context*>(data);

    auto color = (uint32_t) obs_data_get_int(settings, "backgroundcolor");
    ctx->cam.set_color(color);

    auto maxdistance = obs_data_get_double(settings, "maxdistance");
    ctx->cam.set_max_distance(maxdistance);
  }


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  obs_source_info realsense_info = {
    .id = "RealSense Greenscreen" ,
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_ASYNC_VIDEO,
    .get_name = plugin_getname,
    .create = plugin_create,
    .destroy = plugin_destroy,
    .get_properties = plugin_properties,
    .update = plugin_update,
    .icon_type = OBS_ICON_TYPE_CAMERA,
  };
#pragma GCC diagnostic pop

} // anonymous namespace


OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-realsense", "en-US")

extern "C" bool obs_module_load(void)
{
  obs_register_source(&realsense_info);
  return true;
}
