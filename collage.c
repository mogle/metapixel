/*
 * collage.c
 *
 * metapixel
 *
 * Copyright (C) 1997-2004 Mark Probst
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "api.h"

typedef struct _position_t
{
    int x, y;
    struct _position_t *next;
} position_t;

typedef struct
{
    int min_distance;
    position_t **collage_positions;
} collage_position_valid_data_t;

static int
pixel_valid_for_collage_position (void *_data, metapixel_t *pixel, unsigned int pixel_index, int x, int y)
{
    collage_position_valid_data_t *data = (collage_position_valid_data_t*)_data;
    position_t *position;

    if (data->min_distance <= 0)
	return 1;

    if (pixel->client_data != 0)
	for (position = data->collage_positions[pixel_index]; position != 0; position = position->next)
	    if (utils_manhattan_distance(x, y, position->x, position->y) < data->min_distance)
		return 0;

    return 1;
}

static void
add_collage_position (position_t **positions, int x, int y)
{
    position_t *position = (position_t*)malloc(sizeof(position_t));

    assert(position != 0);

    position->x = x;
    position->y = y;

    position->next = *positions;
    *positions = position;
}

static void
free_positions (position_t *positions)
{
    while (positions != 0)
    {
	position_t *next = positions->next;

	free(positions);

	positions = next;
    }
}

bitmap_t*
make_collage_mosaic (int num_libraries, library_t **libraries, bitmap_t *in_bitmap, float in_image_scale,
		     unsigned int small_width, unsigned int small_height,
		     int min_distance, metric_t *metric, unsigned int cheat)
{
    bitmap_t *out_bitmap;
    char *bitmap;
    int num_pixels_done = 0;
    position_t **collage_positions;
    unsigned int num_metapixels = library_count_metapixels(num_libraries, libraries);
    unsigned int i;

    if (num_metapixels == 0)
	return 0;
    
    if (in_image_scale != 1.0)
    {
	int new_width = (float)in_bitmap->width * in_image_scale;
	int new_height = (float)in_bitmap->height * in_image_scale;
	bitmap_t *scaled_bitmap;

	if (new_width < small_width || new_height < small_height)
	{
	    fprintf(stderr, "Error: Source image or scaling factor too small.\n");
	    exit(1);
	}

	//printf("Scaling source image to %dx%d\n", new_width, new_height);

	scaled_bitmap = bitmap_scale(in_bitmap, new_width, new_height, FILTER_MITCHELL);
	assert(scaled_bitmap != 0);

	bitmap_free(in_bitmap);
	in_bitmap = scaled_bitmap;
    }

    out_bitmap = bitmap_new_empty(COLOR_RGB_8, in_bitmap->width, in_bitmap->height);
    assert(out_bitmap != 0);

    bitmap = (char*)malloc(in_bitmap->width * in_bitmap->height);
    memset(bitmap, 0, in_bitmap->width * in_bitmap->height);

    collage_positions = (position_t**)malloc(num_metapixels * sizeof(position_t*));
    memset(collage_positions, 0, num_metapixels * sizeof(position_t*));

    while (num_pixels_done < in_bitmap->width * in_bitmap->height)
    {
	int i, j;
	int x, y;
	coeffs_t coeffs;
	match_t match;
	collage_position_valid_data_t valid_data = { min_distance, collage_positions };

	while (1)
	{
	    x = random() % in_bitmap->width - small_width / 2;
	    y = random() % in_bitmap->height - small_height / 2;

	    if (x < 0)
		x = 0;
	    if (x + small_width > in_bitmap->width)
		x = in_bitmap->width - small_width;

	    if (y < 0)
		y = 0;
	    if (y + small_height > in_bitmap->height)
		y = in_bitmap->height - small_height;

	    for (j = 0; j < small_height; ++j)
		for (i = 0; i < small_width; ++i)
		    if (!bitmap[(y + j) * in_bitmap->width + x + i])
			goto out;
	}

    out:
	metric_generate_coeffs_for_subimage(&coeffs, in_bitmap, x, y, small_width, small_height, metric);

	match = search_metapixel_nearest_to(num_libraries, libraries,
					    &coeffs, metric, x, y, 0, 0, pixel_valid_for_collage_position, &valid_data);
	/* FIXME: match.pixel can be zero! */
	assert(match.pixel != 0);
	if (!metapixel_paste(match.pixel, out_bitmap, x, y, small_width, small_height))
	    /* FIXME: this could go wrong! */
	    assert(0);

	if (min_distance > 0)
	    add_collage_position(&collage_positions[match.pixel_index], x, y);

	for (j = 0; j < small_height; ++j)
	    for (i = 0; i < small_width; ++i)
		if (!bitmap[(y + j) * in_bitmap->width + x + i])
		{
		    bitmap[(y + j) * in_bitmap->width + x + i] = 1;
		    ++num_pixels_done;
		}

	printf(".");
	fflush(stdout);
    }

    free(bitmap);

    for (i = 0; i < num_metapixels; ++i)
	if (collage_positions[i] != 0)
	    free_positions(collage_positions[i]);
    free(collage_positions);

    return out_bitmap;
}
