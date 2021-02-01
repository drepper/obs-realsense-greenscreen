#include <array>
#include <cstring>
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

// XYZ DEBUG
// #include <iostream>


namespace {

  struct memory_pixbuf : public Gtk::DrawingArea
  {
    memory_pixbuf(realsense::greenscreen& cam_);
    virtual ~memory_pixbuf() { }

  private:

    virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
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

} // anonymous namespace


int main(int argc, char* argv[])
{
  bool transparent = false;
  realsense::greenscreen cam(transparent ? realsense::video_format::rgba : realsense::video_format::rgb);

  Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(argc, argv, "org.akkadia.testrealsense");
  pixbuf_window w(cam);
  return app->run(w);
}
