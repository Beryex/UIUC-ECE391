/*									tab:8
 *
 * photo.c - photo display functions
 *
 * "Copyright (c) 2011 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 * 
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO 
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL 
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, 
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE 
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE, 
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:	    Steve Lumetta
 * Version:	    3
 * Creation Date:   Fri Sep  9 21:44:10 2011
 * Filename:	    photo.c
 * History:
 *	SL	1	Fri Sep  9 21:44:10 2011
 *		First written (based on mazegame code).
 *	SL	2	Sun Sep 11 14:57:59 2011
 *		Completed initial implementation of functions.
 *	SL	3	Wed Sep 14 21:49:44 2011
 *		Cleaned up code for distribution.
 */


#include <string.h>

#include "assert.h"
#include "modex.h"
#include "photo.h"
#include "photo_headers.h"
#include "world.h"


/* types local to this file (declared in types.h) */

/* 
 * A room photo.  Note that you must write the code that selects the
 * optimized palette colors and fills in the pixel data using them as 
 * well as the code that sets up the VGA to make use of these colors.
 * Pixel data are stored as one-byte values starting from the upper
 * left and traversing the top row before returning to the left of
 * the second row, and so forth.  No padding should be used.
 */
struct photo_t {
    photo_header_t hdr;			/* defines height and width */
    uint8_t        palette[192][3];     /* optimized palette colors */
    uint8_t*       img;                 /* pixel data               */
};

/* 
 * An object image.  The code for managing these images has been given
 * to you.  The data are simply loaded from a file, where they have 
 * been stored as 2:2:2-bit RGB values (one byte each), including 
 * transparent pixels (value OBJ_CLR_TRANSP).  As with the room photos, 
 * pixel data are stored as one-byte values starting from the upper 
 * left and traversing the top row before returning to the left of the 
 * second row, and so forth.  No padding is used.
 */
struct image_t {
    photo_header_t hdr;			/* defines height and width */
    uint8_t*       img;                 /* pixel data               */
};

/*
 * The Node for the color clustering algorithm based on Octrees
 */
typedef struct Octree_Node_t {
	uint32_t Pattern_Index;		/* RRRRGGGGBBBB for level4 and RRGGBB for level 2 */
	uint32_t Red_Sum;			/* the sum of red component for this node */
	uint32_t Blue_Sum;			/* the sum of blue component for this node */
	uint32_t Green_Sum;			/* the sum of green component for this node */
	uint32_t Pixel_Num;			/* the number of pixels belonging to this node */
	uint32_t Removed;			/* whether is most frequent and been removed, only used for Level4 node */
} Octree_Node_t;


/* file-scope variables */

/* 
 * The room currently shown on the screen.  This value is not known to 
 * the mode X code, but is needed when filling buffers in callbacks from 
 * that code (fill_horiz_buffer/fill_vert_buffer).  The value is set 
 * by calling prep_room.
 */
static const room_t* cur_room = NULL; 


/* 
 * fill_horiz_buffer
 *   DESCRIPTION: Given the (x,y) map pixel coordinate of the leftmost 
 *                pixel of a line to be drawn on the screen, this routine 
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS: (x,y) -- leftmost pixel of line to be drawn 
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void
fill_horiz_buffer (int x, int y, unsigned char buf[SCROLL_X_DIM])
{
    int            idx;   /* loop index over pixels in the line          */ 
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgx;  /* loop index over pixels in object image      */ 
    int            yoff;  /* y offset into object image                  */ 
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo (cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_X_DIM; idx++) {
        buf[idx] = (0 <= x + idx && view->hdr.width > x + idx ?
		    view->img[view->hdr.width * y + x + idx] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate (cur_room); NULL != obj;
    	 obj = obj_next (obj)) {
	obj_x = obj_get_x (obj);
	obj_y = obj_get_y (obj);
	img = obj_image (obj);

        /* Is object outside of the line we're drawing? */
	if (y < obj_y || y >= obj_y + img->hdr.height ||
	    x + SCROLL_X_DIM <= obj_x || x >= obj_x + img->hdr.width) {
	    continue;
	}

	/* The y offset of drawing is fixed. */
	yoff = (y - obj_y) * img->hdr.width;

	/* 
	 * The x offsets depend on whether the object starts to the left
	 * or to the right of the starting point for the line being drawn.
	 */
	if (x <= obj_x) {
	    idx = obj_x - x;
	    imgx = 0;
	} else {
	    idx = 0;
	    imgx = x - obj_x;
	}

	/* Copy the object's pixel data. */
	for (; SCROLL_X_DIM > idx && img->hdr.width > imgx; idx++, imgx++) {
	    pixel = img->img[yoff + imgx];

	    /* Don't copy transparent pixels. */
	    if (OBJ_CLR_TRANSP != pixel) {
		buf[idx] = pixel;
	    }
	}
    }
}


/* 
 * fill_vert_buffer
 *   DESCRIPTION: Given the (x,y) map pixel coordinate of the top pixel of 
 *                a vertical line to be drawn on the screen, this routine 
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS: (x,y) -- top pixel of line to be drawn 
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void
fill_vert_buffer (int x, int y, unsigned char buf[SCROLL_Y_DIM])
{
    int            idx;   /* loop index over pixels in the line          */ 
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgy;  /* loop index over pixels in object image      */ 
    int            xoff;  /* x offset into object image                  */ 
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo (cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_Y_DIM; idx++) {
        buf[idx] = (0 <= y + idx && view->hdr.height > y + idx ?
		    view->img[view->hdr.width * (y + idx) + x] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate (cur_room); NULL != obj;
    	 obj = obj_next (obj)) {
	obj_x = obj_get_x (obj);
	obj_y = obj_get_y (obj);
	img = obj_image (obj);

        /* Is object outside of the line we're drawing? */
	if (x < obj_x || x >= obj_x + img->hdr.width ||
	    y + SCROLL_Y_DIM <= obj_y || y >= obj_y + img->hdr.height) {
	    continue;
	}

	/* The x offset of drawing is fixed. */
	xoff = x - obj_x;

	/* 
	 * The y offsets depend on whether the object starts below or 
	 * above the starting point for the line being drawn.
	 */
	if (y <= obj_y) {
	    idx = obj_y - y;
	    imgy = 0;
	} else {
	    idx = 0;
	    imgy = y - obj_y;
	}

	/* Copy the object's pixel data. */
	for (; SCROLL_Y_DIM > idx && img->hdr.height > imgy; idx++, imgy++) {
	    pixel = img->img[xoff + img->hdr.width * imgy];

	    /* Don't copy transparent pixels. */
	    if (OBJ_CLR_TRANSP != pixel) {
		buf[idx] = pixel;
	    }
	}
    }
}


/* 
 * image_height
 *   DESCRIPTION: Get height of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
image_height (const image_t* im)
{
    return im->hdr.height;
}


/* 
 * image_width
 *   DESCRIPTION: Get width of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
image_width (const image_t* im)
{
    return im->hdr.width;
}

/* 
 * photo_height
 *   DESCRIPTION: Get height of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
photo_height (const photo_t* p)
{
    return p->hdr.height;
}


/* 
 * photo_width
 *   DESCRIPTION: Get width of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
photo_width (const photo_t* p)
{
    return p->hdr.width;
}


/* 
 * prep_room
 *   DESCRIPTION: Prepare a new room for display.  You might want to set
 *                up the VGA palette registers according to the color
 *                palette that you chose for this room.
 *   INPUTS: r -- pointer to the new room
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: changes recorded cur_room for this file
 */
void
prep_room (const room_t* r)
{
    /* Record the current room. */
    cur_room = r;
	/* reload the palette for this room's photo */
	photo_t* cur_photo = room_photo(r);
	fill_palette_color((unsigned char*)(cur_photo->palette));
}


/* 
 * read_obj_image
 *   DESCRIPTION: Read size and pixel data in 2:2:2 RGB format from a
 *                photo file and create an image structure from it.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the image
 */
image_t*
read_obj_image (const char* fname)
{
    FILE*    in;		/* input file               */
    image_t* img = NULL;	/* image structure          */
    uint16_t x;			/* index over image columns */
    uint16_t y;			/* index over image rows    */
    uint8_t  pixel;		/* one pixel from the file  */

    /* 
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the image pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen (fname, "r+b")) ||
	NULL == (img = malloc (sizeof (*img))) ||
	NULL != (img->img = NULL) || /* false clause for initialization */
	1 != fread (&img->hdr, sizeof (img->hdr), 1, in) ||
	MAX_OBJECT_WIDTH < img->hdr.width ||
	MAX_OBJECT_HEIGHT < img->hdr.height ||
	NULL == (img->img = malloc 
		 (img->hdr.width * img->hdr.height * sizeof (img->img[0])))) {
	if (NULL != img) {
	    if (NULL != img->img) {
	        free (img->img);
	    }
	    free (img);
	}
	if (NULL != in) {
	    (void)fclose (in);
	}
	return NULL;
    }

    /* 
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order (top to bottom).
     */
    for (y = img->hdr.height; y-- > 0; ) {

	/* Loop over columns from left to right. */
	for (x = 0; img->hdr.width > x; x++) {

	    /* 
	     * Try to read one 8-bit pixel.  On failure, clean up and 
	     * return NULL.
	     */
	    if (1 != fread (&pixel, sizeof (pixel), 1, in)) {
		free (img->img);
		free (img);
	        (void)fclose (in);
		return NULL;
	    }

	    /* Store the pixel in the image data. */
	    img->img[img->hdr.width * y + x] = pixel;
	}
    }

    /* All done.  Return success. */
    (void)fclose (in);
    return img;
}


/* 
 * read_photo
 *   DESCRIPTION: Read size and pixel data in 5:6:5 RGB format from a
 *                photo file and create a photo structure from it.
 *                Code provided simply maps to 2:2:2 RGB.  You must
 *                replace this code with palette color selection, and
 *                must map the image pixels into the palette colors that
 *                you have defined.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the photo
 */
photo_t*
read_photo (const char* fname)
{
    FILE*    in;	/* input file               */
    photo_t* p = NULL;	/* photo structure          */
    uint16_t x;		/* index over image columns */
    uint16_t y;		/* index over image rows    */
    uint16_t pixel;	/* one pixel from the file  */

    /* 
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the photo pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen (fname, "r+b")) ||
	NULL == (p = malloc (sizeof (*p))) ||
	NULL != (p->img = NULL) || /* false clause for initialization */
	1 != fread (&p->hdr, sizeof (p->hdr), 1, in) ||
	MAX_PHOTO_WIDTH < p->hdr.width ||
	MAX_PHOTO_HEIGHT < p->hdr.height ||
	NULL == (p->img = malloc 
		 (p->hdr.width * p->hdr.height * sizeof (p->img[0])))) {
	if (NULL != p) {
	    if (NULL != p->img) {
	        free (p->img);
	    }
	    free (p);
	}
	if (NULL != in) {
	    (void)fclose (in);
	}
	return NULL;
    }

	uint16_t Pixels[p->hdr.width * p->hdr.height];		// store the pixels
	/* initialize the variable for Octree Algorithm */
	Octree_Node_t Octree_Level4[LEVEL4_NODES];		
	Octree_Node_t Octree_Level2[LEVEL2_NODES];	
	uint32_t Octree_Index;
	for(Octree_Index = 0; Octree_Index < LEVEL4_NODES; Octree_Index++){
		Octree_Level4[Octree_Index].Pattern_Index = Octree_Index;
		Octree_Level4[Octree_Index].Red_Sum = 0;
		Octree_Level4[Octree_Index].Green_Sum = 0;
		Octree_Level4[Octree_Index].Blue_Sum = 0;
		Octree_Level4[Octree_Index].Pixel_Num = 0;
		Octree_Level4[Octree_Index].Removed = 0;
	}
	for(Octree_Index = 0; Octree_Index < LEVEL2_NODES; Octree_Index++){
		Octree_Level2[Octree_Index].Pattern_Index = Octree_Index;
		Octree_Level2[Octree_Index].Red_Sum = 0;
		Octree_Level2[Octree_Index].Green_Sum = 0;
		Octree_Level2[Octree_Index].Blue_Sum = 0;
		Octree_Level2[Octree_Index].Pixel_Num = 0;
	}


	/* 
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order (top to bottom).
     */
	/* first fill the level4 of Octree for this Photo */
    for (y = p->hdr.height; y-- > 0; ) {

		/* Loop over columns from left to right. */
		for (x = 0; p->hdr.width > x; x++) {

	    	/* Try to read one 16-bit pixel.  On failure, clean up and return NULL. */
	    	if (1 != fread (&pixel, sizeof (pixel), 1, in)) {
				free (p->img);
				free (p);
	        	(void)fclose (in);
				return NULL;
	    	}

			/* get the RRRRGGGGBBBB component of this pixel1, which is coded as 5:6:5 RGB*/
			uint32_t pattern_index = (((pixel & 0xF000) >> 4) | ((pixel & 0x0780) >> 3) | ((pixel & 0x001E) >> 1));
			/* get the RGB component and extend to 6 bits if needed */
			Octree_Level4[pattern_index].Red_Sum += (pixel & 0xF100) >> 10;				// 10 comes from extending to 6 bits
			Octree_Level4[pattern_index].Green_Sum += (pixel & 0x07E0) >> 5;			// 5 comes from already 6 bits
			Octree_Level4[pattern_index].Blue_Sum += (pixel & 0x001F) << 1;				// 1 comrs from extending to 6 bits
			Octree_Level4[pattern_index].Pixel_Num += 1;
			Pixels[p->hdr.width * y + x] = pixel;										// store the pixel
		}
    }

	/* copy the octree and sort it */
	Octree_Node_t Octree_Level4_Sorted[LEVEL4_NODES];
	for(Octree_Index = 0; Octree_Index < LEVEL4_NODES; Octree_Index++){
		Octree_Level4_Sorted[Octree_Index].Pattern_Index = Octree_Level4[Octree_Index].Pattern_Index;
		Octree_Level4_Sorted[Octree_Index].Red_Sum = Octree_Level4[Octree_Index].Red_Sum;
		Octree_Level4_Sorted[Octree_Index].Green_Sum = Octree_Level4[Octree_Index].Green_Sum;
		Octree_Level4_Sorted[Octree_Index].Blue_Sum = Octree_Level4[Octree_Index].Blue_Sum;
		Octree_Level4_Sorted[Octree_Index].Pixel_Num = Octree_Level4[Octree_Index].Pixel_Num;
		Octree_Level4_Sorted[Octree_Index].Removed = Octree_Level4[Octree_Index].Removed;
	}
	qsort(Octree_Level4_Sorted, LEVEL4_NODES, sizeof(Octree_Node_t), sort_helper);

	for(Octree_Index = 0; Octree_Index < 128; Octree_Index++){					// 128 comes from 256 - 64(have been used for game object) - 64(level2 nodes) = 128
		/* get the level4 most frequent 128 nodes' RGB average component */
		if(Octree_Level4_Sorted[Octree_Index].Pixel_Num != 0) Octree_Level4_Sorted[Octree_Index].Red_Sum /= Octree_Level4_Sorted[Octree_Index].Pixel_Num;
		if(Octree_Level4_Sorted[Octree_Index].Pixel_Num != 0) Octree_Level4_Sorted[Octree_Index].Green_Sum /= Octree_Level4_Sorted[Octree_Index].Pixel_Num;
		if(Octree_Level4_Sorted[Octree_Index].Pixel_Num != 0) Octree_Level4_Sorted[Octree_Index].Blue_Sum /= Octree_Level4_Sorted[Octree_Index].Pixel_Num;
		/* remove these most 128 frequent nodes RGB in original Octree_Level4, preparing for Level2 */
		Octree_Level4[Octree_Level4_Sorted[Octree_Index].Pattern_Index].Red_Sum = 0;
		Octree_Level4[Octree_Level4_Sorted[Octree_Index].Pattern_Index].Green_Sum = 0;
		Octree_Level4[Octree_Level4_Sorted[Octree_Index].Pattern_Index].Blue_Sum = 0;
		Octree_Level4[Octree_Level4_Sorted[Octree_Index].Pattern_Index].Pixel_Num = 0;
		Octree_Level4[Octree_Level4_Sorted[Octree_Index].Pattern_Index].Removed = 1;	// mark has been removed
	}
	/* get the level2 nodes' RGB total component without level4 nodes' influence */
	for(Octree_Index = 0; Octree_Index < LEVEL4_NODES; Octree_Index++){
		/* get pattern index for level2 nodes, which is RRGGBB */
		uint32_t pattern_index = (((Octree_Level4[Octree_Index].Pattern_Index & 0x0C00) >> 6) | 
								(Octree_Level4[Octree_Index].Pattern_Index & 0x00C0) >> 4 | 
								((Octree_Level4[Octree_Index].Pattern_Index & 0x000C) >> 2));
		Octree_Level2[pattern_index].Red_Sum += Octree_Level4[Octree_Index].Red_Sum;
		Octree_Level2[pattern_index].Green_Sum += Octree_Level4[Octree_Index].Green_Sum;
		Octree_Level2[pattern_index].Blue_Sum += Octree_Level4[Octree_Index].Blue_Sum;
		Octree_Level2[pattern_index].Pixel_Num += Octree_Level4[Octree_Index].Pixel_Num;
	}
	/* get the level2 nodes' RGB average component without level4 nodes' influence */
	for(Octree_Index = 0; Octree_Index < LEVEL2_NODES; Octree_Index++){
		Octree_Level2[Octree_Index].Pattern_Index = Octree_Index;
		if(Octree_Level2[Octree_Index].Pixel_Num != 0) Octree_Level2[Octree_Index].Red_Sum /= Octree_Level2[Octree_Index].Pixel_Num;
		if(Octree_Level2[Octree_Index].Pixel_Num != 0) Octree_Level2[Octree_Index].Green_Sum /= Octree_Level2[Octree_Index].Pixel_Num;
		if(Octree_Level2[Octree_Index].Pixel_Num != 0) Octree_Level2[Octree_Index].Blue_Sum /= Octree_Level2[Octree_Index].Pixel_Num;
	}

	/* fill in the palette with Level2 Nodes */
	for(Octree_Index = 0; Octree_Index < LEVEL2_NODES; Octree_Index++){
		p->palette[Octree_Index][0] = Octree_Level2[Octree_Index].Red_Sum;
		p->palette[Octree_Index][1] = Octree_Level2[Octree_Index].Green_Sum;
		p->palette[Octree_Index][2] = Octree_Level2[Octree_Index].Blue_Sum;
	}
	/* fill in the palette with Level4 Nodes */
	for(Octree_Index = 0; Octree_Index < 128; Octree_Index++){
		p->palette[Octree_Index + 64][0] = Octree_Level4_Sorted[Octree_Index].Red_Sum;
		p->palette[Octree_Index + 64][1] = Octree_Level4_Sorted[Octree_Index].Green_Sum;
		p->palette[Octree_Index + 64][2] = Octree_Level4_Sorted[Octree_Index].Blue_Sum;
	}

	/* map the pixel in photo to the Octree */
	for (y = 0; y < p->hdr.height; y++) {
		for (x = 0; x < p->hdr.width; x++) {
	    	pixel = Pixels[p->hdr.width * y + x];
	    	/* get the RRRRGGGGBBBB component of this pixel1, which is coded as 5:6:5 RGB*/
			uint32_t pattern_index = (((pixel & 0xF000) >> 4) | ((pixel & 0x0780) >> 3) | ((pixel & 0x001E) >> 1));
			/* check if belonging to LEVEL4 or LEVEL2 */
			if(Octree_Level4[pattern_index].Removed == 1){
				/* for level4 node, find its index and store to p->imag */
				for(Octree_Index = 0; Octree_Index < 128; Octree_Index++){
					if(Octree_Level4_Sorted[Octree_Index].Pattern_Index == pattern_index){
						p->img[p->hdr.width * y + x] = Octree_Index + 64 + 64;				// Level4 starts at 128
						break;
					}
				}
			} else {
				/* change the pattern to RRGGBB */
				pattern_index = (((pattern_index & 0x0C00) >> 6) | ((pattern_index & 0x00C0) >> 4) | ((pattern_index & 0x000C) >> 2));
				/* for level2 node, find its index and store to p->imag */
				for(Octree_Index = 0; Octree_Index < LEVEL2_NODES; Octree_Index++){
					if(Octree_Level2[Octree_Index].Pattern_Index == pattern_index){
						p->img[p->hdr.width * y + x] = Octree_Index + 64;					// Level2 starts at 64
						break;
					}
				}
			}
		}
    }

    /* All done.  Return success. */
    (void)fclose (in);
    return p;
}


/* 
 * sort_helper
 *   DESCRIPTION: function that compare two Octree nodes' Pixel_Num field
 *   INPUTS: cmp1 - the first element to be compared
 * 			 cmp2 - the second element to be compared
 *   OUTPUTS: none
 *   RETURN VALUE: The value of cmp1's Pixel_Num - cmp2's Pixel_Num
 */
int
sort_helper(const void* cmp1, const void* cmp2){
	return -(((Octree_Node_t*)cmp1)->Pixel_Num - ((Octree_Node_t*)cmp2)->Pixel_Num);
}