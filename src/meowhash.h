#ifdef __arm64
  #include "meow_hash_armv8.h"
#else
  #include "meow_hash_x64_aesni.h"
#endif

