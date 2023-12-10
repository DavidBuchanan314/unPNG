#include "unpng.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

SDL_Surface* surface_from_unpng(struct unpng *img) {
	switch (img->pixfmt)
	{
	case UNPNG_PIXFMT_RGB888:
		return SDL_CreateRGBSurfaceFrom(
			(void*)img->buffer, img->width, img->height, 24, img->stride,
			0x0000ff,
			0x00ff00,
			0xff0000,
			0x000000
		);
	
	case UNPNG_PIXFMT_RGBA8888:
		return SDL_CreateRGBSurfaceFrom(
			(void*)img->buffer, img->width, img->height, 32, img->stride,
			0x000000ff,
			0x0000ff00,
			0x00ff0000,
			0xff000000
		);
	
	default:
		return NULL;
	}
}

int display_unpng(struct unpng *img) {
	SDL_Window* window = NULL;
	SDL_Surface* windowSurface = NULL;
	SDL_Surface* imgSurface = NULL;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		SDL_Log("Could not init SDL2: %s", SDL_GetError());
		return -1;
	}
	
	// create window
	window = SDL_CreateWindow(
		"unPNG viewer",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		img->width, img->height,
		SDL_WINDOW_SHOWN
	);
	if (window == NULL) {
		SDL_Log("Could not create window: %s", SDL_GetError());
		return -1;
	}

	windowSurface = SDL_GetWindowSurface(window);
	if (windowSurface == NULL) {
		SDL_Log("Could not get window surface: %s", SDL_GetError());
		return -1;
	}

	imgSurface = surface_from_unpng(img);
	if (imgSurface == NULL) {
		SDL_Log("Could not create imgSurface: %s", SDL_GetError());
		return -1;
	}

	// blit our image onto the window surface
	SDL_BlitSurface(imgSurface, NULL, windowSurface, NULL);
	SDL_UpdateWindowSurface(window);

	for (;;) {
		// drain SDL event queue
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				goto cleanup;
			}
		}
		// do something
		SDL_Delay(10);
	}

cleanup:
	SDL_FreeSurface(imgSurface);
	SDL_DestroyWindow(window);
	SDL_Quit();
	
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "USAGE: %s input.un.png\n", argv[0]);
		return -1;
	}

	FILE *f = fopen(argv[1], "r");
	if (f == NULL) {
		perror("fopen");
		return -1;
	}

	fseek(f, 0L, SEEK_END);
	size_t file_len = ftell(f);
	rewind(f);

	uint8_t *buf = malloc(file_len);
	if (buf == NULL) {
		perror("malloc");
		return -1;
	}

	if (fread(buf, 1, file_len, f) != file_len) {
		fprintf(stderr, "incomplete fread\n");
		return -1;
	}

	struct unpng img;
	int res = unpng_parse(&img, buf, file_len);
	if (res != UNPNG_OK) {
		fprintf(stderr, "unpng_parse: %d\n", res);
		return -1;
	}

	printf("Parsed unPNG!\n");
	printf("width=%u\n", img.width);
	printf("height=%u\n", img.height);
	printf("stride=%u\n", img.stride);
	printf("pixfmt=%u\n", img.pixfmt);

	res = display_unpng(&img);

	free(buf);
	return res;
}
