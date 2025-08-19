#pragma once

class Raid {
 public:
  Raid(class ZealService *zeal);
  ~Raid();

 private:
  void callback_main();
};
