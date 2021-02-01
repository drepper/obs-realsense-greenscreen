#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include <dlfcn.h>
#include <pwd.h>
#include <unistd.h>

#include <obs/obs.h>

using namespace std::string_literals;


namespace {

  const obs_source_info* source;

  struct os_sem_data;
  typedef struct os_event_data os_event_t;

  enum os_event_type {
    OS_EVENT_TYPE_AUTO,
    OS_EVENT_TYPE_MANUAL,
  };

  bool terminate = false;


  int test()
  {
    auto ctx = source->create(nullptr, nullptr);

    sleep(2);

    source->destroy(ctx);
    std::cout << std::endl;

    return 0;
  }

} // anonymous namespace


extern "C" {
  void obs_register_source_s(const obs_source_info* s, size_t)
  {
    source = s;
  }

  void os_event_destroy(os_event_t *)
  {
  }

  uint64_t os_gettime_ns()
  {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
  }

  void os_sleepto_ns(uint64_t t)
  {
    timespec ts;
    ts.tv_sec = t / 1000000000;
    ts.tv_nsec = t % 1000000000;
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
  }

  int os_event_init(os_event_t **, enum os_event_type)
  {
    return 0;
  }

  int os_event_try(os_event_t*)
  {
    return terminate ? 0 : EAGAIN;
  }

  int os_event_signal(os_event_t *)
  {
    terminate = true;
    return 0;
  }

  void obs_source_output_video(obs_source_t *, const struct obs_source_frame *frame)
  {
    unsigned t = 0;
    auto cp = frame->data[0];
    for (size_t y = 0; y < frame->height; cp += frame->linesize[0], ++y)
      for (size_t x = 0; x < frame->width; ++x)
        t += cp[x];
    asm("" :: "r" (t));

    std::cout << '.';
    std::cout.flush();
  }


  obs_properties_t *obs_properties_create(void)
  {
    return nullptr;
  }


  const char* obs_module_text(const char* val)
  {
    return val;
  }

  lookup_t* obs_module_load_locale(obs_module_t* /*module*/, const char* /*default_locale*/, const char* /*locale*/)
  {
    return nullptr;
  }

  obs_property_t* obs_properties_add_float_slider(obs_properties_t*, const char*, const char*, double, double, double)
  {
    return nullptr;
  }

  obs_property_t* obs_properties_add_color(obs_properties_t*, const char*, const char*)
  {
    return nullptr;
  }

  obs_property_t* obs_properties_add_list(obs_properties_t*, const char*, const char*, enum obs_combo_type, enum obs_combo_format)
  {
    return nullptr;
  }

  size_t obs_property_list_add_string(obs_property_t *, const char *, const char *)
  {
    return 0;
  }

  void obs_property_set_modified_callback2(obs_property_t *, obs_property_modified2_t, void *)
  {
  }

  long long int obs_data_get_int(obs_data_t* /*data*/, const char* /*name*/)
  {
    return 0xdd44ff;
  }

  double obs_data_get_double(obs_data_t* /*data*/, const char* /*name*/)
  {
    return 1.0;
  }

  const char* obs_data_get_string(obs_data_t */*data*/, const char */*name*/)
  {
    return nullptr;
  }

  void obs_data_set_string(obs_data_t */*data*/, const char */*name*/, const char */*val*/)
  {
  }

  void obs_data_set_default_double(obs_data_t*, const char*, double)
  {
  }

  void obs_data_set_default_int(obs_data_t*, const char*, long long)
  {
  }

  void text_lookup_destroy(lookup_t* /*lookup*/)
  {
  }

  bool text_lookup_getstr(lookup_t* /*lookup*/, const char* /*lookup_val*/, const char** /*out*/)
  {
    return false;
  }

}


int main()
{
	auto d = dlopen("./obs-realsense.so", RTLD_NOW);
  if (d == nullptr)
    throw std::runtime_error("cannot load obs-realsense.so: "s + dlerror());

  auto modinit = (bool(*)()) dlsym(d, "obs_module_load");

  modinit();

  return test();
}
