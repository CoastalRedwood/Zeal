#pragma once

#include <Windows.h>

#include <deque>
#include <string>

// Activates hotbar page 10 buttons and social page 10 macros without switching the visible
// hotbar page. This effectively provides extra macro keys backed by the last (rarely used)
// page of hotbuttons and socials. The bindable keys are registered in ZealService::AddBinds().
//
// Social macros are read directly from the character ini ([Socials] Page10Button<N>Line<1-5>)
// and support the native "/pause <ticks>[, <command>]" syntax. Lines are queued and executed
// with their pause delays in the MainLoop callback. Note the client only rewrites the ini at
// camp / exit, so social edits made in-game are not picked up until then.
class Page10Binds {
 public:
  Page10Binds(class ZealService *zeal);

  void press_hotbutton(int slot);   // Presses a page 10 hotbutton (0-based) without changing pages.
  void execute_social(int button);  // Queues the social lines of a page 10 button (1-based).

 private:
  struct QueuedCommand {
    std::string command;
    ULONGLONG execute_time;  // GetTickCount64() timestamp when the command is due.
  };

  void process_queue();  // MainLoop callback that executes due commands.

  std::deque<QueuedCommand> command_queue;  // Pending social lines awaiting their /pause delays.
};
