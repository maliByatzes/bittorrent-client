#include "tui_app.h"
#include <thread>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/loop.hpp>

using namespace ftxui;

TUIApp::TUIApp(std::shared_ptr<TUIState> state)
  : m_state(state),
    m_screen(ScreenInteractive::Fullscreen()),
    m_running(false)
{}

void TUIApp::run() {
  m_running = true;

  auto ui = buildUI();

  Loop loop(&m_screen, ui);

  while (m_running && loop.HasQuitted() == false) {
    loop.RunOnce();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void TUIApp::stop() {
  m_running = false;
  m_screen.ExitLoopClosure()();
}

ftxui::Component TUIApp::buildUI() {
  auto renderer = Renderer([this] {
    std::string filename = m_state->getFilename();
    uint64_t total_size = m_state->getTotalSize();
    std::string status = m_state->getStatus();

    double size_mb = total_size / (1024.0 * 1024.0);
    std::string size_str;
    if (size_mb >= 1024.0) {
      size_str = std::to_string(size_mb / 1024.0).substr(0, 4) + " GB";
    } else {
      size_str = std::to_string(size_mb).substr(0, 5) + " MB";
    }

    return vbox({
      hbox({
        text("BitTorrent Client v0.0.1") | bold,
        filler(),
        text("Press 'q' to quit") | dim,
      }) | bgcolor(Color::Blue) | color(Color::White),

      separator(),

      vbox({
        text(""),
        hbox({
          text("File: ") | bold,
          text(filename.empty() ? "(none)" : filename),
        }),
        text(""),
        hbox({
          text("Size: ") | bold,
          text(size_str),
        }),
        text(""),
      }) | border,

    });
  });

  auto component = CatchEvent(renderer, [this](Event event) {
    if (event == Event::Character('q') || event == Event::Escape) {
      stop();
      return true;
    }
    return false;
  });

  return component;
}