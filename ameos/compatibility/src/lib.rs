//! Rust bindings for the AmeOS legacy-application compatibility ABI.

use std::{
    ffi::{CStr, CString, NulError},
    os::raw::c_char,
};

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Version {
    pub abi_version: u32,
    pub major: u32,
    pub minor: u32,
    pub patch: u32,
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Status {
    Ok = 0,
    InvalidArgument = 1,
    BufferTooSmall = 2,
    NotFound = 3,
    IoError = 4,
    InvalidHandle = 5,
}

impl Status {
    fn from_raw(value: i32) -> Self {
        match value {
            0 => Self::Ok,
            1 => Self::InvalidArgument,
            2 => Self::BufferTooSmall,
            3 => Self::NotFound,
            4 => Self::IoError,
            5 => Self::InvalidHandle,
            _ => Self::InvalidArgument,
        }
    }
}

#[derive(Debug)]
pub enum Error {
    Status(Status),
    InteriorNul(NulError),
    InvalidUtf8,
}

impl From<NulError> for Error {
    fn from(error: NulError) -> Self {
        Self::InteriorNul(error)
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Status(status) => write!(f, "compatibility operation failed: {status:?}"),
            Self::InteriorNul(_) => f.write_str("compatibility input contains NUL"),
            Self::InvalidUtf8 => f.write_str("compatibility value is not UTF-8"),
        }
    }
}

impl std::error::Error for Error {}

unsafe extern "C" {
    fn ame_compat_get_version(out: *mut Version) -> i32;
    fn ame_compat_getenv(
        name: *const c_char,
        out: *mut c_char,
        out_len: usize,
        required_len: *mut usize,
    ) -> i32;
    fn ame_compat_setenv(name: *const c_char, value: *const c_char, overwrite: i32) -> i32;
    fn ame_compat_memmove(destination: *mut u8, source: *const u8, length: usize) -> *mut u8;
    fn ame_compat_memset(destination: *mut u8, value: i32, length: usize) -> *mut u8;
    fn ame_compat_file_open(
        path: *const c_char,
        write: i32,
        out: *mut *mut std::ffi::c_void,
    ) -> i32;
    fn ame_compat_file_read(
        file: *mut std::ffi::c_void,
        buffer: *mut u8,
        buffer_len: usize,
        bytes_read: *mut usize,
    ) -> i32;
    fn ame_compat_file_write(
        file: *mut std::ffi::c_void,
        buffer: *const u8,
        buffer_len: usize,
        bytes_written: *mut usize,
    ) -> i32;
    fn ame_compat_file_close(file: *mut std::ffi::c_void) -> i32;
    fn ame_compat_process_open(
        path: *const c_char,
        argv: *const *const c_char,
        out: *mut *mut std::ffi::c_void,
    ) -> i32;
    fn ame_compat_process_wait(file: *mut std::ffi::c_void, exit_status: *mut i32) -> i32;
    fn ame_compat_process_close(file: *mut std::ffi::c_void) -> i32;
}

pub fn version() -> Result<Version, Error> {
    let mut version = Version {
        abi_version: 0,
        major: 0,
        minor: 0,
        patch: 0,
    };
    let status = unsafe { ame_compat_get_version(&mut version) };
    (status == Status::Ok as i32)
        .then_some(version)
        .ok_or_else(|| Error::Status(Status::from_raw(status)))
}

pub fn getenv(name: &str) -> Result<Option<String>, Error> {
    let name = CString::new(name)?;
    let mut required_len = 0;
    let status =
        unsafe { ame_compat_getenv(name.as_ptr(), std::ptr::null_mut(), 0, &mut required_len) };
    if Status::from_raw(status) == Status::NotFound {
        return Ok(None);
    }
    if !matches!(
        Status::from_raw(status),
        Status::BufferTooSmall | Status::Ok
    ) {
        return Err(Error::Status(Status::from_raw(status)));
    }

    let mut buffer = vec![0u8; required_len];
    let status = unsafe {
        ame_compat_getenv(
            name.as_ptr(),
            buffer.as_mut_ptr().cast(),
            buffer.len(),
            std::ptr::null_mut(),
        )
    };
    if status != Status::Ok as i32 {
        return Err(Error::Status(Status::from_raw(status)));
    }
    CStr::from_bytes_with_nul(&buffer)
        .map_err(|_| Error::InvalidUtf8)?
        .to_str()
        .map(str::to_owned)
        .map(Some)
        .map_err(|_| Error::InvalidUtf8)
}

pub fn setenv(name: &str, value: &str, overwrite: bool) -> Result<(), Error> {
    let name = CString::new(name)?;
    let value = CString::new(value)?;
    let status = unsafe { ame_compat_setenv(name.as_ptr(), value.as_ptr(), overwrite as i32) };
    (status == Status::Ok as i32)
        .then_some(())
        .ok_or_else(|| Error::Status(Status::from_raw(status)))
}

/// Moves bytes with overlap-safe semantics.
pub fn memmove(destination: &mut [u8], source: &[u8]) {
    assert!(destination.len() >= source.len());
    unsafe {
        ame_compat_memmove(destination.as_mut_ptr(), source.as_ptr(), source.len());
    }
}

/// Fills a byte slice using the platform implementation.
pub fn memset(destination: &mut [u8], value: u8) {
    unsafe {
        ame_compat_memset(
            destination.as_mut_ptr(),
            i32::from(value),
            destination.len(),
        );
    }
}

pub struct File {
    raw: *mut std::ffi::c_void,
}

impl File {
    pub fn open(path: &str, write: bool) -> Result<Self, Error> {
        let path = CString::new(path)?;
        let mut raw = std::ptr::null_mut();
        let status = unsafe { ame_compat_file_open(path.as_ptr(), write as i32, &mut raw) };
        if status != Status::Ok as i32 {
            return Err(Error::Status(Status::from_raw(status)));
        }
        Ok(Self { raw })
    }

    pub fn read(&mut self, buffer: &mut [u8]) -> Result<usize, Error> {
        let mut count = 0;
        let status = unsafe {
            ame_compat_file_read(self.raw, buffer.as_mut_ptr(), buffer.len(), &mut count)
        };
        if status == Status::Ok as i32 {
            Ok(count)
        } else {
            Err(Error::Status(Status::from_raw(status)))
        }
    }

    pub fn write(&mut self, buffer: &[u8]) -> Result<usize, Error> {
        let mut count = 0;
        let status =
            unsafe { ame_compat_file_write(self.raw, buffer.as_ptr(), buffer.len(), &mut count) };
        if status == Status::Ok as i32 {
            Ok(count)
        } else {
            Err(Error::Status(Status::from_raw(status)))
        }
    }
}

impl Drop for File {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe { ame_compat_file_close(self.raw) };
            self.raw = std::ptr::null_mut();
        }
    }
}

pub struct Process {
    raw: *mut std::ffi::c_void,
}

impl Process {
    pub fn spawn(path: &str, args: &[&str]) -> Result<Self, Error> {
        let path = CString::new(path)?;
        let arguments = args
            .iter()
            .map(|arg| CString::new(*arg))
            .collect::<Result<Vec<_>, _>>()?;
        let mut argv = Vec::with_capacity(arguments.len() + 2);
        argv.push(path.as_ptr());
        argv.extend(arguments.iter().map(|arg| arg.as_ptr()));
        argv.push(std::ptr::null());
        let mut raw = std::ptr::null_mut();
        let status = unsafe { ame_compat_process_open(path.as_ptr(), argv.as_ptr(), &mut raw) };
        if status != Status::Ok as i32 {
            return Err(Error::Status(Status::from_raw(status)));
        }
        Ok(Self { raw })
    }

    pub fn wait(&mut self) -> Result<i32, Error> {
        let mut status_code = 0;
        let status = unsafe { ame_compat_process_wait(self.raw, &mut status_code) };
        if status == Status::Ok as i32 {
            Ok(status_code)
        } else {
            Err(Error::Status(Status::from_raw(status)))
        }
    }
}

impl Drop for Process {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe { ame_compat_process_close(self.raw) };
            self.raw = std::ptr::null_mut();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reports_abi_version() {
        let version = version().unwrap();
        assert_eq!(version.abi_version, 1);
        assert_eq!((version.major, version.minor), (0, 1));
    }

    #[test]
    fn round_trips_environment_value() {
        setenv("AME_COMPAT_TEST", "legacy-value", true).unwrap();
        assert_eq!(
            getenv("AME_COMPAT_TEST").unwrap().as_deref(),
            Some("legacy-value")
        );
    }

    #[test]
    fn missing_environment_is_not_an_error() {
        assert_eq!(getenv("AME_COMPAT_MISSING_VALUE").unwrap(), None);
    }

    #[test]
    fn fills_bytes() {
        let mut bytes = [0u8; 4096];
        memset(&mut bytes, 0xa5);
        assert!(bytes.iter().all(|&byte| byte == 0xa5));
    }

    #[test]
    fn moves_overlapping_bytes_backward() {
        let mut bytes = *b"0123456789";
        unsafe {
            ame_compat_memmove(bytes.as_mut_ptr().add(2), bytes.as_ptr(), 8);
        }
        assert_eq!(&bytes, b"0101234567");
    }

    #[test]
    fn runs_a_process_and_reads_exit_status() {
        #[cfg(unix)]
        {
            let mut process = Process::spawn("/bin/sh", &["-c", "exit 7"]).unwrap();
            assert_eq!(process.wait().unwrap(), 7);
        }
        #[cfg(windows)]
        {
            let mut process = Process::spawn("cmd.exe", &["/C", "exit", "7"]).unwrap();
            assert_eq!(process.wait().unwrap(), 7);
        }
    }

    #[test]
    fn round_trips_a_file() {
        let path = std::env::temp_dir().join(format!("ame-compat-{}-file.bin", std::process::id()));
        let path = path.to_string_lossy().into_owned();
        {
            let mut file = File::open(&path, true).unwrap();
            assert_eq!(file.write(b"legacy-data").unwrap(), 11);
        }
        {
            let mut file = File::open(&path, false).unwrap();
            let mut buffer = [0u8; 32];
            let count = file.read(&mut buffer).unwrap();
            assert_eq!(&buffer[..count], b"legacy-data");
        }
        let _ = std::fs::remove_file(path);
    }
}
