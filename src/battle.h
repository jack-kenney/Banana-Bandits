#ifndef BATTLE_H
#define BATTLE_H

// Reserve a safe mixer channel for SFX. Stereo waveforms need ch+1, so avoid
// using the very last channel.
#define SFX_CH 30

#define STRINGIFY(x) #x
#define STYLE(id) "^0" STRINGIFY(id)
#define STYLE_TITLE 1
#define STYLE_GREY 2
#define STYLE_GREEN 3

void battle_mode_loop(void);

#endif // BATTLE_H
