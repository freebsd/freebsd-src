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
}

impl Status {
    fn from_raw(value: i32) -> Self {
        match value {
            0 => Self::Ok,
            1 => Self::InvalidArgument,
            2 => Self::BufferTooSmall,
            3 => Self::NotFound,
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
}
