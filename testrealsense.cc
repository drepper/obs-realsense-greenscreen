#include <array>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wredundant-decls"
#include <glibmm/main.h>
#include <gdkmm/general.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/application.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#pragma GCC diagnostic pop

#include "realsense-greenscreen.hh"


namespace {

  struct memory_pixbuf : public Gtk::DrawingArea
  {
    memory_pixbuf(realsense::greenscreen& cam_);
    virtual ~memory_pixbuf() { }

  private:

    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
    bool on_timeout();

    realsense::greenscreen& cam;
    size_t frame_size;

    Glib::RefPtr<Gdk::Pixbuf> m_image;
  };

  memory_pixbuf::memory_pixbuf(realsense::greenscreen& cam_)
  : cam(cam_)
  {
    bool transparent = cam.get_format() == realsense::video_format::rgba;
    if (transparent)
      cam.set_transparency(0);
    m_image = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, transparent, 8, cam.get_width(), cam.get_height());
    frame_size = cam.get_width() * cam.get_height() * (transparent ? 4 : 3);
    Glib::signal_timeout().connect(sigc::mem_fun(*this, &memory_pixbuf::on_timeout), 33);
  }

  bool memory_pixbuf::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
  {
    if (m_image) {
      cam.get_frame(m_image->get_pixels(), frame_size);

      Gdk::Cairo::set_source_pixbuf(cr, m_image, 0, 0);
      cr->paint();
    }
    return true;
  };

  bool memory_pixbuf::on_timeout()
  {
    queue_draw();
    return true;
  }


  struct pixbuf_window : public Gtk::Window
  {
    pixbuf_window(realsense::greenscreen& cam_);
  private:
    bool on_keypress(GdkEventKey* event);
    memory_pixbuf m;
  };

  pixbuf_window::pixbuf_window(realsense::greenscreen& cam)
  : m(cam)
  {
    set_title("Test of RealSense greenscreen");
    set_default_size(cam.get_width(), cam.get_height());
    signal_key_press_event().connect(sigc::mem_fun(*this, &pixbuf_window::on_keypress), false);

    add(m);
    m.show();
  }

  bool pixbuf_window::on_keypress(GdkEventKey* event)
  {
    if (event->keyval == GDK_KEY_Escape) {
      hide();
      return true;
    }
    return false;
  }


  struct testrealsense final : Gtk::Application
  {
    testrealsense(realsense::greenscreen& cam_)
    : Gtk::Application("org.akkadia.testrealsense", Gio::APPLICATION_HANDLES_COMMAND_LINE),
      cam(cam_)
    {}

    int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine>& cmd) override
    {
      int argc;
      auto argv = cmd->get_arguments(argc);

      std::string serial;
      long width = -1;
      long height = -1;
      while (true) {
        auto opt = getopt(argc, argv, "s:w:h:f:");
        if (opt == -1)
          break;
        switch (opt) {
        case 's':
          serial = optarg;
          break;
        case 'w':
          width = std::atoi(optarg);
          break;
        case 'h':
          height = std::atoi(optarg);
          break;
        case 'f':
          cam.set_ndepth_history(std::atoi(optarg));
          break;
        }
      }

      if (! serial.empty() || width != -1 || height != -1) {
        bool found = false;
        for (const auto& d : cam.available)
          if ((serial.empty() || std::get<4>(d) == serial) &&
              (width == -1 || std::get<1>(d) == size_t(width)) &&
              (height == -1 || std::get<2>(d) == size_t(height))) {
            cam.new_config(std::get<4>(d), std::get<3>(d));
            found = true;
            break;
          }
        if (! found)
          throw std::runtime_error("did not find matching device");
      }

      activate();
      return 0;
    }

    std::unique_ptr<pixbuf_window> win;
    realsense::greenscreen& cam;

    void on_activate() override
    {
      win = std::make_unique<pixbuf_window>(cam);
      add_window(*win);
      win->show();
    }
  };

} // anonymous namespace


int main(int argc, char* argv[])
{
  bool transparent = false;
  realsense::greenscreen cam(transparent ? realsense::video_format::rgba : realsense::video_format::rgb);

  if (argc > 1 && strcmp(argv[1], "-l") == 0) {
    for (const auto& d : cam.available) {
      std::cout << "serial=" << std::get<4>(d) << "  width=" << std::setw(4) << std::get<1>(d) << "  height=" << std::setw(4) << std::get<2>(d) << std::endl;
    }
    return 0;
  }

  return testrealsense(cam).run(argc, argv);
}
