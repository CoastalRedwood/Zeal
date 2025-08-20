#pragma once

class Alarm {
 public:
  Alarm(class ZealService *zeal);
  ~Alarm(){};
  void Set(int minutes, int seconds);
  void Halt(void);

 private:
  void callback_main();
  unsigned long long start_time = 0;
  double duration = 0;
  bool enabled = false;
};
