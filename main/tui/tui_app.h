#include "tui_state.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <memory>
#include <atomic>

class TUIApp {
private:
  std::shared_ptr<TUIState> m_state;
  ftxui::ScreenInteractive m_screen;
  std::atomic<bool> m_running;

public:
  TUIApp(std::shared_ptr<TUIState> state);

  void run();
  void stop();

private:
  ftxui::Component buildUI();
};