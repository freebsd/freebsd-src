dir /usr/src/sys/modules/vinum
dir /usr/src/sys/i386/conf
set remotebaud 38400
core-file
dir /usr/src/sys
file /var/crash/kernel.gdb
set complaints 1
set print pretty
def xi
x/10i $eip
end
def xs
x/12x $esp
end
def xb
x/12x $ebp
end
def z
ni
x/1i $eip
end
def zs
si
x/1i $eip
end
def xp
printf "      esp: " 
output/x $esp
echo  (
output (((int)$ebp)-(int)$esp)/4-4
printf " words on stack)\n      ebp: " 
output/x $ebp
printf "\n      eip: " 
x/1i $eip
printf "Saved ebp: " 
output/x *(int*)$ebp
printf " (maximum of "  
output ((*(int*)$ebp)-(int)$ebp)/4-4
printf " parameters possible)\nSaved eip: " 
x/1i *(int*)($ebp+4)
printf "\nParm 1 at " 
output/x (int) ($ebp+8)
printf ":    " 
output (char*) *(int*)($ebp+8)
printf "\nParm 2 at " 
output/x (int) ($ebp+12)
printf ":    " 
output (char*) *(int*)($ebp+12)
printf "\nParm 3 at " 
output/x (int) ($ebp+16)
printf ":    " 
output (char*) *(int*)($ebp+16)
printf "\nParm 4 at " 
output/x (int) ($ebp+20)
printf ":    " 
output (char*) *(int*)($ebp+20)
echo \n
end
def xx
printf "      ebp: " 
output/x vebp
printf "Saved ebp: " 
output/x *(int*)vebp
printf " (maximum of "  
output ((*(int*)vebp)-(int)vebp)/4-4
printf " parameters possible)\nSaved eip: " 
x/1i *(int*)(vebp+4)
printf "\nParm 1 at " 
output/x (int) (vebp+8)
printf ":    " 
output (char*) *(int*)(vebp+8)
printf "\nParm 2 at " 
output/x (int) (vebp+12)
printf ":    " 
output (char*) *(int*)(vebp+12)
printf "\nParm 3 at " 
output/x (int) (vebp+16)
printf ":    " 
output (char*) *(int*)(vebp+16)
printf "\nParm 4 at " 
output/x (int) (vebp+20)
printf ":    " 
output (char*) *(int*)(vebp+20)
echo \n
end
document xp
Show the register contents and the first four parameter
words of the current frame.
end
def xxp
printf "      esp: " 
output/x $esp
printf "\n      ebp: " 
output/x $ebp
printf "\n      eip: " 
x/1i $eip
printf "Saved ebp: " 
output/x *(int*)$ebp
printf " (maximum of "  
output ((*(int*)$ebp)-(int)$ebp)/4-4
printf " parameters possible)\nSaved eip: " 
x/1i *(int*)($ebp+4)
printf "\nParm  1 at " 
output/x (int) ($ebp+8)
printf ":    " 
output (char*) *(int*)($ebp+8)
printf "\nParm  2 at " 
output/x (int) ($ebp+12)
printf ":    " 
output (char*) *(int*)($ebp+12)
printf "\nParm  3 at " 
output/x (int) ($ebp+16)
printf ":    " 
output (char*) *(int*)($ebp+16)
printf "\nParm  4 at " 
output/x (int) ($ebp+20)
printf ":    " 
output (char*) *(int*)($ebp+20)
printf "\nParm  5 at " 
output/x (int) ($ebp+24)
printf ":    " 
output (char*) *(int*)($ebp+24)
printf "\nParm  6 at " 
output/x (int) ($ebp+28)
printf ":    " 
output (char*) *(int*)($ebp+28)
printf "\nParm  7 at " 
output/x (int) ($ebp+32)
printf ":    " 
output (char*) *(int*)($ebp+32)
printf "\nParm  8 at " 
output/x (int) ($ebp+36)
printf ":    " 
output (char*) *(int*)($ebp+36)
printf "\nParm  9 at " 
output/x (int) ($ebp+40)
printf ":    " 
output (char*) *(int*)($ebp+40)
printf "\nParm 10 at " 
output/x (int) ($ebp+44)
printf ":    " 
output (char*) *(int*)($ebp+44)
echo \n
end
document xxp
Show the register contents and the first ten parameter
words of the current frame.
end
def xp0
x/12x *(int*)$esp
p *(int*)$esp
p (char*)*$esp
end
def xp1
x/12x *(int*)($ebp+4)
p *(int*)($ebp+4)
p (char**)($ebp+4)
end
def xp2
x/12x *(int*)($ebp+8)
p *(int*)($ebp+8)
p *(char**)($ebp+8)
end
def xp3
x/12x *(int*)($ebp+12)
p *(int*)($ebp+12)
p (char**)($ebp+12)
end
def xp4
x/12x *(int*)($ebp+16)
p *(int*)($ebp+16)
p (char**)($ebp+16)
end
document xp0
Show the first parameter of current stack frame in various formats
end
document xp1
Show the second parameter of current stack frame in various formats
end
document xp2
Show the third parameter of current stack frame in various formats
end
document xp3
Show the fourth parameter of current stack frame in various formats
end
document xp4
Show the fifth parameter of current stack frame in various formats
end
def f0
f 0
xp
end
def f1
f 1
xp
end
def f2
f 2
xp
end
def f3
f 3
xp
end
def f4
f 4
xp
end
def f5
f 5
xp
end
document f0
Select stack frame 0 and show assembler-level details
end
document f1
Select stack frame 1 and show assembler-level details
end
document f2
Select stack frame 2 and show assembler-level details
end
document f3
Select stack frame 3 and show assembler-level details
end
document f4
Select stack frame 4 and show assembler-level details
end
document f5
Select stack frame 5 and show assembler-level details
end
document z
Single step 1 instruction (over calls) and show next instruction.
end
document zs
Single step 1 instruction (through calls) and show next instruction.
end
document xi
List the next 10 instructions from the current IP value
end
document xs
Show the last 12 words on stack in hex
end
document xb
Show 12 words starting at current BP value in hex
end
def tr
target remote /dev/cuaa1
f 1
printf "eip: 0x%x\n", $eip
if (lkmods[0].area != 0)
  printf "   asf 0x%x\n", lkmods[1].area
  asf lkmods[0].area
  bt
end
end
document tr
Attach to a remote kernel via /dev/cuaa0
end
def ptok
p *(char**)0xf3e88e00@10
end
def pc
p *(struct _vinum_conf*)0xf3e88f60
end
def asf
add-symbol-file /usr/src/sys/modules/vinum/vinum.ko ($arg0 & 0xfffff000) + 0x20
end
set output-radix 16
def bpp
printf "Buffer: device: " 
output/x bp->b_dev
printf " data: " 
output/x bp->b_data
printf " length: " 
output/x bp->b_bcount
printf " offset: " 
output/x bp->b_blkno
printf "\nFlags: " 
output/x bp->b_flags
echo \n
end
def bpps
bpp
f 3
bpp
f 0
end
def pname
p (char *)curproc->p_comm
end 
def rq
rqq rq
end
def rqq
set $rq = (struct request *) $arg0
printf "Request: \n" 
output/x *$rq
printf "\n"
bpp $rq->bp
set $rqg = $rq->rqg
while ($rqg != 0)
  printf "\nRequest group at %x:\n", $rqg
  output/x *$rqg
  printf "\n"
  set $rqno = 0
  while ($rqno < $rqg->count)
    printf "rqg->rqe [%d]: ", $rqno
    rrqe &$rqg->rqe[$rqno]
    set $rqno = $rqno + 1
    end
  set $rqg = $rqg->next
  end
end
def rqe
rrqe rqe
end
def rrqe
set $rqe = (struct rqelement *) $arg0
    printf "sdoffset 0x%x, useroffset 0x%x, dataoffset 0x%x, datalen 0x%x, groupoffset 0x%x, grouplen 0x%x, buflen 0x%x\n", \
      $rqe->sdoffset, \
      $rqe->useroffset, \
      $rqe->dataoffset, \
      $rqe->datalen, \
      $rqe->groupoffset, \
      $rqe->grouplen, \
      $rqe->buflen
    printf "  Flags 0x%x,  Subdisk %d  Drive %d\n", \
      $rqe->flags, \
      $rqe->sdno, \
      $rqe->driveno
    bpp &$rqe->b
end
def bpp
set $bp = (struct buf *) $arg0
    printf "  Buffer at 0x%x: dev 0x%x  data 0x%x  bcount 0x%x  blkno 0x%x resid 0x%x\n", \
      $bp, \
      $bp->b_dev, \
      $bp->b_data, \
      $bp->b_bcount, \
      $bp->b_blkno, \
      $bp->b_resid
    printf "   flags 0x%x: ", $bp->b_flags
      if $bp->b_flags & 0x10
        printf "busy "
      end
      if $bp->b_flags & 0x40
        printf "call "
      end
      if $bp->b_flags & 0x200
        printf "done "
      end
      if $bp->b_flags & 0x800
        printf "error "
      end
      if $bp->b_flags & 0x40000
        printf "phys "
      end
      if $bp->b_flags & 0x100000
        printf "read "
      end
    printf "\n"
end
def bpl
set $bp = (struct buf *) $arg0
printf "b_proc: "
output $bp->b_proc
printf "\nb_flags:      "
output $bp->b_flags
printf "\nb_qindex:     "
output $bp->b_qindex
printf "\nb_usecount:   "
output $bp->b_usecount
printf "\nb_error:      "
output $bp->b_error
printf "\nb_bufsize:    "
output $bp->b_bufsize
printf "\nb_bcount:     "
output $bp->b_bcount
printf "\nb_resid:      "
output $bp->b_resid
printf "\nb_dev:        "
output $bp->b_dev
printf "\nb_data:       "
output $bp->b_data
printf "\nb_kvasize:    "
output $bp->b_kvasize
printf "\nb_lblkno:     "
output $bp->b_lblkno
printf "\nb_blkno:      "
output $bp->b_blkno
printf "\nb_iodone:     "
output $bp->b_iodone
printf "\nb_vp: "
output $bp->b_vp
printf "\nb_dirtyoff:   "
output $bp->b_dirtyoff
printf "\nb_dirtyend:   "
output $bp->b_dirtyend
printf "\nb_generation: "
output $bp->b_generation
printf "\nb_rcred:      "
output $bp->b_rcred
printf "\nb_wcred:      "
output $bp->b_wcred
printf "\nb_validoff:   "
output $bp->b_validoff
printf "\nb_validend:   "
output $bp->b_validend
printf "\nb_pblkno:     "
output $bp->b_pblkno
printf "\nb_saveaddr:   "
output $bp->b_saveaddr
printf "\nb_savekva:    "
output $bp->b_savekva
printf "\nb_driver1:    "
output $bp->b_driver1
printf "\nb_driver2:    "
output $bp->b_driver2
printf "\nb_spc:        "
output $bp->b_spc
printf "\nb_npages:     "
output $bp->b_npages
printf "\n"
end
def bp
bpp bp
end
def bpd
    printf "Buffer data:\n%s", (char *) bp->b_data
end
def rqq0
printf "rq->prq [0].rqe[0].sdno: " 
output/x rq->prq[0].rqe[0].sdno
printf "\nBuffer: device: " 
output/x rq->prq[0].rqe[0].b.b_dev
printf " data: " 
output/x rq->prq[0].rqe[0].b.b_data
printf " length: " 
output/x rq->prq[0].rqe[0].b.b_bcount
printf " drive offset: " 
output/x rq->prq[0].rqe[0].b.b_blkno
printf " subdisk offset: " 
output/x rq->prq[0].rqe[0].sdoffset
printf "\nFlags: " 
if (rq->prq[0].rqe[0].b.b_flags & 0x10)
printf "busy "
end
if (rq->prq[0].rqe[0].b.b_flags & 0x200)
printf "done "
end
if (rq->prq[0].rqe[0].b.b_flags & 0x800)
printf "error "
end
if (rq->prq[0].rqe[0].b.b_flags & 0x100000)
printf "read "
end
output/x rq->prq[0].rqe[0].b.b_flags
printf "\nrq->prq [0].rqe[1].sdno: " 
output/x rq->prq[0].rqe[1].sdno
printf "\nBuffer: device: " 
output/x rq->prq[0].rqe[1].b.b_dev
printf " data: " 
output/x rq->prq[0].rqe[1].b.b_data
printf " length: " 
output/x rq->prq[0].rqe[1].b.b_bcount
printf " drive offset: " 
output/x rq->prq[0].rqe[1].b.b_blkno
printf " subdisk offset: " 
output/x rq->prq[0].rqe[1].sdoffset
printf "\nFlags: " 
output/x rq->prq[0].rqe[1].b.b_flags
echo \n
end
def rqq1
printf "\nrq->prq [1].rqe[0].sdno: " 
output/x rq->prq[1].rqe[0].sdno
printf "\nBuffer: device: " 
output/x rq->prq[1].rqe[0].b.b_dev
printf " data: " 
output/x rq->prq[1].rqe[0].b.b_data
printf " length: " 
output/x rq->prq[1].rqe[0].b.b_bcount
printf " drive offset: " 
output/x rq->prq[1].rqe[0].b.b_blkno
printf " subdisk offset: " 
output/x rq->prq[1].rqe[0].sdoffset
printf "\nFlags: " 
output/x rq->prq[1].rqe[0].b.b_flags
printf "\nrq->prq [1].rqe[1].sdno: " 
output/x rq->prq[1].rqe[1].sdno
printf "\nBuffer: device: " 
output/x rq->prq[1].rqe[1].b.b_dev
printf " data: 0x%x  length 0x%x  drive offset 0x%x  sd offset 0x%x\n" rq->prq[1].rqe[1].b.b_data,
 rq->prq[1].rqe[1].b.b_bcount,
 rq->prq[1].rqe[1].b.b_blkno,
 rq->prq[1].rqe[1].sdoffset
printf "\nFlags: " 
output/x rq->prq[1].rqe[1].b.b_flags
echo \n
end
def bx
printf "\n b_vnbufs " 
output/x bp->b_vnbufs
printf "\n b_freelist " 
output/x bp->b_freelist
printf "\n b_act " 
output/x bp->b_act
printf "\n b_flags " 
output/x bp->b_flags
printf "\n b_qindex " 
output/x bp->b_qindex
printf "\n b_usecount " 
output/x bp->b_usecount
printf "\n b_error " 
output/x bp->b_error
printf "\n b_bufsize " 
output/x bp->b_bufsize
printf "\n b_bcount " 
output/x bp->b_bcount
printf "\n b_resid " 
output/x bp->b_resid
printf "\n b_dev " 
output/x bp->b_dev
printf "\n b_data " 
output/x bp->b_data
printf "\n b_kvasize " 
output/x bp->b_kvasize
printf "\n b_blkno " 
output/x bp->b_blkno
printf "\n b_iodone_chain " 
output/x bp->b_iodone_chain
printf "\n b_vp " 
output/x bp->b_vp
printf "\n b_dirtyoff " 
output/x bp->b_dirtyoff
printf "\n b_validoff " 
output/x bp->b_validoff
echo \n
end
def panic
set boothowto=0x80000000
s
end
def xy
bpp
echo stripeoffset
p stripeoffset
echo stripebase
p stripebase
echo sdno
p sdno
echo sdoffset
p sdoffset
echo rqe->sectors
p rqe->sectors
echo rqe->sdoffset
p rqe->sdoffset
end
def ps
    set $nproc = nprocs
    set $aproc = allproc.lh_first
    set $proc = allproc.lh_first
    printf "  pid    proc    addr   uid  ppid  pgrp   flag stat comm         wchan\n"
    while (--$nproc >= 0)
        set $pptr = $proc.p_pptr
        if ($pptr == 0)
           set $pptr = $proc
        end
        if ($proc.p_stat)
            printf "%5d %08x %08x %4d %5d %5d  %06x  %d  %-10s   ", \
                   $proc.p_pid, $aproc, \
                   $proc.p_addr, $proc.p_cred->p_ruid, $pptr->p_pid, \
                   $proc.p_pgrp->pg_id, $proc.p_flag, $proc.p_stat, \
                   &$proc.p_comm[0]
            if ($proc.p_wchan)
                if ($proc.p_wmesg)
                    printf "%s ", $proc.p_wmesg
                end
                printf "%x", $proc.p_wchan
            end
            printf "\n"
        end
        set $aproc = $proc.p_list.le_next
        if ($aproc == 0 && $nproc > 0)
            set $aproc = zombproc
        end
        set $proc = $aproc
    end
end
document ps
"ps" -- when kernel debugging, type out a ps-like listing of active processes.
end
def pcb
    set $nproc = nprocs
    set $aproc = allproc.lh_first
    set $proc = allproc.lh_first
    while (--$nproc >= 0)
        set $pptr = $proc.p_pptr
        if ($proc->p_pid == $arg0)
	   set $pcba = $pptr->p_addr->u_pcb
	   printf "ip: %08x sp: %08x bp: %08x bx: %08x\n", $pcba->pcb_eip, $pcba->pcb_esp, $pcba->pcb_ebp, $pcba->pcb_ebx
	   set $nproc = 0
        end
        set $aproc = $proc.p_list.le_next
        if ($aproc == 0 && $nproc > 0)
            set $aproc = zombproc
        end
        set $proc = $aproc
    end
end
set height 70
set width 120
# Catch things hanging in vinvalbuf
# b vfs_subr.c:582
# b vfs_subr.c:722
def vdev
if (vp->v_type == VBLK)
  p *vp->v_un.vu_specinfo
  printf "numoutput: %d\n", vp->v_numoutput
else
  echo "Not a block device"
end
end
tr
def rqi
   set $rqipe = *VC->rqipp
   set $rqip = $rqipe + 1
   set $rqend  = VC->rqinfop + 32
   if ($rqip == $rqend)
      set $rqip = *VC->rqinfop
   end
   set $done = 0
   while ($done == 0)
      printf "%X:\t%d.%06d\tUBP: %x\t", $rqip, $rqip->timestamp.tv_sec, $rqip->timestamp.tv_usec, $rqip->bp
      p $rqip->type
      bpp $rqip->bp
      if ($rqip->type < loginfo_rqe)
         bpp &$rqip->info
      else 
         rrqe &$rqip->info
      end
      set $rqip = $rqip + 1
      if ($rqip == $rqipe)
         set $done = 1
      end
      if ($rqip == $rqend)
         set $rqip = VC->rqinfop
      end
   end
end
def loadaddr
   set $file = files.tqh_first
   set $found = 0
   while ($found == 0)
     if (*$file->filename == 'V')
	set $found = 1
     else
       set $file = $file->link.tqe_next
     end
   end
   printf "And the winner is: %x, name: %s\n", $file, $file->filename
end
loadaddr
