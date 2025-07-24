#include "buff_timers.h"
#include "zeal.h"

BuffDetails::BuffDetails(size_t index, Zeal::GameStructures::_GAMEBUFFINFO info) : BuffSlot(index), Buff(info) {}

// An improvement we could have would be having a pointer/hook to the function that provides
// the spell name based on the spell id.
void BuffTimers::print_timers(void)
{
  std::vector<BuffDetails> activeBuffs;
  std::ostringstream oss;

  auto CharInfo = Zeal::Game::get_self()->CharInfo;
  for (size_t i = 0; i < GAME_NUM_BUFFS; ++i)
  {
    WORD BuffId = CharInfo->Buff[i].SpellId;
    if (BuffId != USHRT_MAX)
    {
      activeBuffs.push_back(BuffDetails({i, CharInfo->Buff[i]}));
    }
  }

  oss << "[Buffs] ";
  if (activeBuffs.size() != 0)
  {
    for (size_t i = 0; i < activeBuffs.size(); ++i)
    {
      BuffDetails details = activeBuffs[i];
      if (details.Buff.SpellId != USHRT_MAX)
      {
        int Mins = ((details.Buff.Ticks) * 6) / 60;
        int Secs = ((details.Buff.Ticks) * 6) % 60;

        oss << "(" << details.BuffSlot + 1 << ")"
            << " " << Mins << "m" << Secs << "s";
        if ((i + 1) != activeBuffs.size())
        {
          oss << ", ";
        }
      }
    }
  }
  else
  {
    oss << "None";
  }
  oss << std::endl;

  Zeal::Game::print_chat(oss.str());
}

BuffTimers::BuffTimers(ZealService *zeal)
{
  if (!Zeal::Game::is_new_ui())
  {
    zeal->commands_hook->Add("/buffs", {}, "Prints your buff timers (mostly useful for oldui).",
                             [this](std::vector<std::string> &args)
                             {
                               print_timers();
                               return true;
                             });
  }
}
