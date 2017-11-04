
.data					# section declaration

        # Useful offset constants for accessing members of a 
        # struct mp1_blink_struct structure
        LOCATION   = 0    
        ON_CHAR    = 2
        OFF_CHAR   = 3 
        ON_LENGTH  = 4
        OFF_LENGTH = 6
        COUNTDOWN  = 8
        STATUS     = 10
        NEXT       = 12

        STRUCT_SIZE = 16

# Pointer to head of list (initialized to NULL)
mp1_list_head:
        .long   0

.text					# section declaration

# Export the function symbol names

.global mp1_rtc_tasklet
.global mp1_ioctl

# void mp1_poke(void);
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


# void mp1 rtc tasklet (unsigned long);
# Inputs: None
# Outputs: None
# Side effect: Traverse the linked list, decrement countdown of each struct, and flip status if necessary
# Registers: Clobbers ecx, edx, eax
mp1_rtc_tasklet:
	pushl	%ebp
	movl	%esp,%ebp
	pushl 	%ebx
	subl	$4,%esp # pointer to current object
	movl 	mp1_list_head,%ebx
	movl 	%ebx,(%esp)

mp1_rtc_tasklet_loop:
	movl 	(%esp),%ebx
	cmpl	$0,%ebx
	je 		mp1_rtc_tasklet_return
	subw 	$1,COUNTDOWN(%ebx)
	cmpw	$0,COUNTDOWN(%ebx)
	ja		mp1_rtc_tasklet_next
	cmpw	$1,STATUS(%ebx)
	jne 	mp1_rtc_tasklet_off
	movb	OFF_CHAR(%ebx),%cl # on char, switch to off char
	movzwl	LOCATION(%ebx),%eax
	shll	$1,%eax
	call	mp1_poke
	movw 	OFF_LENGTH(%ebx),%cx
	movw 	%cx,COUNTDOWN(%ebx)
	movw 	$0x0,STATUS(%ebx)
	jmp		mp1_rtc_tasklet_next

# off char, swirch to on char
mp1_rtc_tasklet_off:
	movb	ON_CHAR(%ebx),%cl
	movzwl	LOCATION(%ebx),%eax
	shll	$1,%eax
	call	mp1_poke
	movw 	ON_LENGTH(%ebx),%cx
	movw 	%cx,COUNTDOWN(%ebx)
	movw 	$0x1,STATUS(%ebx)
	jmp 	mp1_rtc_tasklet_next

# current = current -> next
mp1_rtc_tasklet_next:
	movl 	(%esp),%ebx
	movl 	NEXT(%ebx),%ecx
	movl 	%ecx,(%esp)
	jmp 	mp1_rtc_tasklet_loop

mp1_rtc_tasklet_return:
	movl 	$0,%eax
	popl	%ebx
	leave
	ret



mp1_ioctl:
	movl	8(%esp),%edx
	cmpl	$0,%edx
	jb		mp1_ioctl_err
	cmpl	$3,%edx
	ja		mp1_ioctl_err
	jmp     *mp1_ioctl_table(,%edx,4)

mp1_ioctl_err:
	movl	$-1,%eax
	ret

mp1_ioctl_table:
	.long	mp1_ioctl_add, mp1_ioctl_remove, mp1_ioctl_find, mp1_ioctl_sync


# int mp1 ioctl add(unsigned long arg);
# Inputs: Pointer to a mp1 blink struct
# Outputs: -1 if error occurs, 0 otherwise
# Side effect: Create new mp1 blink sturct, and copy user provided struct to new struct, set the new strut to head of linked list
# Registers: Clobbers ecx, edx, eax
mp1_ioctl_add:
	pushl	%ebp
	movl	%esp,%ebp
	pushl 	%ebx
	subl	$4,%esp # pointer to new object

# malloc
	pushl	$STRUCT_SIZE
	call	mp1_malloc
	addl	$4,%esp
	movl	%eax,(%esp)

	cmpl	$0,%eax # check NULL
	je		mp1_ioctl_add_err
# malloc done

# check NULL
	movl	8(%ebp),%eax
	cmpl	$0,%eax
	je 		mp1_ioctl_add_err

# memcpy
	pushl	$STRUCT_SIZE
	pushl	8(%ebp)
	pushl	8(%esp)
	call	mp1_copy_from_user
	addl	$12,%esp
# memcpy done

# check memcpy error
	cmpl	$0,%eax
	jne		mp1_ioctl_add_err

# check location range
	movl 	(%esp),%eax
	cmpw	$80*25-1,LOCATION(%eax)
	ja 		mp1_ioctl_add_err

	movw	ON_LENGTH(%eax),%cx
	movw 	%cx,COUNTDOWN(%eax) # set countdown to on_length
	movw	$0x1,STATUS(%eax) # set status to on

# insert at front
	movl	mp1_list_head,%ecx
	movl 	%ecx,NEXT(%eax)
	movl 	%eax,mp1_list_head

# poke
	movb	ON_CHAR(%eax),%cl 
	movw	LOCATION(%eax),%bx
	movzwl	%bx,%eax
	shll	$1,%eax
	call	mp1_poke

# add done
	movl	$0,%eax
	jmp		mp1_ioctl_add_return

mp1_ioctl_add_err:
	pushl	(%esp)
	call	mp1_free
	addl	$4,%esp
	movl	$-1,%eax
	jmp		mp1_ioctl_add_return

mp1_ioctl_add_return:
	popl	%ebx
	leave
	ret
    

# int mp1 ioctl remove(unsigned long arg);
# Inputs: Loocation of struct to be removed
# Outputs: -1 if error occurs, 0 otherwise
# Side effect: Call search to locate the struct to be removed and delete it from linked list, return -1 if not found
# Registers: Clobbers ecx, edx, eax
mp1_ioctl_remove:
	pushl	%ebp
	movl	%esp,%ebp
	pushl 	%ebx

	subl	$4,%esp
	movl 	8(%ebp),%ebx # arg is a memeber variable location, not a pointer

# call search
	pushw 	%bx
	call	mp1_ioctl_search
	addl	$2,%esp

	cmpl	$0,%eax # check NULL
	je 		mp1_ioctl_remove_err

	cmpl	%eax,%edx # check head pointer
	jne 	mp1_ioctl_remove_point_to_next
	movl 	NEXT(%edx),%ecx # search returns mp1_list_head
	movl 	%ecx,mp1_list_head # set mp1_list_head
	jmp 	mp1_ioctl_remove_free

# previous -> next = current -> next
mp1_ioctl_remove_point_to_next:
	movl 	NEXT(%eax),%ecx
	movl 	%ecx,NEXT(%edx)

# free
mp1_ioctl_remove_free:
	pushl 	%eax
	call 	mp1_free
	addl 	$4,%esp
	movl 	$0,%eax
	jmp 	mp1_ioctl_remove_return

mp1_ioctl_remove_err:
	movl 	$-1,%eax

mp1_ioctl_remove_return:
	popl	%ebx
	leave
	ret


# int mp1 ioctl find(unsigned long arg);
# Inputs: Pointer to a mp1 blink struct
# Outputs: -1 if error occurs, 0 otherwise
# Side effect: Call search to locate the struct to be found and copy the struct to user provided struct
# Registers: Clobbers ecx, edx, eax
mp1_ioctl_find:
	pushl	%ebp
	movl	%esp,%ebp
	pushl 	%ebx

	subl 	$4,%esp
	movl 	mp1_list_head,%ebx
	cmpl	$0,%ebx # check NULL
	je 		mp1_ioctl_find_err
	movl 	8(%ebp),%eax # check NULL
	cmpl	$0,%eax
	je 		mp1_ioctl_find_err

# call search
	pushw	LOCATION(%eax)
	call	mp1_ioctl_search
	addl	$2,%esp
	cmpl	$0,%eax # check NULL
	je 		mp1_ioctl_find_err

# memcpy
	pushl 	$STRUCT_SIZE
	pushl	%eax
	pushl	8(%ebp)
	call	mp1_copy_to_user
	addl 	$12,%esp
	movl 	$0,%eax
	jmp		mp1_ioctl_find_return

mp1_ioctl_find_err:
	movl 	$-1,%eax

mp1_ioctl_find_return:
	popl	%ebx
	leave
	ret


# int mp1 ioctl sync(unsigned long arg);
# Inputs: Two unsigned short, fist location at upper 16 bits, second location at lower 16 bits
# Outputs: -1 if error occurs, 0 otherwise
# Side effect: Call search with first location and second location, copy time information from first struct to second struct, and poke
# Registers: Clobbers ecx, edx, eax
mp1_ioctl_sync:
	pushl	%ebp
	movl	%esp,%ebp
	pushl 	%ebx

	subl 	$4,%esp # first pointer
	movw 	10(%ebp),%bx # upper 16 bits
	pushw 	%bx
	call	mp1_ioctl_search
	addl	$2,%esp
	cmpl	$0,%eax
	je 		mp1_ioctl_sync_err
	movl 	%eax,(%esp)

	subl 	$4,%esp # first pointer
	movw 	8(%ebp),%bx # lower 16 bits
	pushw 	%bx
	call	mp1_ioctl_search
	addl	$2,%esp
	cmpl	$0,%eax
	je 		mp1_ioctl_sync_err
	movl 	%eax,(%esp)

	movl 	(%esp),%eax
	movl 	4(%esp),%ecx

# copy on_length
	movw 	ON_LENGTH(%ecx),%bx
	movw 	%bx,ON_LENGTH(%eax)

# copy off_length
	movw 	OFF_LENGTH(%ecx),%bx
	movw 	%bx,OFF_LENGTH(%eax)

# copy count_down
	movw 	COUNTDOWN(%ecx),%bx
	movw 	%bx,COUNTDOWN(%eax)

# copy status
	movw 	STATUS(%ecx),%bx
	movw 	%bx,STATUS(%eax)

	cmpw	$0,%bx
	je 		mp1_ioctl_sync_off

	movb	ON_CHAR(%eax),%cl
	jmp 	mp1_ioctl_sync_poke
	
mp1_ioctl_sync_off:
	movb	OFF_CHAR(%eax),%cl

# poke
mp1_ioctl_sync_poke:
	movw	LOCATION(%eax),%bx
	movzwl	%bx,%eax
	shll	$1,%eax
	call	mp1_poke
	movl 	$0,%eax
	jmp 	mp1_ioctl_sync_return

mp1_ioctl_sync_err:
	movl 	$-1,%eax

mp1_ioctl_sync_return:
	popl	%ebx
	leave
	ret


# int mp1 ioctl find(unsigned long arg);
# Inputs: 16 bit location of the struct to search for
# Outputs: Search linked list and return current point in eax, previous pointer in edx, -1 in eax and edx if not found or NULL
# Registers: Clobbers ecx, edx, eax
mp1_ioctl_search:
	pushl	%ebp
	movl	%esp,%ebp
	pushl 	%ebx

	subl 	$4,%esp
	movl 	mp1_list_head,%ebx # current pointer
	movl 	%ebx,%edx
	movl 	%ebx,(%esp)
	cmpl	$0,(%esp) # check NULL
	je 		mp1_ioctl_search_err
	movw 	8(%ebp),%ax

	movw 	LOCATION(%ebx),%cx
	cmpw	%cx,%ax
	je 		mp1_ioctl_search_success

mp1_ioctl_search_loop:
	movl 	(%esp),%ebx
	cmpl 	$0,%ebx
	je 		mp1_ioctl_search_err
	movw 	LOCATION(%ebx),%cx
	cmpw	%cx,%ax
	je 		mp1_ioctl_search_success
	movl 	%ebx,%edx # save previous pointer in edx
	movl 	NEXT(%ebx),%ebx
	movl 	%ebx,(%esp)
	jmp 	mp1_ioctl_search_loop

mp1_ioctl_search_err:
	movl 	$0,%eax
	movl 	$0,%edx
	jmp 	mp1_ioctl_search_return

mp1_ioctl_search_success:
	movl 	%ebx,%eax
	jmp		mp1_ioctl_search_return

mp1_ioctl_search_return:
	popl	%ebx
	leave
	ret


.end

	
