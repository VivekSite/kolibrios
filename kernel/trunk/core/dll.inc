;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;                                                              ;;
;; Copyright (C) KolibriOS team 2004-2022. All rights reserved. ;;
;;  Distributed under terms of the GNU General Public License.  ;;
;;                                                              ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

$Revision$


DRV_COMPAT   =  5  ;minimal required drivers version
DRV_CURRENT  =  6  ;current drivers model version

DRV_VERSION = (DRV_COMPAT shl 16) or DRV_CURRENT
PID_KERNEL  = 1    ;os_idle thread



align 4
proc get_notify stdcall, p_ev:dword

.wait:
        mov     ebx, [current_slot]
        test    dword [ebx + APPDATA.event_mask], EVENT_NOTIFY
        jz      @f
        and     dword [ebx + APPDATA.event_mask], not EVENT_NOTIFY
        mov     edi, [p_ev]
        mov     dword [edi], EV_INTR
        mov     eax, [ebx + APPDATA.event]
        mov     dword [edi+4], eax
        ret
@@:
        call    change_task
        jmp     .wait
endp



handle     equ  IOCTL.handle
io_code    equ  IOCTL.io_code
input      equ  IOCTL.input
inp_size   equ  IOCTL.inp_size
output     equ  IOCTL.output
out_size   equ  IOCTL.out_size


align 4
proc srv_handler stdcall, ioctl:dword
        mov     esi, [ioctl]
        test    esi, esi
        jz      .err

        mov     edi, [esi + handle]
        cmp     [edi + SRV.magic], ' SRV'
        jne     .fail

        cmp     [edi + SRV.size], sizeof.SRV
        jne     .fail

;        stdcall [edi+SRV.srv_proc], esi
        mov     eax, [edi + SRV.srv_proc]
        test    eax, eax
        jz      .fail
        stdcall eax, esi
        ret
.fail:
        xor     eax, eax
        not     eax
        mov     [esi + output], eax
        mov     [esi + out_size], 4
        ret
.err:
        xor     eax, eax
        not     eax
        ret
endp

; param
;  ecx= io_control
;
; retval
;  eax= error code

align 4
srv_handlerEx:
        cmp     ecx, OS_BASE
        jae     .fail

        mov     eax, [ecx + handle]

        cmp     eax, OS_BASE
        jbe     .fail

        cmp     [eax + SRV.magic], ' SRV'
        jne     .fail

        cmp     [eax + SRV.size], sizeof.SRV
        jne     .fail

;        stdcall [eax+SRV.srv_proc], ecx
        mov     eax, [eax + SRV.srv_proc]
        test    eax, eax
        jz      .fail
        stdcall eax, ecx
        ret
.fail:
        or      eax, -1
        ret

restore  handle
restore  io_code
restore  input
restore  inp_size
restore  output
restore  out_size

align 4
proc get_service stdcall, sz_name:dword
        mov     eax, [sz_name]
        test    eax, eax
        jnz     @F
        ret
@@:
        mov     edx, [srv.fd]
@@:
        cmp     edx, srv.fd - SRV.fd
        je      .not_load

        stdcall strncmp, edx, [sz_name], 16
        test    eax, eax
        mov     eax, edx
        je      .nothing

        mov     edx, [edx + SRV.fd]
        jmp     @B
.not_load:
        mov     eax, [sz_name]
        push    edi
        sub     esp, 36
        mov     edi, esp
        mov     dword [edi], '/sys'
        mov     dword [edi+4], '/dri'
        mov     dword [edi+8], 'vers'
        mov     byte [edi+12], '/'
@@:
        mov     dl, [eax]
        mov     [edi+13], dl
        inc     eax
        inc     edi
        test    dl, dl
        jnz     @b
        mov     dword [edi+12], '.sys'
        mov     byte [edi+16], 0
        mov     edi, esp
        stdcall load_pe_driver, edi, 0
        add     esp, 36
        pop     edi
.nothing:
        ret
endp

reg_service:
        xor     eax, eax
        mov     ecx, [esp+8]
        ;jecxz   .nothing
        push    sizeof.SRV
        push    ecx
        pushd   [esp+12]
        call    reg_service_ex
.nothing:
        ret     8

reg_usb_driver:
        push    sizeof.USBSRV
        pushd   [esp+12]
        pushd   [esp+12]
        call    reg_service_ex
        test    eax, eax
        jz      .nothing
        mov     ecx, [esp+12]
        mov     [eax+USBSRV.usb_func], ecx
.nothing:
        ret     12

proc reg_service_ex stdcall, name:dword, handler:dword, srvsize:dword

        push    ebx

        xor     eax, eax

        cmp     [name], eax
        je      .fail

;        cmp     [handler], eax
;        je      .fail

        mov     eax, [srvsize]
        call    malloc
        test    eax, eax
        jz      .fail

        push    esi
        push    edi
        mov     edi, eax
        mov     esi, [name]
        movsd
        movsd
        movsd
        movsd
        pop     edi
        pop     esi

        mov     [eax + SRV.magic], ' SRV'
        mov     [eax + SRV.size], sizeof.SRV

        mov     ebx, srv.fd - SRV.fd
        mov     edx, [ebx + SRV.fd]
        mov     [eax + SRV.fd], edx
        mov     [eax + SRV.bk], ebx
        mov     [ebx + SRV.fd], eax
        mov     [edx + SRV.bk], eax

        mov     ecx, [handler]
        mov     [eax + SRV.srv_proc], ecx
        ;dec     [count_services]
        pop     ebx
        ret
.fail:
        xor     eax, eax
        pop     ebx
        ret
endp

align 4
stop_all_services:      ;used in shutdown system
        push    ebp
        mov     edx, [srv.fd]
.next:
        cmp     edx, srv.fd - SRV.fd
        je      .done
        cmp     [edx + SRV.magic], ' SRV'
        jne     .next
        cmp     [edx + SRV.size], sizeof.SRV
        jne     .next

        mov     ebx, [edx + SRV.entry]
        mov     edx, [edx + SRV.fd]
        test    ebx, ebx
        jz      .next

        push    edx
        mov     ebp, esp
        push    0
        push    DRV_EXIT
        call    ebx  ;stdcall
        mov     esp, ebp
        pop     edx
        jmp     .next
.done:
        pop     ebp
        ret

align 4
proc get_proc stdcall, exp:dword, sz_name:dword

        mov     edx, [exp]
.next:
        mov     eax, [edx]
        test    eax, eax
        jz      .end

        push    edx
        stdcall strncmp, eax, [sz_name], 16
        pop     edx
        test    eax, eax
        jz      .ok

        add     edx, 8
        jmp     .next
.ok:
        mov     eax, [edx+4]
.end:
        ret
endp

align 4
proc get_coff_sym stdcall, pSym:dword,count:dword, sz_sym:dword

@@:
        stdcall strncmp, [pSym], [sz_sym], sizeof.COFF_SYM.Name
        test    eax, eax
        jz      .ok
        add     [pSym], sizeof.COFF_SYM
        dec     [count]
        jnz     @b
        xor     eax, eax
        ret
.ok:
        mov     eax, [pSym]
        mov     eax, [eax+COFF_SYM.Value]
        ret
endp


align 4
proc get_fileinfo stdcall, file_name:dword, info:dword
           locals
             cmd     dd ?
             offset  dd ?
                     dd ?
             count   dd ?
             buff    dd ?
                     db ?
             name    dd ?
           endl

        xor     eax, eax
        mov     ebx, [file_name]
        mov     ecx, [info]

        mov     [cmd], 5
        mov     [offset], eax
        mov     [offset+4], eax
        mov     [count], eax
        mov     [buff], ecx
        mov     byte [buff+4], al
        mov     [name], ebx

        mov     eax, 70
        lea     ebx, [cmd]
        pushad
        cld
        call    protect_from_terminate
        call    file_system_lfn
        call    unprotect_from_terminate
        popad
        ret
endp

align 4
proc read_file stdcall,file_name:dword, buffer:dword, off:dword,\
                                     bytes:dword
           locals
             cmd     dd ?
             offset  dd ?
                     dd ?
             count   dd ?
             buff    dd ?
                     db ?
             name    dd ?
           endl

        xor     eax, eax
        mov     ebx, [file_name]
        mov     ecx, [off]
        mov     edx, [bytes]
        mov     esi, [buffer]

        mov     [cmd], eax
        mov     [offset], ecx
        mov     [offset+4], eax
        mov     [count], edx
        mov     [buff], esi
        mov     byte [buff+4], al
        mov     [name], ebx

        pushad
        lea     ebx, [cmd]
        call    file_system_lfn_protected
        popad
        ret
endp

align 4
; @brief Allocate kernel memory and loads the specified file
;
; @param file_name Path to file
;
; @returns File image in kernel memory in `eax` and size of file in `ebx`
;
; @warning You must call kernel_free() to delete each file loaded by the
;          load_file() function
proc load_file stdcall, file_name:dword
           locals
             attr       dd ?
             flags      dd ?
             cr_time    dd ?
             cr_date    dd ?
             acc_time   dd ?
             acc_date   dd ?
             mod_time   dd ?
             mod_date   dd ?
             file_size  dd ?

             file       dd ?
             file2      dd ?
           endl

        push    esi
        push    edi

        lea     eax, [attr]
        stdcall get_fileinfo, [file_name], eax
        test    eax, eax
        jnz     .fail

        mov     eax, [file_size]
        cmp     eax, 1024*1024*16
        ja      .fail

        stdcall kernel_alloc, [file_size]
        mov     [file], eax
        test    eax, eax
        jz      .fail

        stdcall read_file, [file_name], eax, dword 0, [file_size]
        cmp     ebx, [file_size]
        jne     .cleanup

        mov     eax, [file]
        cmp     dword [eax], 'KPCK'
        jne     .exit
        mov     ebx, [eax+4]
        mov     [file_size], ebx
        stdcall kernel_alloc, ebx

        test    eax, eax
        jz      .cleanup

        mov     [file2], eax

        pushad
        mov     ecx, unpack_mutex
        call    mutex_lock
        popad

        stdcall unpack, [file], eax

        pushad
        mov     ecx, unpack_mutex
        call    mutex_unlock
        popad

        stdcall kernel_free, [file]
        mov     eax, [file2]
        mov     ebx, [file_size]
.exit:
        push    eax
        lea     edi, [eax+ebx]    ;cleanup remain space
        mov     ecx, 4096         ;from file end
        and     ebx, 4095
        jz      @f
        sub     ecx, ebx
        xor     eax, eax
        cld
        rep stosb
@@:
        mov     ebx, [file_size]
        pop     eax
        pop     edi
        pop     esi
        ret
.cleanup:
        stdcall kernel_free, [file]
.fail:
        xor     eax, eax
        xor     ebx, ebx
        pop     edi
        pop     esi
        ret
endp

; description
;  allocate user memory and loads the specified file
;
; param
;  file_name= path to file
;
; retval
;  eax= file image in user memory
;  ebx= size of file
;
; warging
;  You mast call kernel_free() to delete each file
;  loaded by the load_file() function

align 4
proc load_file_umode stdcall, file_name:dword
           locals
             attr       dd ?
             flags      dd ?
             cr_time    dd ?
             cr_date    dd ?
             acc_time   dd ?
             acc_date   dd ?
             mod_time   dd ?
             mod_date   dd ?
             file_size  dd ?

             km_file    dd ?
             um_file    dd ?
           endl

        push    esi
        push    edi
        push    ebx

        lea     eax, [attr]
        stdcall get_fileinfo, [file_name], eax   ;find file and get info
        test    eax, eax
        jnz     .err_1

        mov     eax, [file_size]
        cmp     eax, 1024*1024*1024                ;to be enough for anybody 1Gbt(c)
        ja      .err_1
                                                 ;it is very likely that the file is packed
        stdcall kernel_alloc, [file_size]        ;with kpack, so allocate memory from kernel heap
        mov     [km_file], eax
        test    eax, eax
        jz      .err_1

        stdcall read_file, [file_name], eax, dword 0, [file_size]
        cmp     ebx, [file_size]

        jne     .err_2

        mov     eax, [km_file]
        cmp     dword [eax], 'KPCK'              ; check kpack signature
        jne     .raw_file

        mov     ebx, [eax+4]                     ;get real size of file
        mov     [file_size], ebx
        stdcall user_alloc, ebx                  ;and allocate space from user heap
        mov     [um_file], eax
        test    eax, eax
        jz      .err_2

        mov     edx, [file_size]                 ;preallocate page memory
        shr     eax, 10
        lea     edi, [page_tabs + eax]
        add     edx, 4095
        shr     edx, 12
@@:
        call    alloc_page
        test    eax, eax
        jz      .err_3

        or      eax, PG_UWR
        stosd
        dec     edx
        jnz     @B

        pushad
        mov     ecx, unpack_mutex
        call    mutex_lock

        stdcall unpack, [km_file], [um_file]

        mov     ecx, unpack_mutex
        call    mutex_unlock
        popad

        stdcall kernel_free, [km_file]           ;we don't need packed file anymore
.exit:

        mov     edi, [um_file]
        mov     esi, [um_file]
        mov     eax, [file_size]
        mov     edx, eax

        add     edi, eax                         ;cleanup remain space
        mov     ecx, 4096                        ;from file end
        and     eax, 4095
        jz      @f
        sub     ecx, eax
        xor     eax, eax
        cld
        rep stosb
@@:
        mov     eax, [um_file]

        pop     ebx
        pop     edi
        pop     esi
        ret

.raw_file:                                       ; sometimes we load unpacked file
        stdcall user_alloc, ebx                  ; allocate space from user heap
        mov     [um_file], eax

        test    eax, eax
        jz      .err_2

        shr     eax, 10                          ; and remap pages.

        mov     ecx, [file_size]
        add     ecx, 4095
        shr     ecx, 12

        mov     esi, [km_file]
        shr     esi, 10
        add     esi, page_tabs

        lea     edi, [page_tabs+eax]

        cld
@@:
        lodsd
        and     eax, 0xFFFFF000
        or      eax, PG_UWR
        stosd
        loop    @B

        stdcall free_kernel_space, [km_file]     ; release allocated kernel space
        jmp     .exit                            ; physical pages still in use
.err_3:
        stdcall user_free, [um_file]
.err_2:
        stdcall kernel_free, [km_file]
.err_1:
        xor     eax, eax
        xor     edx, edx

        pop     ebx
        pop     edi
        pop     esi
        ret
endp


uglobal
align 4
unpack_mutex MUTEX
endg

align 4
proc get_proc_ex stdcall uses ebx esi, proc_name:dword, imports:dword
        mov     ebx, [imports]
        test    ebx, ebx
        jz      .end
        xor     esi, esi
.look_up:

        mov     eax, [ebx+32]
        mov     eax, [OS_BASE + eax + esi*4]
        add     eax, OS_BASE
        stdcall strncmp, eax, [proc_name], 256
        test    eax, eax
        jz      .ok

        inc     esi
        cmp     esi, [ebx+24]
        jb      .look_up
.end:
        xor     eax, eax
        ret
.ok:
        mov     eax, [ebx+28]
        mov     eax, [OS_BASE + eax + esi*4]
        add     eax, OS_BASE
        ret
endp

align 4
proc fix_coff_symbols stdcall uses ebx esi, sec:dword, symbols:dword,\
                      sym_count:dword, strings:dword, imports:dword
           locals
             retval dd ?
           endl

        mov     edi, [symbols]
        mov     [retval], 1
.fix:
        movzx   ebx, [edi + COFF_SYM.SectionNumber]
        test    ebx, ebx
        jnz     .internal
        mov     eax, dword [edi + COFF_SYM.Name]
        test    eax, eax
        jnz     @F

        mov     edi, [edi+4]
        add     edi, [strings]
@@:
        push    edi
        stdcall get_proc_ex, edi, [imports]
        pop     edi

        xor     ebx, ebx
        test    eax, eax
        jnz     @F

        ; disable debug msg
        ;mov     esi, msg_unresolved
        ;call    sys_msg_board_str
        ;mov     esi, edi
        ;call    sys_msg_board_str
        ;mov     esi, msg_CR
        ;call    sys_msg_board_str

        mov     [retval], 0
@@:
        mov     edi, [symbols]
        mov     [edi + COFF_SYM.Value], eax
        jmp     .next
.internal:
        cmp     bx, -1
        je      .next
        cmp     bx, -2
        je      .next

        dec     ebx
        shl     ebx, 3
        lea     ebx, [ebx + ebx*4]
        add     ebx, [sec]

        mov     eax, [ebx + COFF_SECTION.VirtualAddress]
        add     [edi + COFF_SYM.Value], eax
.next:
        add     edi, sizeof.COFF_SYM
        mov     [symbols], edi
        dec     [sym_count]
        jnz     .fix
        mov     eax, [retval]
        ret
endp

align 4
proc fix_coff_relocs stdcall uses ebx esi, coff:dword, sym:dword, \
        delta:dword
           locals
             n_sec     dd ?
           endl

        mov     eax, [coff]
        movzx   ebx, [eax + COFF_HEADER.nSections]
        mov     [n_sec], ebx
        lea     esi, [eax+20]
.fix_sec:
        mov     edi, [esi + COFF_SECTION.PtrReloc]
        add     edi, [coff]

        movzx   ecx, [esi + COFF_SECTION.NumReloc]
        test    ecx, ecx
        jz      .next
.reloc_loop:
        mov     ebx, [edi + COFF_RELOC.SymIndex]
        add     ebx, ebx
        lea     ebx, [ebx+ebx*8]
        add     ebx, [sym]

        mov     edx, [ebx + COFF_SYM.Value]

        cmp     [edi + COFF_RELOC.Type], 6
        je      .dir_32

        cmp     [edi + COFF_RELOC.Type], 20
        jne     .next_reloc
.rel_32:
        mov     eax, [edi + COFF_RELOC.VirtualAddress]
        add     eax, [esi + COFF_SECTION.VirtualAddress]
        sub     edx, eax
        sub     edx, 4
        jmp     .fix
.dir_32:
        mov     eax, [edi + COFF_RELOC.VirtualAddress]
        add     eax, [esi + COFF_SECTION.VirtualAddress]
.fix:
        add     eax, [delta]
        add     [eax], edx
.next_reloc:
        add     edi, 10
        dec     ecx
        jnz     .reloc_loop
.next:
        add     esi, sizeof.COFF_SECTION
        dec     [n_sec]
        jnz     .fix_sec
.exit:
        ret
endp

align 4
proc rebase_coff stdcall uses ebx esi, coff:dword, sym:dword, \
        delta:dword
           locals
             n_sec     dd ?
           endl

        mov     eax, [coff]
        movzx   ebx, [eax + COFF_HEADER.nSections]
        mov     [n_sec], ebx
        lea     esi, [eax+20]
        mov     edx, [delta]
.fix_sec:
        mov     edi, [esi + COFF_SECTION.PtrReloc]
        add     edi, [coff]

        movzx   ecx, [esi + COFF_SECTION.NumReloc]
        test    ecx, ecx
        jz      .next
.reloc_loop:
        cmp     [edi + COFF_RELOC.Type], 6
        jne     .next_reloc
.dir_32:
        mov     eax, [edi + COFF_RELOC.VirtualAddress]
        add     eax, [esi + COFF_SECTION.VirtualAddress]
        add     [eax + edx], edx
.next_reloc:
        add     edi, 10
        dec     ecx
        jnz     .reloc_loop
.next:
        add     esi, sizeof.COFF_SECTION
        dec     [n_sec]
        jnz     .fix_sec
.exit:
        ret
endp

; in: edx -> COFF_SECTION struct
; out: eax = alignment as mask for bits to drop
coff_get_align:
; Rules:
; - if alignment is not given, use default = 4K;
; - if alignment is given and is no more than 4K, use it;
; - if alignment is more than 4K, revert to 4K.
        push    ecx
        mov     cl, byte [edx + COFF_SECTION.Characteristics+2]
        mov     eax, 1
        shr     cl, 4
        dec     cl
        js      .default
        cmp     cl, 12
        jbe     @f
.default:
        mov     cl, 12
@@:
        shl     eax, cl
        pop     ecx
        dec     eax
        ret

align 4
proc load_library stdcall, file_name:dword, encoding:dword
    locals
        fullname    dd  ?
        fileinfo    rb  40
        coff        dd  ?
        img_base    dd  ?
    endl

; resolve file name
        stdcall kernel_alloc, maxPathLength
        mov     [fullname], eax
        mov     edi, eax
        mov     esi, [file_name]
        mov     eax, [encoding]
        push    ebp
        call    getFullPath
        pop     ebp
        test    eax, eax
        jz      .fail
; scan for required DLL in list of already loaded for this process,
; ignore timestamp
        cli
        mov     esi, [current_process]
        mov     edi, [fullname]
        mov     ebx, [esi + PROC.dlls_list_ptr]
        test    ebx, ebx
        jz      .not_in_process
        mov     esi, [ebx + HDLL.fd]
.scan_in_process:
        cmp     esi, ebx
        jz      .not_in_process
        mov     eax, [esi + HDLL.parent]
        add     eax, DLLDESCR.name
        stdcall strncmp, eax, edi, -1
        test    eax, eax
        jnz     .next_in_process
; simple variant: load DLL which is already loaded in this process
; just increment reference counters and return address of exports table
        inc     [esi + HDLL.refcount]
        mov     ecx, [esi + HDLL.parent]
        inc     [ecx + DLLDESCR.refcount]
        mov     eax, [ecx + DLLDESCR.exports]
        sub     eax, [ecx + DLLDESCR.defaultbase]
        add     eax, [esi + HDLL.base]
        sti
        push    eax
        stdcall kernel_free, [fullname]
        pop     eax
        ret

.next_in_process:
        mov     esi, [esi + HDLL.fd]
        jmp     .scan_in_process

.not_in_process:
; scan in full list, compare timestamp
        sti
        lea     eax, [fileinfo]
        stdcall get_fileinfo, edi, eax
        test    eax, eax
        jnz     .fail
        cli
        mov     esi, [dll_list.fd]
.scan_for_dlls:
        cmp     esi, dll_list
        jz      .load_new
        lea     eax, [esi + DLLDESCR.name]
        stdcall strncmp, eax, edi, -1
        test    eax, eax
        jnz     .continue_scan
.test_prev_dll:
        mov     eax, dword [fileinfo+24]; last modified time
        mov     edx, dword [fileinfo+28]; last modified date
        cmp     dword [esi + DLLDESCR.timestamp], eax
        jnz     .continue_scan
        cmp     dword [esi + DLLDESCR.timestamp+4], edx
        jz      .dll_already_loaded
.continue_scan:
        mov     esi, [esi + DLLDESCR.fd]
        jmp     .scan_for_dlls

; new DLL
.load_new:
        sti
; load file
        stdcall load_file, edi
        test    eax, eax
        jz      .fail
        mov     [coff], eax
        mov     dword [fileinfo+32], ebx

; allocate DLLDESCR struct; size is DLLDESCR.sizeof plus size of DLL name
        mov     esi, edi
        mov     ecx, -1
        xor     eax, eax
        repnz scasb
        not     ecx
        lea     eax, [ecx + sizeof.DLLDESCR]
        push    ecx
        call    malloc
        pop     ecx
        test    eax, eax
        jz      .fail_and_free_coff
; save timestamp
        lea     edi, [eax + DLLDESCR.name]
        rep movsb
        mov     esi, eax
        mov     eax, dword [fileinfo+24]
        mov     dword [esi + DLLDESCR.timestamp], eax
        mov     eax, dword [fileinfo+28]
        mov     dword [esi + DLLDESCR.timestamp+4], eax

; calculate size of loaded DLL
        mov     edx, [coff]
        movzx   ecx, [edx + COFF_HEADER.nSections]
        xor     ebx, ebx

        add     edx, sizeof.COFF_HEADER
@@:
        call    coff_get_align
        add     ebx, eax
        not     eax
        and     ebx, eax
        add     ebx, [edx + COFF_SECTION.SizeOfRawData]
        add     edx, sizeof.COFF_SECTION
        dec     ecx
        jnz     @B
; it must be nonzero and not too big
        mov     [esi + DLLDESCR.size], ebx
        test    ebx, ebx
        jz      .fail_and_free_dll
        cmp     ebx, MAX_DEFAULT_DLL_ADDR-MIN_DEFAULT_DLL_ADDR
        ja      .fail_and_free_dll
; allocate memory for kernel-side image
        stdcall kernel_alloc, ebx
        test    eax, eax
        jz      .fail_and_free_dll
        mov     [esi + DLLDESCR.data], eax
; calculate preferred base address
        add     ebx, 0x1FFF
        and     ebx, not 0xFFF
        mov     ecx, [dll_cur_addr]
        lea     edx, [ecx + ebx]
        cmp     edx, MAX_DEFAULT_DLL_ADDR
        jb      @f
        mov     ecx, MIN_DEFAULT_DLL_ADDR
        lea     edx, [ecx + ebx]
@@:
        mov     [esi + DLLDESCR.defaultbase], ecx
        mov     [dll_cur_addr], edx

; copy sections and set correct values for VirtualAddress'es in headers
        push    esi
        mov     edx, [coff]
        movzx   ebx, [edx + COFF_HEADER.nSections]
        mov     edi, eax
        add     edx, 20
        cld
@@:
        call    coff_get_align
        add     ecx, eax
        add     edi, eax
        not     eax
        and     ecx, eax
        and     edi, eax
        mov     [edx + COFF_SECTION.VirtualAddress], ecx
        add     ecx, [edx + COFF_SECTION.SizeOfRawData]
        mov     esi, [edx + COFF_SECTION.PtrRawData]
        push    ecx
        mov     ecx, [edx + COFF_SECTION.SizeOfRawData]
        test    esi, esi
        jnz     .copy
        xor     eax, eax
        rep stosb
        jmp     .next
.copy:
        add     esi, [coff]
        rep movsb
.next:
        pop     ecx
        add     edx, sizeof.COFF_SECTION
        dec     ebx
        jnz     @B
        pop     esi

; save some additional data from COFF file
; later we will use COFF header, headers for sections and symbol table
; and also relocations table for all sections
        mov     edx, [coff]
        mov     ebx, [edx + COFF_HEADER.pSymTable]
        mov     edi, dword [fileinfo+32]
        sub     edi, ebx
        jc      .fail_and_free_data
        mov     [esi + DLLDESCR.symbols_lim], edi
        add     ebx, edx
        movzx   ecx, [edx + COFF_HEADER.nSections]
        lea     ecx, [ecx*5]
        lea     edi, [edi + ecx*8+20]
        add     edx, sizeof.COFF_HEADER
@@:
        movzx   eax, [edx + COFF_SECTION.NumReloc]
        lea     eax, [eax*5]
        lea     edi, [edi + eax*2]
        add     edx, sizeof.COFF_SECTION
        sub     ecx, 5
        jnz     @b
        stdcall kernel_alloc, edi
        test    eax, eax
        jz      .fail_and_free_data
        mov     edx, [coff]
        movzx   ecx, [edx + COFF_HEADER.nSections]
        lea     ecx, [ecx*5]
        lea     ecx, [ecx*2+5]
        mov     [esi + DLLDESCR.coff_hdr], eax
        push    esi
        mov     esi, edx
        mov     edi, eax
        rep movsd
        pop     esi
        mov     [esi + DLLDESCR.symbols_ptr], edi
        push    esi
        mov     ecx, [edx+COFF_HEADER.nSymbols]
        mov     [esi + DLLDESCR.symbols_num], ecx
        mov     ecx, [esi + DLLDESCR.symbols_lim]
        mov     esi, ebx
        rep movsb
        pop     esi
        mov     ebx, [esi + DLLDESCR.coff_hdr]
        push    esi
        movzx   eax, [edx + COFF_HEADER.nSections]
        lea     edx, [ebx+20]
@@:
        movzx   ecx, [edx + COFF_SECTION.NumReloc]
        lea     ecx, [ecx*5]
        mov     esi, [edx + COFF_SECTION.PtrReloc]
        mov     [edx + COFF_SECTION.PtrReloc], edi
        sub     [edx + COFF_SECTION.PtrReloc], ebx
        add     esi, [coff]
        shr     ecx, 1
        rep movsd
        adc     ecx, ecx
        rep movsw
        add     edx, sizeof.COFF_SECTION
        dec     eax
        jnz     @b
        pop     esi

; fixup symbols
        mov     edx, ebx
        mov     eax, [ebx + COFF_HEADER.nSymbols]
        add     edx, sizeof.COFF_HEADER
        mov     ecx, [esi + DLLDESCR.symbols_num]
        lea     ecx, [ecx*9]
        add     ecx, ecx
        add     ecx, [esi + DLLDESCR.symbols_ptr]

        stdcall fix_coff_symbols, edx, [esi + DLLDESCR.symbols_ptr], eax, \
                ecx, 0
;          test eax, eax
;          jnz @F
;
;@@:

        stdcall get_coff_sym, [esi + DLLDESCR.symbols_ptr], [ebx + COFF_HEADER.nSymbols], szEXPORTS
        test    eax, eax
        jnz     @F

        stdcall get_coff_sym, [esi + DLLDESCR.symbols_ptr], [ebx + COFF_HEADER.nSymbols], sz_EXPORTS
@@:
        mov     [esi + DLLDESCR.exports], eax

; fix relocs in the hidden copy in kernel memory to default address
; it is first fix; usually this will be enough, but second fix
; can be necessary if real load address will not equal assumption
        mov     eax, [esi + DLLDESCR.data]
        sub     eax, [esi + DLLDESCR.defaultbase]
        stdcall fix_coff_relocs, ebx, [esi + DLLDESCR.symbols_ptr], eax

        stdcall kernel_free, [coff]

        cli
; initialize DLLDESCR struct
        and     dword [esi + DLLDESCR.refcount], 0; no HDLLs yet; later it will be incremented
        mov     [esi + DLLDESCR.fd], dll_list
        mov     eax, [dll_list.bk]
        mov     [dll_list.bk], esi
        mov     [esi + DLLDESCR.bk], eax
        mov     [eax + DLLDESCR.fd], esi
.dll_already_loaded:
        stdcall kernel_free, [fullname]
        inc     [esi + DLLDESCR.refcount]
        push    esi
        call    init_heap
        pop     esi
        mov     edi, [esi + DLLDESCR.size]
        stdcall user_alloc_at, [esi + DLLDESCR.defaultbase], edi
        test    eax, eax
        jnz     @f
        stdcall user_alloc, edi
        test    eax, eax
        jz      .fail_and_dereference
@@:
        mov     [img_base], eax
        mov     eax, sizeof.HDLL
        call    malloc
        test    eax, eax
        jz      .fail_and_free_user
        mov     ebx, [current_slot]
        mov     edx, [ebx + APPDATA.tid]
        mov     [eax + HDLL.pid], edx
        push    eax
        call    init_dlls_in_thread
        pop     ebx
        test    eax, eax
        jz      .fail_and_free_user
        mov     edx, [eax + HDLL.fd]
        mov     [ebx + HDLL.fd], edx
        mov     [ebx + HDLL.bk], eax
        mov     [eax + HDLL.fd], ebx
        mov     [edx + HDLL.bk], ebx
        mov     eax, ebx
        mov     ebx, [img_base]
        mov     [eax + HDLL.base], ebx
        mov     [eax + HDLL.size], edi
        mov     [eax + HDLL.refcount], 1
        mov     [eax + HDLL.parent], esi
        mov     edx, ebx
        shr     edx, 12
        or      dword [page_tabs + (edx-1)*4], MEM_BLOCK_DONT_FREE
; copy entries of page table from kernel-side image to usermode
; use copy-on-write for user-mode image, so map as readonly
        xor     edi, edi
        mov     ecx, [esi + DLLDESCR.data]
        shr     ecx, 12
.map_pages_loop:
        mov     eax, [page_tabs + ecx*4]
        and     eax, -PAGE_SIZE
        or      al, PG_UR
        xchg    eax, [page_tabs + edx*4]
        test    al, 1
        jz      @f
        call    free_page
@@:
        invlpg  [ebx+edi]
        inc     ecx
        inc     edx
        add     edi, PAGE_SIZE
        cmp     edi, [esi + DLLDESCR.size]
        jb      .map_pages_loop

; if real user-mode base is not equal to preferred base, relocate image
        sub     ebx, [esi + DLLDESCR.defaultbase]
        jz      @f
        stdcall rebase_coff, [esi + DLLDESCR.coff_hdr], [esi + DLLDESCR.symbols_ptr], ebx
@@:

        mov     eax, [esi + DLLDESCR.exports]
        sub     eax, [esi + DLLDESCR.defaultbase]
        add     eax, [img_base]
        sti
        ret

.fail_and_free_data:
        stdcall kernel_free, [esi + DLLDESCR.data]
.fail_and_free_dll:
        mov     eax, esi
        call    free
.fail_and_free_coff:
        stdcall kernel_free, [coff]
.fail:
        stdcall kernel_free, [fullname]
        xor     eax, eax
        ret

.fail_and_free_user:
        stdcall user_free, [img_base]
.fail_and_dereference:
        mov     eax, 1  ; delete 1 reference
        call    dereference_dll
        sti
        xor     eax, eax
        ret
endp

; initialize [APPDATA.dlls_list_ptr] for given thread
; DLL is per-process object, so APPDATA.dlls_list_ptr must be
; kept in sync for all threads of one process.
; out: eax = APPDATA.dlls_list_ptr if all is OK,
; NULL if memory allocation failed
init_dlls_in_thread:
        mov     ebx, [current_process]
        mov     eax, [ebx + PROC.dlls_list_ptr]
        test    eax, eax
        jnz     .ret

        mov     eax, 8
        call    malloc                               ; FIXME
        test    eax, eax
        jz      .ret

        mov     [eax], eax
        mov     [eax+4], eax

        mov     ebx, [current_process]
        mov     [ebx + PROC.dlls_list_ptr], eax
.ret:
        ret

; in: eax = number of references to delete, esi -> DLLDESCR struc
dereference_dll:
        sub     [esi + DLLDESCR.refcount], eax
        jnz     .ret
        mov     eax, [esi + DLLDESCR.fd]
        mov     edx, [esi + DLLDESCR.bk]
        mov     [eax + DLLDESCR.bk], edx
        mov     [edx + DLLDESCR.fd], eax
        stdcall kernel_free, [esi + DLLDESCR.coff_hdr]
        stdcall kernel_free, [esi + DLLDESCR.data]
        mov     eax, esi
        call    free
.ret:
        ret

destroy_hdll:
        push    ebx ecx esi edi
        mov     ebx, [eax + HDLL.base]
        mov     esi, [eax + HDLL.parent]
        mov     edx, [esi + DLLDESCR.size]

        push    eax
        mov     esi, [eax + HDLL.parent]
        mov     eax, [eax + HDLL.refcount]
        call    dereference_dll
        pop     eax
        mov     edx, [eax + HDLL.bk]
        mov     ebx, [eax + HDLL.fd]
        mov     [ebx + HDLL.bk], edx
        mov     [edx + HDLL.fd], ebx
        call    free
        pop     edi esi ecx ebx
        ret

; ecx -> APPDATA for slot, esi = dlls_list_ptr
destroy_all_hdlls:
        test    esi, esi
        jz      .ret
.loop:
        mov     eax, [esi + HDLL.fd]
        cmp     eax, esi
        jz      free
        call    destroy_hdll
        jmp     .loop
.ret:
        ret

; param
;  eax= size
;  ebx= pid

align 4
create_kernel_object:

        push    ebx
        call    malloc
        pop     ebx
        test    eax, eax
        jz      .fail

        mov     ecx, [current_slot]
        add     ecx, APP_OBJ_OFFSET

        pushfd
        cli
        mov     edx, [ecx + APPOBJ.fd]
        mov     [eax + APPOBJ.fd], edx
        mov     [eax + APPOBJ.bk], ecx
        mov     [eax + APPOBJ.pid], ebx

        mov     [ecx + APPOBJ.fd], eax
        mov     [edx + APPOBJ.bk], eax
        popfd
.fail:
        ret

; param
;  eax= object

align 4
destroy_kernel_object:

        pushfd
        cli
        mov     ebx, [eax + APPOBJ.fd]
        mov     ecx, [eax + APPOBJ.bk]
        mov     [ebx + APPOBJ.bk], ecx
        mov     [ecx + APPOBJ.fd], ebx
        popfd

        xor     edx, edx       ;clear common header
        mov     [eax], edx
        mov     [eax+4], edx
        mov     [eax+8], edx
        mov     [eax+12], edx
        mov     [eax+16], edx

        call    free           ;release object memory
        ret


;void* __fastcall create_object(size_t size)
; param
;  ecx= size

align 4
create_object:

        push    esi
        push    edi
        pushfd
        cli

        mov     esi, [current_process]
        mov     eax, [esi + PROC.ht_free]
        mov     edi, [esi + PROC.ht_next]
        dec     eax
        js      .err0

        mov     [esi + PROC.ht_free], eax
        mov     eax, [esi + PROC.htab + edi*4]
        mov     [esi + PROC.ht_next], eax
        popfd

        mov     eax, ecx
        call    malloc
        test    eax, eax
        jz      .err1

        mov     [eax + FUTEX.handle], edi
        mov     [esi + PROC.htab+edi*4], eax
        pop     edi
        pop     esi
        ret

.err1:
        pushfd
        cli

        mov     eax, [esi + PROC.ht_next]
        mov     [esi + PROC.htab+edi*4], eax
        mov     [esi + PROC.ht_next], edi
        inc     [esi + PROC.ht_free]
.err0:
        popfd
        pop     edi
        pop     esi
        xor     eax, eax
        ret


;int __fastcall destroy_object(struct object *obj)

align 4
destroy_object:
        push    esi
        mov     esi, [current_process]
        mov     edx, [ecx + FUTEX.handle]

        pushfd
        cli

        mov     eax, [esi + PROC.ht_next]
        mov     [esi + PROC.htab + edx*4], eax
        mov     [esi + PROC.ht_next], edx
        inc     [esi + PROC.ht_free]

        popfd
        pop     esi

        mov     eax, ecx
        call    free
        xor     eax, eax
        ret
.fail:
        popfd
        pop     esi
        mov     eax, -1
        ret

