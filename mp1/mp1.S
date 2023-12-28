
# mp1.S - missile-command version
# Good luck, and have fun!

.data
	
    # Constants for accessing the fields of a struct missile, 
    # struct missile is defined in rtc.h and mp1.h

	NEXT 	  = 0
	X	  = 4
	Y	  = 8
	VX	  = 12
	VY	  = 16
	DEST_X	  = 20
	DEST_Y	  = 24
	EXPLODED  = 28
	C	  = 32

    # Character to draw for an explosion - '@'

	EXPLOSION = 64


# Data shared between this file and rtc.c helper functions

# This '.globl' directive makes these symbols visible externally
.globl mp1_missile_list, base_alive, mp1_score

mp1_missile_list:	.long 0x0	# Head of list pointer
base_alive:		.long 0x0	# Array of 3 bytes, plus a padding byte
mp1_score:		.long 0x0	# Player's current score


# Data private to this  file

base_pic:     	    .string "/^^^\\"	# Picture of a live base
dead_base_pic: 	    .string "xxxxx"	# Picture of a dead base
crosshairs_x:		.long 0x0	# X-position of the crosshairs
crosshairs_y:		.long 0x0	# Y-position of the crosshairs
message:			.long 0x0	# message passed to user space
explode_num:		.long 0x0	# count the number that explosion changes game stauts
next_missile:		.long 0x0	# store the next missile

jump_table:
.long mp1_ioctl_startgame, mp1_ioctl_addmissile, mp1_ioctl_movexhairs, mp1_ioctl_getstatus, mp1_ioctl_endgame



.text

# void mp1_poke(void);
# You can use this function to write to video memory.
#
# Interface: Register-based arguments (not C-style)
#    Inputs: %cl  - The byte you wish to write
#            %eax - Offset from the start of video memory that you wish
#                   to write to
#   Outputs: Text-mode video screen is written to at location %eax with
#            the byte in %cl
# Registers: Clobbers EDX

mp1_poke:
		movl    vmem_base_addr(,1),%edx
		movb    %cl,(%edx,%eax,1)
		ret

#   ----------------- Exported functions ---------------------

# void mp1_rtc_tasklet(unsigned long garbage);
# Performs three tasks:
#	(1) updates the list of missiles (implement this in update_missiles,
#           below, and call it from here).
#	(2) Redraw the bases - they may have been overwritten with missiles
#	(3) Redraw the crosshairs - it may have been overwritten with missiles
# Inputs   : none
# Outputs  : none
# Registers: Standard C calling convention

.globl mp1_rtc_tasklet
mp1_rtc_tasklet:
		pushl %ebp
		movl %esp, %ebp

		pushl %ebx
		pushl %esi
		pushl %edi

		subl $12, %esp 				# allocate 3 local variable

		movl mp1_missile_list, %esi	# esi stores pointer to missile

		movl $0, explode_num		# initialize explode_num

for_each_missile:
		/* update missiles' status */
		cmpl $0, %esi				# move to redraw bases if all missile are checked
		je check_status

		movl NEXT(%esi), %eax
		movl %eax, next_missile
		pushl %esi
		call mp1_update_missiles
		addl $4, %esp
		movl next_missile, %esi		# check next missile
		jmp for_each_missile

check_status:
		/* check if explosion changes game status first */
		movl explode_num, %eax
		cmpl $0, %eax
		je redraw_bases
		call mp1_notify_user

redraw_bases:
		/* for each city, use for loop to draw it */
		movl $3, %esi
		/* the citys' starting location are (18,24), (38, 24), (58, 24)) */
		movl $18, -16(%ebp)
		movl base_alive, %edx
		movl %edx, -20(%ebp)
for_each_city:
		cmpl $0, %esi
		je redraw_crosshairs
		movl -20(%ebp), %edx
		movl $0x000000FF, %eax
		andl %edx, %eax
		sarl $8, %edx
		movl %edx, -20(%ebp)
		cmpl $0, %eax
		jne draw_alive_city
		jmp draw_dead_city

main_for_body:
		/* update parameter */
		movl -16(%ebp), %edx
		addl $15, %edx				# next start location = current start location -5 + 20 = current start location + 15
		movl %edx, -16(%ebp)
		subl $1, %esi
		jmp for_each_city

draw_alive_city:
		movl $5, %edi
		leal base_pic, %eax
		movl %eax, -24(%ebp)
		jmp for_each_position

draw_dead_city:
		movl $5, %edi
		leal dead_base_pic, %eax
		movl %eax, -24(%ebp)
		jmp for_each_position

for_each_position:
		cmpl $0, %edi
		je main_for_body
		/* draw city location */
		movl -24(%ebp), %edx
		movl (%edx), %eax
		pushl %eax
		addl $1, %edx
		movl %edx, -24(%ebp)
		
		movl $24, %edx
		pushl %edx

		movl -16(%ebp), %edx
		pushl %edx
		addl $1, %edx
		movl %edx, -16(%ebp)

		call mp1_redraw
		add $12, %esp

		subl $1, %edi
		jmp for_each_position

redraw_crosshairs:
		movl $43 , %edx				# draw crosshairs, whose ASCII is $43
		pushl %edx
		movl crosshairs_y, %edx
		pushl %edx
		movl crosshairs_x, %edx
		pushl %edx
		call mp1_redraw
		addl $12, %esp

mp1_rtc_tasklet_exit:
		addl $12, %esp 				# deallocate 3 local variable

		popl %edi
		popl %esi
		popl %ebx

		leave
		ret

# int mp1_ioctl(unsigned long arg, unsigned int cmd)
# The dispatch function for the MP1 ioctls - should use the cmd argument
# and a jumptable to execute one of the specific ioctls implemented below.
# Inputs   : unsigned long arg - parameter to the mp1_ioctl_....
#          : unsigned int cmd  - specifies which mp1_ioctl_... function 
#          :                     to execute
# Outputs  : Returns an integer - depends on which ioctl() function is called
# Registers: Standard C calling convention

.globl mp1_ioctl
mp1_ioctl:
		/* check cmd value validness */
		movl 8(%esp), %edx
		cmpl $4, %edx
		ja invalid_input
		leal jump_table, %eax
		movl (%eax, %edx, 4), %eax
		jmp *%eax					# not coming back!

invalid_input:
		movl $-1, %eax
		ret


# ----------------- Functions private to this file -------------------

# void mp1_missile_remove(struct missile* target);
# remove the missile from linked list, including:
#	(1) remove the missile from linked list
#	(2) free its memory 
# Inputs   : struct missile* target - the pointer to the target missile that need removed
# Outputs  : none
# Registers: Standard C calling convention

mp1_missile_remove:
		pushl %ebp
		movl %esp, %ebp

		pushl %esi

		/* check if it is the head one */
		movl mp1_missile_list, %edx
		movl 8(%ebp), %eax
		cmpl %eax, %edx
		je head_one

		/* if not head one, find its previous one */
		movl 8(%ebp), %esi
searching:
		movl NEXT(%edx), %eax
		cmpl %eax, %esi
		je has_find
		movl NEXT(%edx), %edx
		jmp searching

has_find:
		movl NEXT(%esi), %eax
		movl %eax, NEXT(%edx)
		pushl %esi
		call mp1_free
		addl $4, %esp
		jmp mp1_missile_remove_exit

head_one:
		movl NEXT(%eax), %edx
		movl %edx, mp1_missile_list	# relink
		pushl %eax					# free its memory
		call mp1_free
		addl $4, %esp

mp1_missile_remove_exit:		
		popl %esi

		leave
		ret

# void mp1_erase(int x_coordinate, int y_coordinate);
# erase the target from the screen, including:
#	(1) replace the char at location (x_coordinate, y_coordinate) with Space, whose ASCII is $32
# Inputs   : int x_coordinate - the x coordinate of target
#		   : int y_coordinate - the y coordinate of target
# Outputs  : none
# Registers: Standard C calling convention

mp1_erase:
		pushl %ebp
		movl %esp, %ebp

		movl 12(%ebp), %eax
		movl $0, %edx				# compute its position, that is (column * 2 + row * 80 * 2)
		IMULL $80, %eax, %eax
		IMULL $2, %eax, %eax
		addl %eax, %edx
		movl 8(%ebp), %eax
		IMULL $2, %eax, %eax
		addl %eax, %edx
		movl %edx, %eax
		movb $32, %cl				# draw Space
		pushl %edx					# caller-save register: edx
		call mp1_poke
		pop %edx

		leave
		ret

# void mp1_redraw(int x_coordinate, int y_coordinate, char C);
# redraw the target, including:
#	(1) redraw the target at location (x_coordinate, y_coordinate) with char C
# Inputs   : int x_coordinate - the x coordinate of target
#		   : int y_coordinate - the y coordinate of target
#		   : char C - the character to draw
# Outputs  : none
# Registers: Standard C calling convention

mp1_redraw:
		pushl %ebp
		movl %esp, %ebp

		movl 12(%ebp), %eax
		movl $0, %edx				# compute its position, that is (column * 2 + row * 80 * 2)
		IMULL $80, %eax, %eax
		IMULL $2, %eax, %eax
		addl %eax, %edx
		movl 8(%ebp), %eax
		IMULL $2, %eax, %eax
		addl %eax, %edx
		movl %edx, %eax
		movb 16(%ebp), %cl			# draw the character
		pushl %edx					# caller-save register: edx
		call mp1_poke
		pop %edx

		leave
		ret

# void update_missiles(struct missile* head);
# Updates the list of missiles, including:
#	(1) erase missile and free memory if it moves out of the screen
#	(2) check whether the missile reaches its destination or currently exploding and deal with it
#	(3) check whether its screen position has changed and if so, erase it and re-draw it in new position
# Inputs   : struct missile* head - pointer to the head of the linked list consist of struct missile
# Outputs  : none
# Registers: Standard C calling convention

mp1_update_missiles:
		pushl %ebp
		movl %esp, %ebp
		
		pushl %ebx
		pushl %esi
		pushl %edi

		subl $8, %esp				# allocate 2 local variable

		movl 8(%ebp), %esi

		/* check if exploding, if not, store old position and update its location */
		cmpl $0, EXPLODED(%esi)
		jg handle_explosion

		movl X(%esi), %edx
		movl %edx, -16(%ebp)
		movl Y(%esi), %edx
		movl %edx, -20(%ebp)

		movl X(%esi), %edx
		movl VX(%esi), %eax
		addl %eax, %edx
		movl %edx, X(%esi)
		movl Y(%esi), %edx
		movl VY(%esi), %eax
		addl %eax, %edx
		movl %edx, Y(%esi)

		/* check if out of boundary */
		movl X(%esi), %ebx			# test if x coordinate out of boundary ([0, 79])
		sarl $16, %ebx
		cmpl $79, %ebx
		jg out_boundary
		cmpl $0, %ebx
		jl out_boundary
		movl Y(%esi), %ebx			# test if y coordinate out of boundary ([0, 24])
		sarl $16, %ebx
		cmpl $24, %ebx
		jg out_boundary
		cmpl $0, %ebx
		jl out_boundary
		jmp check_needed_explosion

out_boundary:
		/* erase it from screen at old location */
		movl -20(%ebp), %edx
		sarl $16, %edx
		pushl %edx
		movl -16(%ebp), %edx
		sarl $16, %edx
		pushl %edx
		call mp1_erase
		addl $8, %esp

		/* remove the missile from linked list */
		pushl %esi
		call mp1_missile_remove
		addl $4, %esp
		jmp mp1_update_missiles_exit

check_needed_explosion:
		/* check if need to explode or being exploding */
		movl X(%esi), %ebx
		sarl $16, %ebx
		movl DEST_X(%esi), %edx
		cmpl %ebx, %edx
		jne check_position
		movl Y(%esi), %ebx
		sarl $16, %ebx
		movl DEST_Y(%esi), %edx
		cmpl %ebx, %edx
		jne check_position
		/* if going to explode, erase it from screen at old location */
		movl -20(%ebp), %edx
		sarl $16, %edx
		pushl %edx
		movl -16(%ebp), %edx
		sarl $16, %edx
		pushl %edx
		call mp1_erase
		addl $8, %esp

handle_explosion:
		/* call missile_explode to influence the level */
		pushl %esi
		call missile_explode
		add $4, %esp

		/* notify the user-space program if return non-zero value */
		cmpl $0, %eax
		je no_need_notify
		movl explode_num, %eax
		addl $1, %eax
		movl %eax, explode_num

no_need_notify:
		/* decreament missile's exploded field and deal with it */
		decl EXPLODED(%esi)
		cmpl $0, EXPLODED(%esi)
		je finish_exploding
		/* if not finished, draw the exposion */
		pushl $EXPLOSION
		movl Y(%esi), %edx
		sarl $16, %edx
		pushl %edx
		movl X(%esi), %edx
		sarl $16, %edx
		pushl %edx
		call mp1_redraw
		add $12, %esp
		jmp mp1_update_missiles_exit

finish_exploding:
		/* erase it from screen at old location */
		movl Y(%esi), %edx
		sarl $16, %edx
		pushl %edx
		movl X(%esi), %edx
		sarl $16, %edx
		pushl %edx
		call mp1_erase
		addl $8, %esp

		/* remove the missile from linked list */
		pushl %esi
		call mp1_missile_remove
		addl $4, %esp
		jmp mp1_update_missiles_exit

check_position:
		/* compare position with old one */
		movl -16(%ebp), %edx			# compare missile's X coordinate
		shrl $16, %edx
		movl X(%esi), %eax
		shrl $16, %eax
		cmpl %eax, %edx
		jne need_erase_old

		movl -20(%ebp), %edx			# compare missile's Y coordinate
		shrl $16, %edx
		movl Y(%esi), %eax
		shrl $16, %eax
		cmpl %eax, %edx
		jne need_erase_old
		jmp redraw_missile

need_erase_old:
		/* erase it from screen at old location */
		movl -20(%ebp), %edx
		sarl $16, %edx
		pushl %edx
		movl -16(%ebp), %edx
		sarl $16, %edx
		pushl %edx
		call mp1_erase
		addl $8, %esp

redraw_missile:
		/* redraw the missile */
		movl C(%esi), %edx
		pushl %edx
		movl Y(%esi), %edx
		sarl $16, %edx
		pushl %edx
		movl X(%esi), %edx
		sarl $16, %edx
		pushl %edx
		call mp1_redraw
		add $12, %esp

mp1_update_missiles_exit:
		addl $8, %esp				# deallocate 2 local variable

		popl %edi
		popl %esi
		popl %ebx

		leave
		ret

# int mp1_ioctl_startgame(unsigned long ignore);
# Performs twp tasks:
#	(1) initialize all variables declared in mp1.S
#	(2) set crosshairs to the middle of the screen: (40, 12)
# Inputs   : unsigned long ignore - meaningless and should be ignored
# Outputs  : return 0 as default
# Registers: Standard C calling convention

mp1_ioctl_startgame:
		pushl %ebp
		movl %esp, %ebp

		pushl %ebx
		pushl %edi
		pushl %esi
		/* set crosshairs to middle of the screen: (40, 12) */
		movl $40, crosshairs_x
		movl $12, crosshairs_y
		/* initialize other variable to 0 */
		movl $0, mp1_missile_list
		movl $0xFFFFFFFF, base_alive	# initialize base_alive to 0xFFFFFFFF as all city are alive
		movl $0, mp1_score
		movl $0, message
		movl $0, next_missile

		movl $0, %eax
		popl %esi
		popl %edi
		popl %ebx
		leave
		ret

# int mp1_ioctl_addmissile(struct missile* user_missile);
# Performs three tasks:
#	(1) allocate dynamic memory
#	(2) copy the memory from user space to kernel
#	(3) if previoius steps success, add the struct missile to linked list
# Inputs   : struct missile* user_missile - a pointer to a struct missile in user space
# Outputs  : return 0 if  allocate memory successfully and -1 otherwise
# Registers: Standard C calling convention
mp1_ioctl_addmissile:
		pushl %ebp
		movl %esp, %ebp

		pushl %ebx
		pushl %edi
		pushl %esi

		/* allocate memory for new struct missile */
		movl $36, %edx
		pushl %edx
		call mp1_malloc
		add $4, %esp
		cmpl $0, %eax
		je mp1_ioctl_addmissile_fail
		movl %eax, %esi

		/* copy data from user space to kernel space */
		movl $36, %eax
		pushl %eax
		movl 8(%ebp), %eax
		pushl %eax
		pushl %esi
		call mp1_copy_from_user
		addl $12, %esp
		cmpl $0, %eax
		je add_to_linked_list
		/* if not success, free the allocated memory */
		pushl %esi
		call mp1_free
		addl $4, %esp
		jmp mp1_ioctl_addmissile_fail

add_to_linked_list:
		movl mp1_missile_list, %eax
		movl %eax, NEXT(%esi)
		movl %esi, mp1_missile_list
		jmp mp1_ioctl_addmissile_success

mp1_ioctl_addmissile_fail:
		movl $-1, %eax
		popl %esi
		leave
		ret

mp1_ioctl_addmissile_success:
		movl $0, %eax
		popl %esi
		popl %edi
		popl %ebx
		leave
		ret

# int mp1_ioctl_movexhairs(unsigned long xhair_delta_packed);
# Performs two tasks:
#	(1) do not change if try to go beyound boundary
#	(2) erase old one and redraw it at new location if position does change
# Inputs   : unsigned long xhair_delta_packed - a 32-bit integer where low 16 bits contain x position and high 16 bits contain y position
# Outputs  : return 0
# Registers: Standard C calling convention
mp1_ioctl_movexhairs:
		pushl %ebp
		movl %esp, %ebp

		pushl %ebx
		pushl %esi
		pushl %edi

		subl $16, %esp				# allocate 4 local memory address

		/* store old position */
		movl crosshairs_x, %edx
		movl %edx, -16(%ebp)
		movl crosshairs_y, %edx
		movl %edx, -20(%ebp)
		/* compute new location and store them */
		movl 8(%ebp), %edx
		movl $0x0000FFFF, %eax
		andl %edx, %eax
		/* check if negative */
		movl $0x00008000, %esi
		andl %eax, %esi
		cmpl $0, %esi
		je prepare_to_add_offset
		movl $0xFFFF0000, %esi
		orl %esi, %eax
prepare_to_add_offset:
		movl -16(%ebp), %ebx		# add offet to old X
		addl %eax, %ebx
		movl %ebx, -24(%ebp)
		sarl $16, %edx
		movl $0x0000FFFF, %eax
		andl %edx, %eax
		movl $0x00008000, %esi
		andl %eax, %esi
		cmpl $0, %esi
		je prepare_to_add_offset_y
		movl $0xFFFF0000, %esi
		orl %esi, %eax
prepare_to_add_offset_y:
		movl -20(%ebp), %ebx		# add offset to old y
		addl %eax, %ebx
		movl %ebx, -28(%ebp)

		/* check if new position out of boundary */
		movl -24(%ebp), %edx
		cmpl $0, %edx
		jl x_out_boundary
		cmpl $79, %edx
		jg x_out_boundary
check_y_coordinate:
		movl -28(%ebp), %edx
		cmpl $0, %edx
		jl y_out_boundary
		cmpl $24, %edx
		jg y_out_boundary

compare_old_new_location:
		/* upload new location */
		movl -24(%ebp), %edx
		movl %edx, crosshairs_x
		movl -28(%ebp), %edx
		movl %edx, crosshairs_y
		/* compare location to decide whether redraw */
		movl -16(%ebp), %edx
		movl -24(%ebp), %eax
		cmpl %edx, %eax
		jne draw_new_location_crosshairs
		movl -20(%ebp), %edx
		movl -28(%ebp), %eax
		cmpl %edx, %eax
		jne draw_new_location_crosshairs
		jmp mp1_ioctl_movexhairs_exit	# exit as position not change

x_out_boundary:
		movl -16(%ebp), %edx
		movl %edx, -24(%ebp)		# store x position as old one if out of boundary
		jmp check_y_coordinate

y_out_boundary:
		movl -20(%ebp), %edx
		movl %edx, -28(%ebp)		# store y position as old one if out of boundary
		jmp compare_old_new_location

draw_new_location_crosshairs:
		/* erase old one */
		movl -20(%ebp), %eax
		pushl %eax
		movl -16(%ebp), %eax
		pushl %eax
		call mp1_erase
		addl $8, %esp

		/* draw the new missile */
		movl $43, %edx				# ASCII for crosshair is $43
		pushl %edx
		movl -28(%ebp), %edx
		pushl %edx
		movl -24(%ebp), %edx
		pushl %edx
		call mp1_redraw
		addl $12, %esp

mp1_ioctl_movexhairs_exit:
		movl $0, %eax	
		addl $16, %esp				# deallocate 4 local memory address

		popl %edi
		popl %esi
		popl %ebx
		leave
		ret

# int mp1_ioctl_getstatus(unsigned long* user_status);
# Performs two tasks:
#	(1) form a message where 16,17,18 bits indicate citys' alive and low 16 bits indicate mp1_score low 16 bits
#	(2) copy the message to user_status
# Inputs   : unsigned long* user_status - a pointer to 32-bit integer in user space
# Outputs  : return 0 if copy success and -1 otherwie
# Registers: Standard C calling convention
mp1_ioctl_getstatus:
		pushl %ebp
		movl %esp, %ebp

		pushl %ebx
		pushl %edi
		pushl %esi

		/* initialize message */
		movl $0, %esi
		movl base_alive, %edx
		movl $0x000000FF, %eax
		andl %edx, %eax
		cmpl $0, %eax
		je second_city
		addl $1, %esi
second_city:
		shll $1, %esi
		sarl $8, %edx
		movl $0x000000FF, %eax
		andl %edx, %eax
		cmpl $0, %eax
		je third_city
		addl $1, %esi
third_city:
		shll $1, %esi
		sarl $8, %edx
		movl $0x000000FF, %eax
		andl %edx, %eax
		cmpl $0, %eax
		je copy_message
		addl $1, %esi
copy_message:
		shll $16, %esi
		movl $0x0000FFFF, %eax
		movl mp1_score, %edx
		andl %edx, %eax
		orl %eax, %esi
		movl %esi, message

		/* copy message to user space */
		movl $4, %edx
		pushl %edx
		leal message, %edx
		pushl %edx
		movl 8(%ebp), %edx
		pushl %edx
		call mp1_copy_to_user
		addl $12, %esp

		cmpl $0, %eax
		jne mp1_ioctl_getstatus_fail
		jmp mp1_ioctl_getstatus_success

mp1_ioctl_getstatus_fail:
		movl $-1, %eax
		popl %esi
		leave
		ret

mp1_ioctl_getstatus_success:
		movl $0, %eax
		popl %esi
		popl %edi
		popl %ebx
		leave
		ret

# int mp1_ioctl_endgame(unsigned long ignore);
# Performs one tasks:
#	(1) free the linked list
# Inputs   : unsigned long ignore - useless
# Outputs  : return 0
# Registers: Standard C calling convention
mp1_ioctl_endgame:
		pushl %ebp
		movl %esp, %ebp

		pushl %ebx
		pushl %edi
		pushl %esi

		movl mp1_missile_list, %esi
for_each_struct:
		cmpl $0, %esi
		je mp1_ioctl_endgame_exit
		movl NEXT(%esi), %eax
		movl %eax, mp1_missile_list
		pushl %esi
		call mp1_free
		add $4, %esp
		movl mp1_missile_list, %esi
		jmp for_each_struct

mp1_ioctl_endgame_exit:
		movl $0, %eax
		popl %esi
		popl %edi
		popl %ebx
		leave
		ret
