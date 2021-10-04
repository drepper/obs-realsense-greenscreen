#include <cstdint>
#include <thread>

#include <obs/obs.h>
#include <obs/obs-module.h>
#include <obs/util/platform.h>
#include <obs/util/threading.h>

#include "realsense-greenscreen.hh"

// XYZ DEBUG
#include <iostream>


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
    auto framesize = cam.get_framesize();
    std::unique_ptr<uint8_t[]> mem(new uint8_t[framesize]);

    obs_source_frame obs_frame;
    memset(&obs_frame, '\0', sizeof(obs_frame));
    obs_frame.data[0] = mem.get();
    // obs_frame.linesize[0] = cam.get_width() * cam.get_bpp();
    // obs_frame.width = cam.get_width();
    // obs_frame.height = cam.get_height();
    obs_frame.format = VIDEO_FORMAT_RGBA;

    auto cur_time = os_gettime_ns();

    while (! terminate) {
      cam.get_frame(mem.get(), framesize);
      obs_frame.linesize[0] = cam.get_width() * cam.get_bpp();
      obs_frame.width = cam.get_width();
      obs_frame.height = cam.get_height();

      obs_frame.timestamp = cur_time;
      obs_source_output_video(source, &obs_frame);
      //
      os_sleepto_ns(cur_time += delay);
    }
  }


  const char* plugin_getname(void*)
  {
    return "RealSense Greenscreen";
  }


  bool device_selected(void* /*data*/, obs_properties_t* /*props*/, obs_property_t* /*p*/, obs_data_t* /*settings*/)
  {
    // std::cout << "device selected " << obs_data_get_string(settings, "devicename") << "  resolution " << obs_data_get_string(settings, "resolutions") << std::endl;
    return true;
  }


  void* plugin_create(obs_data_t* settings, obs_source_t* source)
  {
    try {
      auto res = new plugin_context(source);

      obs_data_set_default_int(settings, "backgroundcolor", res->cam.get_color());
      obs_data_set_default_double(settings, "maxdistance", res->cam.get_max_distance());

      return res;
    }
    catch (rs2::backend_error& e) {
      return nullptr;
    }
  }


  void plugin_destroy(void* data)
  {
    auto ctx = static_cast<plugin_context*>(data);

    if (ctx)
      delete ctx;
  }


  void plugin_defaults(obs_data_t* settings)
  {
    obs_data_set_string(settings, "devicename", "");
    obs_data_set_string(settings, "resolutions", "");
  }


  obs_properties_t* plugin_properties(void *data)
  {
    auto ctx = static_cast<plugin_context*>(data);

    auto props = obs_properties_create();

    auto devicename = obs_properties_add_list(props, "devicename", obs_module_text("Device"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    std::string last;
    for (const auto& e : ctx->cam.available)
      if (auto cur = std::get<0>(e); cur != last) {
        obs_property_list_add_string(devicename, cur.c_str(), std::get<4>(e).c_str());
        last = cur;
      }
    obs_property_set_modified_callback2(devicename, device_selected, data);

    auto resolutions = obs_properties_add_list(props, "resolutions", obs_module_text("Resolution"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    for (const auto& e : ctx->cam.available)
      if (std::get<0>(e) == std::get<0>(ctx->cam.available.front())) 
        obs_property_list_add_string(resolutions, std::get<3>(e).c_str(), std::get<3>(e).c_str());

    obs_properties_add_float_slider(props, "maxdistance", obs_module_text("Cutoff distance"), 0.25, 3.0, 0.0625);

    obs_properties_add_color(props, "backgroundcolor", obs_module_text("Background Color"));

    return props;
  }


  void plugin_update(void* data, obs_data_t* settings)
  {
    auto ctx = static_cast<plugin_context*>(data);

    auto serial = obs_data_get_string(settings, "devicename");
    auto resolution = obs_data_get_string(settings, "resolutions");
    ctx->cam.new_config(serial, resolution);

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
    .get_defaults = plugin_defaults,
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
