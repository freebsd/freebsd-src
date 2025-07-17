# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 ConnectWise
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS DOCUMENTATION IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

. $(atf_get_srcdir)/ctl.subr

# TODO
# * PRIN READ RESERVATION, with one reservation
# * PROUT with illegal type
# * PROUT REGISTER AND IGNORE EXISTING KEY
# * PROUT REGISTER AND IGNORE EXISTING KEY with a RESERVATION KEY that isn't registered
# * PROUT REGISTER AND IGNORE EXISTING KEY to unregister
# * PROUT CLEAR allows previously prevented medium removal
# * PROUT PREEMPT
# * PROUT PREEMPT with a RESERVATION KEY that isn't registered
# * PROUT PREEMPT_AND_ABORT
# * PROUT PREEMPT_AND_ABORT with a RESERVATION KEY that isn't registered
# * PROUT REGISTER AND MOVE
# * PROUT REGISTER AND MOVE with a RESERVATION KEY that isn't registered
# * multiple initiators

# Not Tested
# * PROUT REPLACE LOST RESERVATION (not supported by ctl)
# * Specify Initiator Ports bit (not supported by ctl)
# * Activate Persist Through Power Loss bit (not supported by ctl)
# * All Target Ports bit (not supported by ctl)

RESERVATION_KEY=0xdeadbeef1a7ebabe

atf_test_case prin_read_full_status_empty cleanup
prin_read_full_status_empty_head()
{
	atf_set "descr" "PERSISTENT RESERVATION IN with the READ FULL STATUS service action, with no status descriptors"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist ctladm
}
prin_read_full_status_empty_body()
{
	create_ramdisk

	atf_check -o match:"No full status descriptors" sg_persist -ns /dev/$dev
}
prin_read_full_status_empty_cleanup()
{
	cleanup
}

atf_test_case prin_read_keys_empty cleanup
prin_read_keys_empty_head()
{
	atf_set "descr" "PERSISTENT RESERVATION IN with the READ KEYS service action, with no registered keys"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist ctladm
}
prin_read_keys_empty_body()
{
	create_ramdisk

	atf_check -o match:"there are NO registered reservation keys" sg_persist -nk /dev/$dev
}
prin_read_keys_empty_cleanup()
{
	cleanup
}

atf_test_case prin_read_reservation_empty cleanup
prin_read_reservation_empty_head()
{
	atf_set "descr" "PERSISTENT RESERVATION IN with the READ RESERVATION service action, with no reservations"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist ctladm
}
prin_read_reservation_empty_body()
{
	create_ramdisk

	atf_check -o match:"there is NO reservation held" sg_persist -nr /dev/$dev
}
prin_read_reservation_empty_cleanup()
{
	cleanup
}

atf_test_case prin_report_capabilities cleanup
prin_report_capabilities_head()
{
	atf_set "descr" "PERSISTENT RESERVATION IN with the REPORT CAPABILITIES service action"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist ctladm
}
prin_report_capabilities_body()
{
	create_ramdisk

	cat > expected <<HERE
Report capabilities response:
  Replace Lost Reservation Capable(RLR_C): 0
  Compatible Reservation Handling(CRH): 1
  Specify Initiator Ports Capable(SIP_C): 0
  All Target Ports Capable(ATP_C): 0
  Persist Through Power Loss Capable(PTPL_C): 0
  Type Mask Valid(TMV): 1
  Allow Commands: 5
  Persist Through Power Loss Active(PTPL_A): 0
    Support indicated in Type mask:
      Write Exclusive, all registrants: 1
      Exclusive Access, registrants only: 1
      Write Exclusive, registrants only: 1
      Exclusive Access: 1
      Write Exclusive: 1
      Exclusive Access, all registrants: 1
HERE
	atf_check -o file:expected sg_persist -nc /dev/$dev
}
prin_report_capabilities_cleanup()
{
	cleanup
}

atf_test_case prout_clear cleanup
prout_clear_head()
{
	atf_set "descr" "PERSISTENT RESERVATION OUT with the CLEAR service action"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist ctladm
}
prout_clear_body()
{
	create_ramdisk

	# First register a key
	atf_check sg_persist -n --out --param-rk=0 --param-sark=$RESERVATION_KEY -G /dev/$dev

	# Then make a reservation using that key
	atf_check sg_persist -n --out --param-rk=$RESERVATION_KEY --reserve --prout-type=8 /dev/$dev

	# Now, clear all reservations and registrations
	atf_check sg_persist -n --out --param-rk=$RESERVATION_KEY --clear /dev/$dev

	# Finally, check that all reservations and keys are gone
	atf_check -o match:"there is NO reservation held" sg_persist -nr /dev/$dev
	atf_check -o match:"there are NO registered reservation keys" sg_persist -nk /dev/$dev
}
prout_clear_cleanup()
{
	cleanup
}


atf_test_case prout_register cleanup
prout_register_head()
{
	atf_set "descr" "PERSISTENT RESERVATION OUT with the REGISTER service action"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist ctladm
}
prout_register_body()
{
	create_ramdisk
	atf_check sg_persist -n --out --param-rk=0 --param-sark=$RESERVATION_KEY -G /dev/$dev
	atf_check -o match:$RESERVATION_KEY sg_persist -nk /dev/$dev
}
prout_register_cleanup()
{
	cleanup
}

atf_test_case prout_register_duplicate cleanup
prout_register_duplicate_head()
{
	atf_set "descr" "attempting to register a key twice should fail"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist ctladm
}
prout_register_duplicate_body()
{
	create_ramdisk
	atf_check sg_persist -n --out --param-rk=0 --param-sark=$RESERVATION_KEY -G /dev/$dev
	atf_check -s exit:24 -e match:"Reservation conflict" sg_persist -n --out --param-rk=0 --param-sark=$RESERVATION_KEY -G /dev/$dev
	atf_check -o match:$RESERVATION_KEY sg_persist -nk /dev/$dev
}
prout_register_duplicate_cleanup()
{
	cleanup
}

atf_test_case prout_register_huge_cdb cleanup
prout_register_huge_cdb_head()
{
	atf_set "descr" "PERSISTENT RESERVATION OUT with an enormous CDB size should not cause trouble"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist ctladm
}
prout_register_huge_cdb_body()
{
	create_ramdisk

	atf_check -s exit:1 $(atf_get_srcdir)/prout_register_huge_cdb $LUN
}
prout_register_huge_cdb_cleanup()
{
	cleanup
}

atf_test_case prout_register_unregister cleanup
prout_register_unregister_head()
{
	atf_set "descr" "use PERSISTENT RESERVATION OUT with the REGISTER service action to remove a prior registration"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist ctladm
}
prout_register_unregister_body()
{
	create_ramdisk
	# First register a key
	atf_check sg_persist -n --out --param-rk=0 --param-sark=$RESERVATION_KEY -G /dev/$dev
	# Then unregister it
	atf_check sg_persist -n --out --param-sark=0 --param-rk=$RESERVATION_KEY -G /dev/$dev
	# Finally, check that no keys are registered
	atf_check -o match:"there are NO registered reservation keys" sg_persist -nk /dev/$dev
}
prout_register_unregister_cleanup()
{
	cleanup
}

atf_test_case prout_release cleanup
prout_release_head()
{
	atf_set "descr" "PERSISTENT RESERVATION OUT with the RESERVE service action"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist ctladm
}
prout_release_body()
{
	create_ramdisk

	# First register a key
	atf_check sg_persist -n --out --param-rk=0 --param-sark=$RESERVATION_KEY -G /dev/$dev

	# Then make a reservation using that key
	atf_check sg_persist -n --out --param-rk=$RESERVATION_KEY --reserve --prout-type=8 /dev/$dev
	atf_check sg_persist -n --out --param-rk=$RESERVATION_KEY --prout-type=8 --release /dev/$dev

	# Now check that the reservation is released
	atf_check -o match:"there is NO reservation held" sg_persist -nr /dev/$dev
	# But the registration shouldn't be.
	atf_check -o match:$RESERVATION_KEY sg_persist -nk /dev/$dev
}
prout_release_cleanup()
{
	cleanup
}


atf_test_case prout_reserve cleanup
prout_reserve_head()
{
	atf_set "descr" "PERSISTENT RESERVATION OUT with the RESERVE service action"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist ctladm
}
prout_reserve_body()
{
	create_ramdisk
	# First register a key
	atf_check sg_persist -n --out --param-rk=0 --param-sark=$RESERVATION_KEY -G /dev/$dev
	# Then make a reservation using that key
	atf_check sg_persist -n --out --param-rk=$RESERVATION_KEY --reserve --prout-type=8 /dev/$dev
	# Finally, check that the reservation is correct
	cat > expected <<HERE
  PR generation=0x1
    Key=0xdeadbeef1a7ebabe
      All target ports bit clear
      Relative port address: 0x0
      << Reservation holder >>
      scope: LU_SCOPE,  type: Exclusive Access, all registrants
      Transport Id of initiator:
        Parallel SCSI initiator SCSI address: 0x1
        relative port number (of corresponding target): 0x0
HERE
	atf_check -o file:expected sg_persist -ns /dev/$dev
}
prout_reserve_cleanup()
{
	cleanup
}

atf_test_case prout_reserve_bad_scope cleanup
prout_reserve_bad_scope_head()
{
	atf_set "descr" "PERSISTENT RESERVATION OUT will be rejected with an unknown scope field"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_persist camcontrol ctladm
}
prout_reserve_bad_scope_body()
{
	create_ramdisk
	# First register a key
	atf_check sg_persist -n --out --param-rk=0 --param-sark=$RESERVATION_KEY -G /dev/$dev

	# Then make a reservation using that key
	atf_check -s exit:1 -e match:"ILLEGAL REQUEST asc:24,0 .Invalid field in CDB." camcontrol persist $dev -o reserve -k $RESERVATION_KEY -T read_shared -s 15 -v

	# Finally, check that nothing has been reserved
	atf_check -o match:"there is NO reservation held" sg_persist -nr /dev/$dev
}
prout_reserve_bad_scope_cleanup()
{
	cleanup
}


atf_init_test_cases()
{
	atf_add_test_case prin_read_full_status_empty
	atf_add_test_case prin_read_keys_empty
	atf_add_test_case prin_read_reservation_empty
	atf_add_test_case prin_report_capabilities
	atf_add_test_case prout_clear
	atf_add_test_case prout_register
	atf_add_test_case prout_register_duplicate
	atf_add_test_case prout_register_huge_cdb
	atf_add_test_case prout_register_unregister
	atf_add_test_case prout_release
	atf_add_test_case prout_reserve
	atf_add_test_case prout_reserve_bad_scope
}
