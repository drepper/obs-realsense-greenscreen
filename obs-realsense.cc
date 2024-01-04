#include <atomic>
#include <cstdint>
#include <thread>

#include <obs/obs.h>
#include <obs/obs-frontend-api.h>
#include <obs/obs-module.h>
#include <obs/util/base.h>
#include <obs/util/config-file.h>
#include <obs/util/platform.h>
#include <obs/util/threading.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#include <QtCore/QString>
#pragma GCC diagnostic pop

#include "realsense-greenscreen.hh"

// XYZ DEBUG
// #include <iostream>
static int log_level = LOG_DEBUG;


namespace {

  struct config_type {
    config_type();
    ~config_type();

    void load();
    void save();

    void set_serial(const char* new_serial) { serial = new_serial; }
    void set_resolution(const char* new_resolution) { resolution = new_resolution; }
    void set_backgroundcolor(int new_backgroundcolor) { backgroundcolor = new_backgroundcolor; }
    void set_maxdistance(double new_maxdistance) { maxdistance = new_maxdistance; }
    void set_depthfilter(int new_depthfilter) { depthfilter = new_depthfilter; }
    const std::string& get_serial() const { return serial; }
    const std::string& get_resolution() const { return resolution; }
    int get_backgroundcolor() const { return backgroundcolor; }
    double get_maxdistance() const { return maxdistance; }
    int get_depthfilter() const { return depthfilter; }

  private:
    std::string serial;
    std::string resolution;
    int backgroundcolor;
    double maxdistance;
    int depthfilter;

    static constexpr char section_name[] = "realsense-greenscreen";
    static constexpr char param_serial[] = "serial";
    static constexpr char param_resolution[] = "resolution";
    static constexpr char param_backgroundcolor[] = "backgroundcolor";
    static constexpr char param_maxdistance[] = "maxdistance";
    static constexpr char param_depthfilter[] = "depthfilter";

    static void on_frontend_event(enum obs_frontend_event event, void* param);
  };

  config_type::config_type()
  : serial(""), resolution("")
  {
    config_t* obs_config = obs_frontend_get_profile_config();
    if (obs_config != nullptr) {
      config_set_default_string(obs_config, section_name, param_serial, serial.c_str());
      config_set_default_string(obs_config, section_name, param_resolution, resolution.c_str());
      config_set_default_int(obs_config, section_name, param_backgroundcolor, 0xdd44ff);
      config_set_default_double(obs_config, section_name, param_maxdistance, 1.0);
      config_set_default_int(obs_config, section_name, param_depthfilter, 4);
    }
  }

  config_type::~config_type()
  {
  }


  void config_type::load()
  {
    config_t* obs_config = obs_frontend_get_profile_config();

    serial = config_get_string(obs_config, section_name, param_serial);
    resolution = config_get_string(obs_config, section_name, param_resolution);
    backgroundcolor = config_get_int(obs_config, section_name, param_backgroundcolor);
    maxdistance = config_get_double(obs_config, section_name, param_maxdistance);
    depthfilter = config_get_int(obs_config, section_name, param_depthfilter);
  }

  void config_type::save()
  {
    config_t* obs_config = obs_frontend_get_profile_config();

    config_set_string(obs_config, section_name, param_serial, serial.c_str());
    config_set_string(obs_config, section_name, param_resolution, resolution.c_str());
    config_set_int(obs_config, section_name, param_backgroundcolor, backgroundcolor);
    config_set_double(obs_config, section_name, param_maxdistance, maxdistance);
    config_set_int(obs_config, section_name, param_depthfilter, depthfilter);

    config_save(obs_config);
  }


  std::unique_ptr<config_type> config;


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
    if (! config->get_serial().empty() || ! config->get_resolution().empty())
      cam.new_config(config->get_serial(), config->get_resolution());
    cam.set_color(config->get_backgroundcolor());
    cam.set_max_distance(config->get_maxdistance());
    cam.set_ndepth_history(config->get_depthfilter());
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
    blog(log_level, "obs-realsense: create");

    try {
      auto res = new plugin_context(source);

      obs_data_set_default_int(settings, "backgroundcolor", res->cam.get_color());
      obs_data_set_default_double(settings, "maxdistance", res->cam.get_max_distance());
      obs_data_set_default_int(settings, "depthfilter", res->cam.get_ndepth_history());

      return res;
    }
    catch (rs2::backend_error& e) {
      return nullptr;
    }

    blog(log_level, "obs-realsense: create done");
  }


  void plugin_destroy(void* data)
  {
    blog(log_level, "obs-realsense: destroy");

    auto ctx = static_cast<plugin_context*>(data);

    if (ctx)
      delete ctx;
  }


  void plugin_defaults(obs_data_t* settings)
  {
    blog(log_level, "obs-realsense: defaults");

    obs_data_set_string(settings, "devicename", config->get_serial().c_str());
    obs_data_set_string(settings, "resolution", config->get_resolution().c_str());
    obs_data_set_int(settings, "backgroundcolor", config->get_backgroundcolor());
    obs_data_set_double(settings, "maxdistance", config->get_maxdistance());
    obs_data_set_int(settings, "depthfilter", config->get_depthfilter());
  }


  obs_properties_t* plugin_properties(void *data)
  {
    blog(log_level, "obs-realsense: properties");

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

    auto resolutions = obs_properties_add_list(props, "resolution", obs_module_text("Resolution"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    for (const auto& e : ctx->cam.available)
      if (std::get<0>(e) == std::get<0>(ctx->cam.available.front()))
        obs_property_list_add_string(resolutions, std::get<3>(e).c_str(), std::get<3>(e).c_str());

    obs_properties_add_float_slider(props, "maxdistance", obs_module_text("Cutoff distance"), 0.25, 3.0, 0.0625);

    obs_properties_add_int_slider(props, "depthfilter", obs_module_text("Depth Filter"), 1, 16, 1);

    obs_properties_add_color(props, "backgroundcolor", obs_module_text("Background Color"));

    return props;
  }


  void plugin_update(void* data, obs_data_t* settings)
  {
    blog(log_level, "obs-realsense: update");

    auto ctx = static_cast<plugin_context*>(data);

    auto serial = obs_data_get_string(settings, "devicename");
    auto resolution = obs_data_get_string(settings, "resolution");
    blog(LOG_INFO, "serial=%s  resolution=%s", serial, resolution);
    ctx->cam.new_config(serial, resolution);
    if (serial[0] != '\0') {
      blog(log_level, "obs-realsense: serial=%s", serial);
      config->set_serial(serial);
    }
    if (resolution[0] != '\0') {
      blog(log_level, "obs-realsense: resolution=%s", resolution);
      config->set_resolution(resolution);
    }

    auto color = (uint32_t) obs_data_get_int(settings, "backgroundcolor");
    ctx->cam.set_color(color);
    config->set_backgroundcolor(color);
    blog(log_level, "obs-realsense: color=%06x", color);

    auto maxdistance = obs_data_get_double(settings, "maxdistance");
    ctx->cam.set_max_distance(maxdistance);
    config->set_maxdistance(maxdistance);
    blog(log_level, "obs-realsense: maxdistance=%f", maxdistance);

    auto depthfilter = obs_data_get_int(settings, "depthfilter");
    ctx->cam.set_ndepth_history(depthfilter);
    config->set_depthfilter(depthfilter);
    blog(log_level, "obs-realsense: depthfilter=%lld", depthfilter);

    config->save();
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
  blog(log_level, "loading obs-realsense");

  config = std::make_unique<config_type>();
  config->load();

  obs_register_source(&realsense_info);
  return true;
}
