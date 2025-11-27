const int KEY_VOLUME_UP = 0;
const int KEY_VOLUME_DOWN = 1;
const int KEY_BRIGHTNESS_UP = 2;
const int KEY_BRIGHTNESS_DOWN = 3;
const int KEY_MIC_MUTE = 4;      // Microphone mute (for single tap)
const int KEY_LOCK_WORKSTATION = 5; // Lock workstation - Win+L (for double tap)

class KeyReporter {
public:
  virtual void reportKey(int key, int count) = 0;
  virtual void reportScroll(int steps) = 0;
};
