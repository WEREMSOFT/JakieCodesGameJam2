#include <stdbool.h>
#include "game/game.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

int main(int argc, char **argv)
{
	Game game = create_game();
	game.init(800, 600, 1, false);

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(*game.updateWeb, 0, false);
#else
	while (game.update());
	game.destroy();
#endif
	return 0;
}