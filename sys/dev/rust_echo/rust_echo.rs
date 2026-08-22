#![no_std]

use core::cmp::min;
use core::panic::PanicInfo;
use core::ptr;

const BUFFER_SIZE: usize = 256;

static mut BUFFER: [u8; BUFFER_SIZE] = [0; BUFFER_SIZE];
static mut LEN: usize = 0;

#[unsafe(no_mangle)]
pub extern "C" fn rust_echo_clear() {
    unsafe {
        LEN = 0;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_echo_write(src: *const u8, len: usize) -> usize {
    if src.is_null() || len == 0 {
        return 0;
    }

    unsafe {
        let available = BUFFER_SIZE.saturating_sub(LEN);
        let copy_len = min(len, available);
        if copy_len == 0 {
            return 0;
        }
        let dst = ptr::addr_of_mut!(BUFFER).cast::<u8>().add(LEN);
        ptr::copy_nonoverlapping(src, dst, copy_len);
        LEN += copy_len;
        copy_len
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_echo_read(dst: *mut u8, len: usize) -> usize {
    if dst.is_null() || len == 0 {
        return 0;
    }

    unsafe {
        let copy_len = min(len, LEN);
        if copy_len == 0 {
            return 0;
        }
        let src = ptr::addr_of!(BUFFER).cast::<u8>();
        ptr::copy_nonoverlapping(src, dst, copy_len);
        let remaining = LEN - copy_len;
        if remaining != 0 {
            let buffer = ptr::addr_of_mut!(BUFFER).cast::<u8>();
            ptr::copy(buffer.add(copy_len), buffer, remaining);
        }
        LEN = remaining;
        copy_len
    }
}

#[panic_handler]
fn panic(_info: &PanicInfo<'_>) -> ! {
    loop {
        core::hint::spin_loop();
    }
}
