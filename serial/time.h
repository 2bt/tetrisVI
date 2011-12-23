#include <SDL/SDL.h>

// milliseconds
inline unsigned int get_time() { return SDL_GetTicks(); }
inline void wait(unsigned int t) { SDL_Delay(t); }
