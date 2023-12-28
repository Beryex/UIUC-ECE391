# Bug Log

## Bug 1: The bar shown at the middle of the sreen

- Type: Algorithmic Errors

- Symptom: Bar shown at the middle of the screen

- Difficulty to Find: Take about half hour to find the reason

- Difficulty to Fix: Change two line of code

- What Cause the Bug: For the value stored in CRTC register to set mode X. I put the wrong value 200 -18 = 182 in LineCompare Register, which should 2 * (200 - 18) = 364 instead as the VGA dimension is 640 * 400 using double scan.

- How to Fix it: Change the value stored in LineCompare Register from 182 to 364 can fix it.

## Bug 2: The bar also shown at the top and flashing

- Type: Algorighmic Errors

- Symptom: Bar also shown at the top of the screen

- Difficulty to Find: Take about half hour to find the reason

- Difficulty to Fix: Change one line of code

- What Cause the Bug: The video memory to store the bar and the image are at the same storage area starting at mem_image + 0x0000, so that the bar also shows at the top and flashing

- How to Fix it: Change the value of target_img in function set_mode_X in modex.c to
  
  ```
  target_img = 0x0800
  ```
  
  , whose value need to be larger than the address size of the bar.

## Bug 3: The bar is always black

- Type: Semantic Errors

- Symptom: Bar is always black with no text showing on

- Difficulty to Find: Take about 3 hours

- Difficulty to Fix: Change three lines of code

- What Cause the Bug: The wrong way to make comment for inline code for the function copy_bar in modex.c.

- How to Fix it: Change the function copy_bar from
  
  ```c
  static void
  copy_bar (unsigned char* bar, unsigned short scr_addr)
  {
      /* 
       * memcpy is actually probably good enough here, and is usually
       * implemented using ISA-specific features like those below,
       * but the code here provides an example of x86 string moves
       */
      asm volatile (
          "cld                                                 ;"
             "movl $1440,%%ecx  # copy BAR_IMAGE_X_DIM * BAR_IMAGE_Y_DIM / 4 block    ;"
             "rep movsb    # copy ECX bytes from M[ESI] to M[EDI]  "
        : /* no outputs */
        : "S" (bar), "D" (mem_image + scr_addr) 
        : "eax", "ecx", "memory"
      );
  }
  ```
  
  to
  
  ```
  static void
  copy_bar (unsigned char* bar, unsigned short scr_addr)
  {
      /* 
       * memcpy is actually probably good enough here, and is usually
       * implemented using ISA-specific features like those below,
       * but the code here provides an example of x86 string moves
       */
      asm volatile (
          "cld                                                 ;"
          /* copy BAR_IMAGE_X_DIM * BAR_IMAGE_Y_DIM / 4 blocks */
             "movl $1440,%%ecx                                    ;"
             "rep movsb    # copy ECX bytes from M[ESI] to M[EDI]  "
        : /* no outputs */
        : "S" (bar), "D" (mem_image + scr_addr) 
        : "eax", "ecx", "memory"
      );
  }
  ```

## Bug 4: The color of image is chaos

- Type: Algorighmic Errors

- Symptom: The color of image is chaos

- Difficulty to Find: Take about 20 minuts to find the reason

- Difficulty to Fix: Change two line of code

- What Cause the Bug: The palette index for the level2 nodes takes the same index to the indexs already used for game objects

- How to Fix it: reassign the palette index for each pixel's data, change from
  
  ```c
  p->img[p->hdr.width * y + x] = Octree_Index + 64; // for Level4 nodes
  p->img[p->hdr.width * y + x] = Octree_Index;      // for Level2 nodes
  ```
  
  to
  
  ```c
  p->img[p->hdr.width * y + x] = Octree_Index + 64 + 64; // Level4 nodes
  p->img[p->hdr.width * y + x] = Octree_Index + 64;      // Level2 nodes
  ```

## Bug 5: The color of image is too dark

- Type: Algorighmic Errors

- Symptom: The color of image is too dark

- Difficulty to Find: Take about 20 minuts to find the reason

- Difficulty to Fix: Change three line of code

- What Cause the Bug: To caculate the average component of RGB, we need to first transfer the RGB: 5:6:5 bits to 6:6:6, but I forget it and directly calculate the average of 4:4:4, which is Level4 nodes pattern index.

- How to Fix it: recalculate the RGB sum, change from
  
  ```c
  /* get the RRRRGGGGBBBB component of this pixel1, which is coded as 5:6:5 RGB*/
  uint32_t pattern_index = (((pixel & 0xF000) >> 4) | ((pixel & 0x0780) >> 3) | ((pixel & 0x001E) >> 1))
  /* get the RGB component and extend to 6 bits if needed */
  Octree_Level4[pattern_index].Red_Sum += (pattern_index & 0x0F00) >> 8;    
  Octree_Level4[pattern_index].Green_Sum += (pattern_index & 0x00F0) >> 4;
  Octree_Level4[pattern_index].Blue_Sum += (pixel & 0x000F);
  ```
  
  to
  
  ```c
  /* get the RRRRGGGGBBBB component of this pixel1, which is coded as 5:6:5 RGB*/
  uint32_t pattern_index = (((pixel & 0xF000) >> 4) | ((pixel & 0x0780) >> 3) | ((pixel & 0x001E) >> 1))
  /* get the RGB component and extend to 6 bits if needed */
  Octree_Level4[pattern_index].Red_Sum += (pixel & 0xF100) >> 10;    
  Octree_Level4[pattern_index].Green_Sum += (pixel & 0x07E0) >> 5;
  Octree_Level4[pattern_index].Blue_Sum += (pixel & 0x001F) << 1;    
  ```

## Bug 6: When pressing the A, B, C button and not release it, it will continuously trigger command.

- Type: Algorighmic Errors

- Symptom: When pressing the A, B, C button and not release it, it will continuously trigger command.

- Difficulty to Find: Take about 30 mins to find the reason

- Difficulty to Fix: Complete redisign

- What Cause the Bug: The input from Tux for A,B,C buttons can continuously trigger.

- How to Fix it: Change get_Tux_Command() to:
  
  ```c
  cmd_t
  get_Tux_command(){
      /* pull driver */
      int buttons;
      cmd_t return_cmd;
      ioctl(fd, TUX_BUTTONS, &buttons);
      /* transfer its low byte it to command */
      buttons = buttons & 0x000000FF;
      switch(buttons){
          case 0x7F:
          return_cmd = CMD_RIGHT;
          break;
          case 0xBF:
          return_cmd = CMD_LEFT;
          break;
          case 0xDF:
          return_cmd = CMD_DOWN;
          break;
          case 0xEF:
          return_cmd = CMD_UP;
          break;
          case 0xF7:
          return_cmd = CMD_MOVE_RIGHT;
          break;
          case 0xFB:
          return_cmd = CMD_ENTER;
          break;
          case 0xFD:
          return_cmd = CMD_MOVE_LEFT;
          break;
          default:
          return_cmd = CMD_NONE;
          break;
      }
      /* CMD_MOVE_RIGHT, CMD_ENTER and CMD_MOVE_LEFT only triger once */
      if(prev_cmd == return_cmd && ((prev_cmd == CMD_MOVE_RIGHT) | (prev_cmd == CMD_ENTER) | (prev_cmd == CMD_MOVE_LEFT))){
          return CMD_NONE;
      }
      prev_cmd = return_cmd;
      return return_cmd;
  }
  ```

# 
